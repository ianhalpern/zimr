/*   Zimr - Next Generation Web Server
 *
 *+  Copyright (c) 2009 Ian Halpern
 *@  http://Zimr.org
 *
 *   This file is part of Zimr.
 *
 *   Zimr is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Zimr is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Zimr.  If not, see <http://www.gnu.org/licenses/>
 *

TODO:
zimr start [all [states]]
zimr pause [all [states]]
zimr restart [all [states]]
zimr status [all [states], proxy]
zimr help
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>
#include <fcntl.h>

#include "general.h"
#include "zsocket.h"
#include "zfildes.h"
#include "zcnf.h"
#include "simclist.h"
#include "userdir.h"

#define START   0x1
#define PAUSE    0x2
#define RESTART 0x3
#define REMOVE  0x4
#define STATUS  0x5

#define CMDSTR(X) (X==START?"starting":X==PAUSE?"pausing":X==RESTART?"restarting":X==REMOVE?"removing":NULL)

typedef struct command {
	char* name;
	char* description;
	void* sub_commands;
	void (*func)( int optc, char* optv[ ] );
} command_t;

command_t root_command;

int commands_to_string( char* commands[ 50 ][ 2 ], char* top_cmd, command_t* command, int* n ) {
	int max_len;
	char* tmp = (char*) malloc( strlen( top_cmd ) + strlen( command->name ) + 2 );
	sprintf( tmp, "%s %s", top_cmd, command->name );
	top_cmd = tmp;

	if ( !command->func ) {
		(*n)--;
	}

	else {
		commands[ *n ][ 0 ] = top_cmd;
		commands[ *n ][ 1 ] = command->description;
	}

	max_len = strlen( top_cmd );
	if ( command->sub_commands ) {
		int i = 0, tmp_max_len;
		command_t* tmp;
		while ( ( tmp = command->sub_commands + i )->name ) {
			(*n)++;
			tmp_max_len = commands_to_string( commands, top_cmd, tmp, n );
			if ( tmp_max_len > max_len ) max_len = tmp_max_len;
			i += sizeof( command_t );
		}
	}

	if ( !command->func )
		free( top_cmd );

	return max_len;
}

void print_usage() {
	printf( "Zimr " ZIMR_VERSION " (" BUILD_DATE ") - " ZIMR_WEBSITE "\n\n" );
	printf( "Commands:\n" );
	int n = 0;
	char* commands[ 50 ][ 2 ];
	int len = commands_to_string( commands, "", &root_command, &n );

	int i;
	for ( i = 0; i <= n; i++ ) {
		char buf[ 16 ];
		sprintf( buf, " %%-%ds %%s\n", len );
		printf( buf, commands[ i ][ 0 ], commands[ i ][ 1 ] );
		free( commands[ i ][ 0 ] );
	}
}

bool run_commands( command_t* command, int argc, char* argv[ ] ) {
	if ( strcmp( command->name, argv[ 0 ] ) == 0 ) {
		if ( command->sub_commands && argc - 1 ) {
			int i = 0;
			command_t* tmp;
			while ( ( tmp = command->sub_commands + i )->name ) {
				if ( run_commands( tmp, argc - 1, argv + 1 ) )
					return true;
				i += sizeof( command_t );
			}
		}

		if ( command->func && argc ) {
			command->func( argc - 1, argv + 1 );
			return true;
		}
	}

	return false;
}

// Functions ////////////////////////////

pid_t application_exec( uid_t uid, char* path, list_t* args, bool nofork ) {
	pid_t pid = !nofork ? fork() : 0;
	char dir[ PATH_MAX ],* ptr;

	if ( pid == 0 ) {
		// in the child, do exec
		ptr = strrchr( path, '/' );
		strncpy( dir, path, ptr - path );
		dir[ ptr - path ] = 0;
		chdir( dir );
		char* argv[ list_size( args ) + 3 ];
		argv[ 0 ] = ZM_APP_EXEC;
		argv[ 1 ] = path;
		argv[ list_size( args ) + 2 ] = NULL;

		int i;
		for ( i = 0; i < list_size( args ); i++ )
			argv[ i ] = list_get_at( args, i + 2 );

		setuid( uid );
		setgid( uid );

		char buf[ 256 ];
		expand_tilde( ZM_USR_MODULE_DIR, buf, sizeof( buf ), getuid() );
		setenv( "LD_LIBRARY_PATH", buf, 1 );

		execvp( argv[ 0 ], argv );
		puts( "application_exec(): execvp() error" );
	} else if ( pid == (pid_t) -1 ) {
		// still in parent, but there is no child
		puts( "parent error: no child" );
	}

	return pid;
}

bool application_kill( pid_t pid ) {
	if ( !pid ) return true;
	if ( !stopproc( pid ) && errno != ESRCH ) {
		printf( "failed: %s\n", strerror( errno ) );
		return false;
	}
	return true;
}

bool application_function( uid_t uid, char* cnf_path, list_t* args, char type ) {
	if ( CMDSTR( type ) )
		printf( " * %s %s...", CMDSTR( type ), cnf_path ); fflush( stdout );

	userdir_init( uid );
	zcnf_state_t* state = zcnf_state_load( uid );

	if ( !state ) {
		puts( "failed to load state" );
		return false;
	}

	bool retval = true;

	zcnf_state_app_t* app = NULL;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		if ( strcmp( app->path, cnf_path ) == 0 ) {
			if ( type == START && app->pid && kill( app->pid, 0 ) == 0 ) {
				puts( "application is already running" );
				retval = false;
				goto quit;
			}
			break;
		}
		app = NULL;
	}

	if ( type != START && !app ) {
		puts( "application not in state" );
		retval = false;
		goto quit;
	}

	else {
		pid_t pid;
		switch ( type ) {
			case START:
				if ( ( pid = application_exec( state->uid, cnf_path, args, false ) ) == -1 ) {
					retval = false;
					printf( "failed\n" );
					goto quit;
				}
				zcnf_state_set_app( state, cnf_path, pid, args );
				break;
			case PAUSE:
				if ( !application_kill( app->pid ) ) {
					retval = false;
					printf( "failed\n" );
					goto quit;
				}
				zcnf_state_set_app( state, app->path, 0, NULL );
				break;
			case RESTART:
				if ( application_kill( app->pid )
				  && ( pid = application_exec( state->uid, app->path, &app->args, false ) ) == -1 ) {
					retval = false;
					printf( "failed\n" );
					goto quit;
				}
				zcnf_state_set_app( state, app->path, pid, NULL );
				break;
			case REMOVE:
				if ( !application_kill( app->pid ) ) {
					retval = false;
					printf( "failed\n" );
					goto quit;
				}
				list_delete_at( &state->apps, i );
				zcnf_state_app_free( app );
				break;
			case STATUS:
				printf( "%d: %s ", i, app->path );

				if ( !app->pid || kill( app->pid, 0 ) == -1 ) // process still running, not dead
					printf( "(*Paused)" );
				else printf( "%d", app->pid );
				// TODO: proxy address, uptime, memory usage, running websites, etc

				printf( "\n" );
				break;
		}
	}

	if ( CMDSTR( type ) )
		printf( "success.\n" );
	zcnf_state_save( state );

quit:
	zcnf_state_free( state );
	return retval;
}

bool state_function( uid_t uid, char type ) {
	zcnf_state_t* state = zcnf_state_load( uid );
	if ( !state ) {
		puts( "failed to load state" );
		return false;
	}

	bool retval = true;

	zcnf_state_app_t* app;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		if ( !application_function( uid, app->path, &app->args, type ) ) {
			retval = false;
			goto quit;
		}
	}

quit:
	zcnf_state_free( state );
	return retval;
}

bool state_all_function( char type ) {
	/* yea...this whole bit is a hack */
	char line[ 64 ];
	FILE* fp;
	fp = popen( "awk -F\":\" '{ print $3 }' /etc/passwd", "r" );
	while ( fgets( line, sizeof( line ), fp ) ) {
		uid_t uid = atoi( line );

		char filepath[ 128 ];
		if ( !expand_tilde( ZM_USR_STATE_FILE, filepath, sizeof( filepath ), uid ) )
			continue;

		int fd;
		if ( ( fd = open( filepath, O_RDONLY ) ) == -1 ) continue;
		close( fd );

		printf( "%d:%s\n", uid, filepath );

		state_function( uid, type );
	}
	pclose(fp);
	exit( EXIT_SUCCESS );
}

