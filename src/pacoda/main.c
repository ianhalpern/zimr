/*   Pacoda - Next Generation Web Server
 *
 *+  Copyright (c) 2009 Ian Halpern
 *@  http://Pacoda.org
 *
 *   This file is part of Pacoda.
 *
 *   Pacoda is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Pacoda is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Pacoda.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>

#include "general.h"
#include "ptransport.h"
#include "psocket.h"
#include "pfildes.h"
#include "pcnf.h"
#include "simclist.h"

typedef struct command {
	char* name;
	char* description;
	void* sub_commands;
	void (*func)( int optc, char* optv[ ] );
} command_t;

command_t root_command;

void print_command( command_t* command, int depth ) {
	char padding[ depth * 5 + 3 ];
	memset( padding, ' ', sizeof( padding ) - 1 );
	padding[ sizeof( padding ) - 1 ] = 0;

	char spacing[ 25 - ( strlen( padding ) + strlen( command->name ) ) ];
	memset( spacing, ' ', sizeof( spacing ) - 1 );
	spacing[ sizeof( spacing ) - 1 ] = 0;

	printf( "%s%s%s%s\n", padding, command->name, spacing, command->description );

	if ( command->sub_commands ) {
		int i = 0;
		command_t* tmp;
		while ( ( tmp = command->sub_commands + i )->name ) {
			print_command( tmp, depth + 1 );
			i += sizeof( command_t );
		}
	}
}

void print_usage( ) {
	printf( "Pacoda " PACODA_VERSION " (" BUILD_DATE ") - " PACODA_WEBSITE "\n\n" );
	printf( "Commands:\n" );
	print_command( &root_command, 0 );
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

// Application Functions ////////////////////////////

pid_t application_exec( uid_t uid, char* exec, char* dir, list_t* args ) {
	pid_t pid = fork( );

	if ( pid == 0 ) {
		// in the child, do exec
		chdir( dir );
		char* argv[ list_size( args ) + 2 ];
		argv[ 0 ] = exec;
		argv[ list_size( args ) + 1 ] = NULL;

		int i;
		for ( i = 1; i < list_size( args ) + 1; i++ ) {
			argv[ i ] = list_get_at( args, i - 1 );
		}

		setuid( uid );
		setgid( uid );

		execvp( argv[ 0 ], argv );
		puts( "error" );
	} else if ( pid == (pid_t) -1 ) {
		// still in parent, but there is no child
		puts( "parent error: no child" );
	}

	return pid;
}

bool application_kill( pid_t pid ) {
	if ( !stopproc( pid ) && errno != ESRCH ) {
		printf( "failed: %s\n", strerror( errno ) );
		return false;
	}
	return true;
}

bool application_start( char* exec, char* dir, list_t* args ) {
	printf( " * starting %s@%s ...", exec, dir ); fflush( stdout );
	pcnf_state_t* state = pcnf_state_load( getuid( ) );
	if ( !state ) {
		puts( "failed to load state" );
		return false;
	}

	bool retval = true;

	pcnf_state_app_t* app;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		if ( strcmp( app->dir, dir ) == 0 && strcmp( app->exec, exec ) == 0 ) {
			pcnf_state_free( state );
			puts( "application is already running" );
			retval = false;
			goto quit;
		}
	}

	pid_t pid = application_exec( state->uid, exec, dir, args );
	if ( pid == -1 ) {
		pcnf_state_free( state );
		retval = false;
		printf( "failed\n" );
		goto quit;
	}

	printf( "success.\n" );
	pcnf_state_set_app( state, exec, dir, pid, args );
	pcnf_state_save( state );

quit:
	pcnf_state_free( state );
	return retval;
}

bool application_stop( char* exec, char* dir ) {
	printf( " * stopping %s@%s...", exec, dir ); fflush( stdout );
	pcnf_state_t* state = pcnf_state_load( getuid( ) );
	if ( !state ) {
		puts( "failed to load state" );
		return false;
	}

	bool retval = true;

	pcnf_state_app_t* app = NULL;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		if ( strcmp( app->dir, dir ) == 0 && strcmp( app->exec, exec ) == 0 ) {
			break;
		}
		app = NULL;
	}

	if ( !app ) {
		puts( "application not in state" );
		retval = false;
		goto quit;
	} else {
		if ( application_kill( app->pid ) ) {
			printf( "success.\n" );
			list_delete_at( &state->apps, i );
			pcnf_state_app_free( app );
			pcnf_state_save( state );
		} else retval = false;
	}

quit:
	pcnf_state_free( state );
	return retval;
}

bool application_restart( uid_t uid, char* exec, char* dir ) {
	printf( " * restarting %s@%s...", exec, dir ); fflush( stdout );
	pcnf_state_t* state = pcnf_state_load( uid );
	if ( !state ) {
		puts( "failed to load state" );
		return false;
	}

	bool retval = true;

	pcnf_state_app_t* app = NULL;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		if ( strcmp( app->dir, dir ) == 0 && strcmp( app->exec, exec ) == 0 ) {
			break;
		}
		app = NULL;
	}

	if ( !app ) {
		puts( "application not in state" );
		retval = false;
	} else {
		if ( application_kill( app->pid ) ) {
			pid_t pid = application_exec( state->uid, app->exec, app->dir, &app->args );
			if ( pid == -1 ) {
				retval = false;
				goto quit;
			}
			pcnf_state_set_app( state, app->exec, app->dir, pid, NULL );
			pcnf_state_save( state );
			printf( "success.\n" );
		} else retval = false;
	}

quit:
	pcnf_state_free( state );
	return retval;
}

bool application_status( char* exec, char* dir ) {
	pcnf_state_t* state = pcnf_state_load( getuid( ) );
	if ( !state ) {
		puts( "failed to load state" );
		return false;
	}

	bool retval = true;

	pcnf_state_app_t* app;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		if ( strcmp( app->dir, dir ) == 0 && strcmp( app->exec, exec ) == 0 ) {
			printf( "%d: %s ", i, app->exec );

			if ( kill( app->pid, 0 ) == -1 ) // process still running, not dead
				printf( "(*not running)" );
			else printf( "%d", app->pid );

			printf( "\n   directory: %s\n", app->dir );
			// TODO: proxy address, uptime, memory usage, running websites, etc
			printf( "\n" );
			break;
		}
	}

	if ( i == list_size( &state->apps ) ) {
		puts( "application is not in state." );
		retval = false;
	}

	pcnf_state_free( state );
	return retval;
}

void application_start_cmd( int optc, char* optv[ ] ) {
	char* exec = "pacoda-application";
	char dir[ PATH_MAX ] = "";
	list_t args;

	getcwd( dir, sizeof( dir ) );
	list_init( &args );

	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( optv[ i ][ 0 ] != '-' ) {
			exec = optv[ i ];
		} else if ( strcmp( optv[ i ], "-dir" ) == 0 && i + 1 < optc ) {
			realpath( optv[ i + 1 ], dir );
			i++;
		} else if ( strcmp( optv[ i ], "-a" ) == 0 ) {
			while ( ++i < optc ) {
				if ( strcmp( optv[ i ], "-dir" ) == 0 ) {
					i--;
					break;
				}
				list_append( &args, optv[ i ] );
			}
		} else {
			print_usage( );
			return;
		}
	}

	if ( !application_start( exec, dir, &args ) )
		exit( EXIT_FAILURE );
	list_destroy( &args );
}

void application_stop_cmd( int optc, char* optv[ ] ) {
	char* exec = "pacoda-application";
	char dir[ PATH_MAX ] = "";
	getcwd( dir, sizeof( dir ) );

	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( optv[ i ][ 0 ] != '-' ) {
			exec = optv[ i ];
		} else if ( strcmp( optv[ i ], "-dir" ) == 0 && i + 1 < optc ) {
			realpath( optv[ i + 1 ], dir );
			i++;
		} else {
			print_usage( );
			return;
		}
	}

	if ( !application_stop( exec, dir ) )
		exit( EXIT_FAILURE );
}

void application_restart_cmd( int optc, char* optv[ ] ) {
	char* exec = "pacoda-application";
	char dir[ PATH_MAX ] = "";
	getcwd( dir, sizeof( dir ) );

	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( optv[ i ][ 0 ] != '-' ) {
			exec = optv[ i ];
		} else if ( strcmp( optv[ i ], "-dir" ) == 0 && i + 1 < optc ) {
			realpath( optv[ i + 1 ], dir );
			i++;
		} else {
			print_usage( );
			return;
		}
	}

	if ( !application_restart( getuid( ), exec, dir ) )
		exit( EXIT_FAILURE );
}

void application_status_cmd( int optc, char* optv[ ] ) {
	char* exec = "pacoda-application";
	char dir[ PATH_MAX ] = "";
	getcwd( dir, sizeof( dir ) );

	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( optv[ i ][ 0 ] != '-' ) {
			exec = optv[ i ];
		} else if ( strcmp( optv[ i ], "-dir" ) == 0 && i + 1 < optc ) {
			realpath( optv[ i + 1 ], dir );
			i++;
		} else {
			print_usage( );
			return;
		}
	}

	if ( !application_status( exec, dir ) )
		exit( EXIT_FAILURE );
}

///////////////////////////////////////////////


// State Functions ////////////////////////////

bool state_unload( uid_t uid ) {
	pcnf_state_t* state = pcnf_state_load( uid );

	if ( !state ) {
		puts( "failed to load state" );
		return false;
	}

	bool retval = true;

	pcnf_state_app_t* app;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		printf( " * stopping %s@%s...", app->exec, app->dir ); fflush( stdout );
		if ( !application_kill( app->pid ) ) {
			retval = false;
			goto quit;
		}
	}

	printf( "success.\n" );
quit:
	pcnf_state_free( state );
	return retval;
}

bool state_clear( ) {
	pcnf_state_t* state = pcnf_state_load( getuid( ) );
	if ( !state ) {
		puts( "failed to load state" );
		return false;
	}

	bool retval = true;

	pcnf_state_app_t* app;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		if ( !application_stop( app->exec, app->dir ) ) {
			retval = false;
			goto quit;
		}
	}

quit:
	pcnf_state_free( state );
	return retval;
}

bool state_reload( uid_t uid ) {
	pcnf_state_t* state = pcnf_state_load( uid );
	if ( !state ) {
		puts( "failed to load state" );
		return false;
	}

	bool retval = true;

	pcnf_state_app_t* app;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		if ( !application_restart( uid, app->exec, app->dir ) ) {
			retval = false;
			goto quit;
		}
	}

quit:
	pcnf_state_free( state );
	return retval;
}

bool state_status( ) {
	pcnf_state_t* state = pcnf_state_load( getuid( ) );
	if ( !state ) {
		puts( "failed to load state" );
		return false;
	}

	bool retval = true;

	pcnf_state_app_t* app;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		if ( !application_status( app->exec, app->dir ) ) {
			retval = false;
			goto quit;
		}
	}

quit:
	pcnf_state_free( state );
	return retval;
}

void state_reload_cmd( int optc, char* optv[ ] ) {
	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( strcmp( optv[ i ], "all" ) == 0 ) {
			/* yea...this whole bit is a hack */
			char line[ 64 ];
			FILE* fp;
			fp = popen( "awk -F\":\" '{ print $3 }' /etc/passwd", "r" );
			while ( fgets( line, sizeof( line ), fp ) ) {
				uid_t uid = atoi( line );

				char filepath[ 128 ];
				if ( !expand_tilde( PD_USR_STATE_FILE, filepath, sizeof( filepath ), uid ) )
					continue;

				int fd;
				if ( ( fd = open( filepath, O_RDONLY ) ) == -1 ) continue;
				close( fd );

				printf( "%d:%s\n", uid, filepath );

				state_reload( uid );
			}
			pclose(fp);
			exit( EXIT_SUCCESS );
		}
	}

	if ( !state_reload( getuid( ) ) ) exit( EXIT_FAILURE );
}

