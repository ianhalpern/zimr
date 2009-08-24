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

pid_t application_exec( char* exec, char* dir, list_t* args ) {
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

void application_start( char* exec, char* dir, list_t* args ) {
	pcnf_state_t* state = pcnf_state_load( );
	if ( !state ) {
		puts( "failed to load state" );
		exit( EXIT_FAILURE );
	}

	pcnf_state_app_t* app;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		if ( strcmp( app->dir, dir ) == 0 && strcmp( app->exec, exec ) == 0 ) {
			pcnf_state_free( state );
			puts( "application is already running" );
			return;
		}
	}

	pid_t pid = application_exec( exec, dir, args );
	pcnf_state_set_app( state, exec, dir, pid, args );
	pcnf_state_save( state );
	pcnf_state_free( state );
}

void application_stop( char* exec, char* dir ) {
	pcnf_state_t* state = pcnf_state_load( );

	if ( !state ) {
		puts( "failed to load state" );
		exit( EXIT_FAILURE );
	}

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
	} else {
		if ( application_kill( app->pid ) ) {
			list_delete_at( &state->apps, i );
			pcnf_state_app_free( app );
			pcnf_state_save( state );
		}
	}

	pcnf_state_free( state );
}

void application_restart( char* exec, char* dir ) {
	pcnf_state_t* state = pcnf_state_load( );

	if ( !state ) {
		puts( "failed to load state" );
		exit( EXIT_FAILURE );
	}

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
	} else {
		if ( application_kill( app->pid ) ) {
			pid_t pid = application_exec( app->exec, app->dir, &app->args );
			pcnf_state_set_app( state, app->exec, app->dir, pid, NULL );
		}
	}

	pcnf_state_save( state );
	pcnf_state_free( state );
}

void application_status( char* exec, char* dir ) {
	pcnf_state_t* state = pcnf_state_load( );
	if ( !state ) {
		puts( "failed to load state" );
		exit( EXIT_FAILURE );
	}

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
			break;
		}
	}

	if ( i == list_size( &state->apps ) )
		puts( "application is not in state." );

	pcnf_state_free( state );
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

	application_start( exec, dir, &args );
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

	application_stop( exec, dir );
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

	application_restart( exec, dir );
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

	application_status( exec, dir );
}

///////////////////////////////////////////////


// State Functions ////////////////////////////

void state_unload( ) {
	pcnf_state_t* state = pcnf_state_load( );

	if ( !state ) {
		puts( "failed to load state" );
		exit( EXIT_FAILURE );
	}

	pcnf_state_app_t* app;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		application_kill( app->pid );
	}

	pcnf_state_free( state );
}

void state_clear( ) {
	pcnf_state_t* state = pcnf_state_load( );

	if ( !state ) {
		puts( "failed to load state" );
		exit( EXIT_FAILURE );
	}

	pcnf_state_app_t* app;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		application_stop( app->exec, app->dir );
	}

	pcnf_state_free( state );
}

void state_reload( ) {
	pcnf_state_t* state = pcnf_state_load( );

	if ( !state ) {
		puts( "failed to load state" );
		exit( EXIT_FAILURE );
	}

	pcnf_state_app_t* app;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		application_restart( app->exec, app->dir );
	}

	pcnf_state_free( state );
}

void state_status( ) {
	pcnf_state_t* state = pcnf_state_load( );

	if ( !state ) {
		puts( "failed to load state" );
		exit( EXIT_FAILURE );
	}

	pcnf_state_app_t* app;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		application_status( app->exec, app->dir );
	}

	pcnf_state_free( state );
}

void state_reload_cmd( int optc, char* optv[ ] ) {
	state_reload( );
}

void state_unload_cmd( int optc, char* optv[ ] ) {
	state_unload( );
}

void state_clear_cmd( int optc, char* optv[ ] ) {
	state_clear( );
}

void state_status_cmd( int optc, char* optv[ ] ) {
	state_status( );
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
		{ "reload", "options: <config path>", NULL, &state_reload_cmd },
		{ "unload", "options: <config path>", NULL, &state_unload_cmd },
		{ "clear",  "options: <config path>", NULL, &state_clear_cmd },
		{ "status", "options: <config path>", NULL, &state_status_cmd },
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

	printf( "Pacoda " PACODA_VERSION " (" BUILD_DATE ")\n" PACODA_WEBSITE "\n\n" );

	if ( !run_commands( &root_command, argc, argv ) )
		print_usage( );

	return EXIT_SUCCESS;
}