///////////////////////////////////////////////////////

void application_start_cmd( int optc, char* optv[] ) {
	char cnf_path[ PATH_MAX ] = "";
	getcwd( cnf_path, sizeof( cnf_path ) );
	strcat( cnf_path, "/" ZM_APP_CNF_FILE );

	list_t args;
	list_init( &args );

	bool nostate = false;
	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( strcmp( optv[ i ], "-a" ) == 0 ) {
			while ( ++i < optc ) {
				if ( strcmp( optv[ i ], "-no-state" ) == 0 ) {
					i--;
					break;
				}
				list_append( &args, optv[ i ] );
			}
		} else if ( strcmp( optv[ i ], "-no-state" ) == 0 ) {
			nostate = true;
		} else if ( !i ) {
			realpath( optv[ i ], cnf_path );
		} else {
			print_usage();
			return;
		}
	}

	if ( nostate )
		application_exec( getuid(), cnf_path, &args, true );

	else if ( !application_function( getuid(), cnf_path, &args, START ) )
		exit( EXIT_FAILURE );

	list_destroy( &args );
}

void application_stop_cmd( int optc, char* optv[ ] ) {
	char cnf_path[ PATH_MAX ] = "";
	getcwd( cnf_path, sizeof( cnf_path ) );
	strcat( cnf_path, "/" ZM_APP_CNF_FILE );

	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( !i )
			realpath( optv[ i ], cnf_path );
		else {
			print_usage();
			return;
		}
	}

	if ( !application_function( getuid(), cnf_path, NULL, PAUSE ) )
		exit( EXIT_FAILURE );
}