void state_unload_cmd( int optc, char* optv[ ] ) {
	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( strcmp( optv[ i ], "all" ) == 0 ) {
			/* yea...this whole bit is a hack */
			char line[ 64 ];
			FILE* fp;
			fp = popen( "awk -F\":\" '{ print $3 }' /etc/passwd", "r" );
			while ( fgets( line, sizeof( line ), fp ) ) {
				uid_t uid = atoi( line );

				char filepath[ 128 ];
				if ( !expand_tilde( PD_USR_STATE_FILE, filepath, sizeof( filepath ), uid ) )
					continue;

				int fd;
				if ( ( fd = open( filepath, O_RDONLY ) ) == -1 ) continue;
				close( fd );

				printf( "%d:%s\n", uid, filepath );

				state_unload( uid );
			}
			pclose(fp);
			exit( EXIT_SUCCESS );
		}
	}

	if ( !state_unload( getuid( ) ) ) exit( EXIT_FAILURE );
}

void state_clear_cmd( int optc, char* optv[ ] ) {
	if ( !state_clear( ) ) exit( EXIT_FAILURE );
}

void state_status_cmd( int optc, char* optv[ ] ) {
	if ( !state_status( ) ) exit( EXIT_FAILURE );
}

///////////////////////////////////////////////


// Proxy Functions ////////////////////////////

