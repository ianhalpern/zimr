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

#include "ptransport.h"
#include "psocket.h"
#include "pfildes.h"

void print_usage( ) {
	printf(
"\nUsage: podora [OPTIONS] COMMAND\n\
	-h --help\n\
	--option\n\
"
	);
}

int main( int argc, char* argv[ ] ) {
	int command = 0;

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

	// parse command line commands
	for ( i = i; i < argc; i++ ) {
		if ( strcmp( argv[ i ], "status" ) == 0 ) {
			command = PD_CMD_STATUS;
			break;
		} else {
			printf( "\nUnknown Command: %s\n", argv[ i ] );
			print_usage( );
			exit( 0 );
		}
	}
	/////////////////////////////////////////////////

	psocket_t* proxy = psocket_connect( inet_addr( PD_PROXY_ADDR ), PD_PROXY_PORT );

	if ( !proxy ) {
		printf( "could not connect to proxy.\n" );
		return EXIT_FAILURE;
	}

	ptransport_t* transport;

	transport = ptransport_open( proxy->sockfd, PT_WO, command );
	ptransport_close( transport );

	transport  = ptransport_open( proxy->sockfd, PT_RO, 0 );
	ptransport_read( transport );
	ptransport_close( transport );

	puts( transport->message );

	psocket_close( proxy );
	puts( "quit" );
	return EXIT_SUCCESS;
}