void application_restart_cmd( int optc, char* optv[ ] ) {
	char cnf_path[ PATH_MAX ] = "";
	getcwd( cnf_path, sizeof( cnf_path ) );
	strcat( cnf_path, "/" ZM_APP_CNF_FILE );

	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( !i )
			realpath( optv[ i ], cnf_path );
		else {
			print_usage();
			return;
		}
	}

	if ( !application_function( getuid(), cnf_path, NULL, RESTART ) )
		exit( EXIT_FAILURE );
}

void application_remove_cmd( int optc, char* optv[ ] ) {
	char cnf_path[ PATH_MAX ] = "";
	getcwd( cnf_path, sizeof( cnf_path ) );
	strcat( cnf_path, "/" ZM_APP_CNF_FILE );

	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( !i )
			realpath( optv[ i ], cnf_path );
		else {
			print_usage();
			return;
		}
	}

	if ( !application_function( getuid(), cnf_path, NULL, REMOVE ) )
		exit( EXIT_FAILURE );
}

void application_status_cmd( int optc, char* optv[ ] ) {
	char cnf_path[ PATH_MAX ] = "";
	getcwd( cnf_path, sizeof( cnf_path ) );
	strcat( cnf_path, "/" ZM_APP_CNF_FILE );

	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( !i )
			realpath( optv[ i ], cnf_path );
		else {
			print_usage();
			return;
		}
	}

	if ( !application_function( getuid(), cnf_path, NULL, STATUS ) )
		exit( EXIT_FAILURE );
}

void state_start_cmd( int optc, char* optv[ ] ) {
	if ( !state_function( getuid(), START ) )
		exit( EXIT_FAILURE );
}

void state_pause_cmd( int optc, char* optv[ ] ) {
	if ( !state_function( getuid(), PAUSE ) )
		exit( EXIT_FAILURE );
}

void state_restart_cmd( int optc, char* optv[ ] ) {
	if ( !state_function( getuid(), RESTART ) )
		exit( EXIT_FAILURE );
}

void state_remove_cmd( int optc, char* optv[ ] ) {
	if ( !state_function( getuid(), REMOVE ) )
		exit( EXIT_FAILURE );
}

void state_status_cmd( int optc, char* optv[ ] ) {
	if ( !state_function( getuid(), STATUS ) )
		exit( EXIT_FAILURE );
}

void state_start_all_cmd( int optc, char* optv[ ] ) {
	if ( !state_all_function( START ) )
		exit( EXIT_FAILURE );
}

void state_pause_all_cmd( int optc, char* optv[ ] ) {
	if ( !state_all_function( PAUSE ) )
		exit( EXIT_FAILURE );
}

void state_restart_all_cmd( int optc, char* optv[ ] ) {
	if ( !state_all_function( RESTART ) )
		exit( EXIT_FAILURE );
}

