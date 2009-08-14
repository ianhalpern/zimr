/*   Podora - Next Generation Web Server
 *
 *+  Copyright (c) 2009 Ian Halpern
 *@  http://Podora.org
 *
 *   This file is part of Podora.
 *
 *   Podora is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Podora is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Podora.  If not, see <http://www.gnu.org/licenses/>
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

void print_usage( ) {
	printf(
"\nUsage: podora [OPTIONS] COMMAND\n\
	-h --help\n\
	--option\n\
"
	);
}

void print_proxy_status( ) {
	int command = PD_CMD_STATUS;
	psocket_t* proxy = psocket_connect( inet_addr( PD_PROXY_ADDR ), PD_PROXY_PORT );

	if ( !proxy ) {
		printf( "could not connect to proxy.\n" );
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

void start_application( char* command, char* cwd ) {
	pcnf_state_t* state = pcnf_state_load( );
	if ( !state ) {
		puts( "failed to load state" );
		exit( EXIT_FAILURE );
	}

	pcnf_state_app_t* app;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		if ( strcmp( app->cwd, cwd ) == 0 && strcmp( app->exec, command ) == 0 ) {
			pcnf_state_free( state );
			puts( "application is already running" );
			return;
		}
	}

	pid_t pid = fork( );

	if ( pid == 0 ) {
		// in the child, do exec
		chdir( cwd );
		char* args[ ] = { command, NULL };
		execvp( args[ 0 ], args );
		puts( "error" );
	} else if ( pid == (pid_t) -1 ) {
		// still in parent, but there is no child
		puts( "parent error: no child" );
	}

	pcnf_state_set_app( state, command, cwd, pid );
	pcnf_state_save( state );
	pcnf_state_free( state );
}

void stop_application( char* command, char* cwd ) {
	pcnf_state_t* state = pcnf_state_load( );

	if ( !state ) {
		puts( "failed to load state" );
		exit( EXIT_FAILURE );
	}

	pcnf_state_app_t* app = NULL;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		if ( strcmp( app->cwd, cwd ) == 0 && strcmp( app->exec, command ) == 0 ) {
			break;
		}
		app = NULL;
	}

	if ( !app ) {
		puts( "application not in state" );
	} else {
		if ( !stopproc( app->pid ) && errno != ESRCH ) {
			printf( "failed: %s\n", strerror( errno ) );
		} else {
			list_delete_at( &state->apps, i );
			pcnf_state_app_free( app );
			pcnf_state_save( state );
		}
	}

	pcnf_state_free( state );
}

void reload_state( ) {
	pcnf_state_t* state = pcnf_state_load( );

	if ( !state ) {
		puts( "failed to load state" );
		exit( EXIT_FAILURE );
	}

	pcnf_state_app_t* app;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		stop_application( app->exec, app->cwd );
		start_application( app->exec, app->cwd );
	}

	pcnf_state_free( state );
}

int main( int argc, char* argv[ ] ) {

	printf( "Podora " PODORA_VERSION " (" BUILD_DATE ")\n" );

	///////////////////////////////////////////////
	// parse command line options
	int i;
	for ( i = 1; i < argc; i++ ) {
		if ( argv[ i ][ 0 ] != '-' ) break;
		if ( strcmp( argv[ i ], "--option" ) == 0 ) {
		} else {
			if ( strcmp( argv[ i ], "--help" ) != 0 && strcmp( argv[ i ], "-h" ) != 0 )
				printf( "\nUnknown Option: %s\n", argv[ i ] );
			print_usage( );
			exit( 0 );
		}
	}

	if ( i == argc ) {
		print_usage( );
		exit( 0 );
	}

	char cwd[ 512 ];
	getcwd( cwd, sizeof( cwd ) );
	// parse command line commands
	for ( i = i; i < argc; i++ ) {
		if ( strcmp( argv[ i ], "status" ) == 0 ) {
			print_proxy_status( );
			break;
		} else if ( strcmp( argv[ i ], "start" ) == 0 ) {
			if ( i + 1 < argc )
				start_application( argv[ i + 1 ], cwd );
			else
				start_application( "podora-website", cwd );
			break;
		} else if ( strcmp( argv[ i ], "stop" ) == 0 ) {
			if ( i + 1 < argc )
				stop_application( argv[ i + 1 ], cwd );
			else
				stop_application( "podora-website", cwd );
			break;
		} else if ( strcmp( argv[ i ], "restart" ) == 0 ) {
			if ( i + 1 < argc ) {
				stop_application( argv[ i + 1 ], cwd );
				start_application( argv[ i + 1 ], cwd );
			} else {
				stop_application( "podora-website", cwd );
				start_application( "podora-website", cwd );
			}
			break;
		} else if ( strcmp( argv[ i ], "reload" ) == 0 ) {
			reload_state( );
			break;
		} else {
			printf( "\nUnknown Command: %s\n", argv[ i ] );
			print_usage( );
			exit( 0 );
		}
	}
	/////////////////////////////////////////////////

	puts( "quit" );
	return EXIT_SUCCESS;
}