void proxy_status( ) {
	int command = PD_CMD_STATUS;
	psocket_t* proxy = psocket_connect( inet_addr( PD_PROXY_ADDR ), PD_PROXY_PORT );

	if ( !proxy ) {
		printf( "could not connect to proxy.\n" );
		return;
	}

	ptransport_t* transport;

	transport = ptransport_open( proxy->sockfd, PT_WO, command );
	ptransport_close( transport );

	transport  = ptransport_open( proxy->sockfd, PT_RO, 0 );
	ptransport_read( transport );
	ptransport_close( transport );

	puts( transport->message );

	psocket_close( proxy );
}

void proxy_status_cmd( int optc, char* optv[ ] ) {
	proxy_status( );
}

///////////////////////////////////////////////



int main( int argc, char* argv[ ] ) {
	command_t app_commands[ ] = {
		{ "start",   "options: <exec> [ -dir <path>, -a [...] ]", NULL, &application_start_cmd },
		{ "stop",    "options: <exec> [ -dir <path>, -n ]", NULL, &application_stop_cmd },
		{ "restart", "options: <exec> [ -dir <path>, -n ]", NULL, &application_restart_cmd },
		{ "status",  "options: <exec> [ -dir <path>, -n ]", NULL, &application_status_cmd },
		{ NULL }
	};

	command_t state_commands[ ] = {
		{ "reload", "options: [all, <config path>]", NULL, &state_reload_cmd },
		{ "unload", "options: [all, <config path>]", NULL, &state_unload_cmd },
		{ "clear",  "options: [<config path>]", NULL, &state_clear_cmd },
		{ "status", "options: [<config path>]", NULL, &state_status_cmd },
		{ NULL }
	};

	command_t proxy_commands[ ] = {
		{ "status", "options: <host>[:<port>]", NULL, &proxy_status_cmd },
		{ NULL }
	};

	command_t pacoda_commands[ ] = {
		{ "application", "", &app_commands, NULL },
		{ "state", "", &state_commands, NULL },
		{ "proxy", "", &proxy_commands, NULL },
		{ NULL }
	};

	root_command.name = "pacoda";
	root_command.description = "";
	root_command.sub_commands = pacoda_commands;
	root_command.func = NULL;

	char* last_slash = strrchr( argv[ 0 ], '/' );
	if ( last_slash ) argv[ 0 ] = last_slash + 1;

	if ( !run_commands( &root_command, argc, argv ) )
		print_usage( );

	return EXIT_SUCCESS;
}