void state_remove_all_cmd( int optc, char* optv[ ] ) {
	if ( !state_all_function( REMOVE ) )
		exit( EXIT_FAILURE );
}

void state_status_all_cmd( int optc, char* optv[ ] ) {
	if ( !state_all_function( STATUS ) )
		exit( EXIT_FAILURE );
}

void help_cmd( int optc, char* optv[ ] ) {
	print_usage();
}

// Proxy Functions ////////////////////////////

/*void proxy_status() {
	int command = ZM_CMD_STATUS;
	zsocket_init();
	zsocket_t* proxy = zsocket_connect( inet_addr( ZM_PROXY_ADDR ), ZM_PROXY_PORT );

	if ( !proxy ) {
		printf( "could not connect to proxy.\n" );
		return;
	}

	ztransport_t* transport;

	transport = ztransport_open( proxy->sockfd, ZT_WO, command );
	ztransport_close( transport );

	transport  = ztransport_open( proxy->sockfd, ZT_RO, 0 );
	ztransport_read( transport );
	ztransport_close( transport );

	puts( transport->message );

	zsocket_close( proxy );
}

void proxy_status_cmd( int optc, char* optv[ ] ) {
	proxy_status();
}*/

///////////////////////////////////////////////


int main( int argc, char* argv[ ] ) {

	command_t start_all_commands[ ] = {
		{ "states", "Start all zimr applications for all user's zimr states.\n", NULL, &state_start_all_cmd },
		{ NULL }
	};

	command_t start_commands[ ] = {
		{ "all", "Start all zimr applications in the current user's state.", &start_all_commands, &state_start_cmd },
		{ NULL }
	};

	command_t pause_all_commands[ ] = {
		{ "states", "Stop all zimr applications for all user's zimr states.\n", NULL, &state_pause_all_cmd },
		{ NULL }
	};

	command_t pause_commands[ ] = {
		{ "all", "Stop all zimr applications for the current user's state.", &pause_all_commands, &state_pause_cmd },
		{ NULL }
	};

	command_t restart_all_commands[ ] = {
		{ "states", "Restart all zimr applications for all user's zimr states.\n", NULL, &state_restart_all_cmd },
		{ NULL }
	};

	command_t restart_commands[ ] = {
		{ "all", "Restart all zimr applications for the current user's state.", &restart_all_commands, &state_restart_cmd },
		{ NULL }
	};

	command_t remove_all_commands[ ] = {
		{ "states", "Remove all zimr applications for all user's zimr states.\n", NULL, &state_remove_all_cmd },
		{ NULL }
	};

	command_t remove_commands[ ] = {
		{ "all", "Remove all zimr applications for the current user's state.", &remove_all_commands, &state_remove_cmd },
		{ NULL }
	};

	command_t status_all_commands[ ] = {
		{ "states", "Report the status of all zimr applications for all user's zimr states.\n", NULL, &state_status_all_cmd },
		{ NULL }
	};

	command_t status_commands[ ] = {
		{ "all", "Report the status of all zimr applications for the current user's state.", &status_all_commands, &state_status_cmd },
		{ NULL }
	};

	command_t zimr_commands[ ] = {
		{ "start",   "Start a zimr application. [ <config path>, -a [...], -no-state ]", &start_commands, &application_start_cmd },
		{ "pause",   "Stop a zimr application. [ <config path> ]", &pause_commands, &application_stop_cmd },
		{ "restart", "Restart a zimr application. [ <config path> ]", &restart_commands, &application_restart_cmd },
		{ "remove",  "Remove a zimr application. [ <config path> ]", &remove_commands, &application_remove_cmd },
		{ "status",  "Report the status of a zimr application. [ <config path> ]", &status_commands, &application_status_cmd },
		{ "help",    "List all commands.", NULL, &help_cmd },
		{ NULL }
	};

	root_command.name = "zimr";
	root_command.description = "";
	root_command.sub_commands = zimr_commands;
	root_command.func = NULL;

	char* last_slash = strrchr( argv[ 0 ], '/' );
	if ( last_slash ) argv[ 0 ] = last_slash + 1;

	if ( !run_commands( &root_command, argc, argv ) )
		print_usage();

	return EXIT_SUCCESS;
}
