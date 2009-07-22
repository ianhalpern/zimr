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
#include <signal.h>

#include "general.h"
#include "pcom.h"
#include "pfildes.h"

int res_pd, req_pd;
int key;

void start_website( );
void request_handler( int fd, void* udata );
void file_handler( int fd, void* udata );

void quitproc( ) {
	printf( "quit\n" );
}

int main( int argc, char *argv[ ] ) {

	key = randkey( );

	if ( argc < 2 ) {
		return EXIT_FAILURE;
	}

	signal( SIGINT, quitproc );

	if ( ( res_pd = pcom_connect( atoi( argv[ 1 ] ), 3 ) ) < 0 ) {
		return EXIT_FAILURE;
	}

	if ( ( req_pd = pcom_create( ) ) < 0 ) {
		return EXIT_FAILURE;
	}

	pfd_register_type( 1, &request_handler );
	pfd_register_type( 2, &file_handler );

	pfd_set( req_pd, 1, NULL );

	start_website( );

	pfd_start( );

	return 0;
}

void send_cmd( int cmd, void* message, int size ) {
	pcom_transport_t* transport = pcom_open( res_pd, PCOM_WO, cmd, key );
	pcom_write( transport, message, size );
	pcom_close( transport );
}

void start_website( ) {
	int pid = getpid( );
	char message[ sizeof( int ) * 2 ];
	memcpy( message, &pid, sizeof( int ) );
	memcpy( message + sizeof( int ), &req_pd, sizeof( int ) );
	send_cmd( -1, message, sizeof( message ) );
}

void request_handler( int fd, void* udata ) {
	char filename[ 100 ];

	pcom_transport_t* transport = pcom_open( fd, PCOM_RO, 0, 0 );
	pcom_read( transport );

	printf( "requested: %d\n", transport->header->id );

	sprintf( filename, "in/%d.in", transport->header->id );
	if ( ( fd = open( filename, O_RDONLY ) ) < 0 ) {
		return;
	}

	pfd_set( fd, 2, pcom_open( res_pd, PCOM_WO, transport->header->id, key ) );

	pcom_close( transport );
}

void file_handler( int fd, void* transport ) {
	int n;
	char buffer[ 2048 ];

	if ( ( n = read( fd, buffer, sizeof( buffer ) ) ) ) {
		if ( !pcom_write( transport, buffer, n ) ) {
			fprintf( stderr, "[fatal] file_handler: pcom_write() failed\n" );
			exit( EXIT_FAILURE );
		}
	} else {
		pcom_close( transport );
		pfd_clr( fd );
		close( fd );
	}
}
