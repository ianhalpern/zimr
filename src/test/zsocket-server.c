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
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

#include "zsocket.h"
#include "zfildes.h"

#define ACCEPT 0x1
#define READ   0x2
#define WRITE  0x3

void write_data_complete( int fd, int n, void* buffer, size_t s, void* udata ) {
	if ( n > 0 )
		printf( "wrote: %s\n", buffer );
	else
		puts( "error: write_data_complete" );

	free( buffer );
	//zfd_clr( fd, WRITE );
}

void write_data( int fd, void* udata ) {
	void* buff = malloc( 1024 );
	strcpy( buff, "This is a test, holler!" );
	zwrite( fd, buff, 1024, write_data_complete, NULL );
}

void read_data_complete( int fd, int n, void* buffer, size_t s, void* udata ) {
	if ( n > 0 )
		printf( "read: %s\n", buffer );
	else {
		puts( "error: read_data_complete" );
		zfd_clr( fd, READ );
		zclose( fd );
		return;
	}
	zwrite( fd, memdup( buffer, 1024 ), 1024, write_data_complete, NULL );
	zwrite( fd, memdup( buffer, 1024 ), 1024, write_data_complete, NULL );
	//zfd_set( fd, WRITE, NULL );
	free( buffer );
}

void read_data( int fd, void* udata ) {
	void* buff = malloc( 1024 );
	memset( buff, 0, 1024 );
	zread( fd, buff, 1024, read_data_complete, NULL );
}

void accept_conn_complete( int fd, struct sockaddr_in *cli_addr, socklen_t *cli_len, void* udata ) {
	free( cli_addr );
	free( cli_len );

	if ( fd <= 0 ) {
		puts( "error: accept_conn_complete" );
		return;
	}

	zfd_set( fd, READ, NULL );
}

void accept_conn( int fd, void* udata ) {
	struct sockaddr_in *cli_addr = malloc( sizeof( struct sockaddr_in ) );
	socklen_t *cli_len = malloc( sizeof( socklen_t ) );
	*cli_len = sizeof( cli_addr );
	zaccept( fd, cli_addr, cli_len, accept_conn_complete, NULL );
}

int main( int argc, char **argv ) {
	zsocket_init();

	zfd_register_type( ACCEPT, ZFD_R, ZFD_TYPE_HDLR accept_conn );
	zfd_register_type( READ,   ZFD_R, ZFD_TYPE_HDLR read_data );
	zfd_register_type( WRITE,  ZFD_W, ZFD_TYPE_HDLR write_data );

	puts( "Hello World, I'm a server." );

	int fd = zsocket( INADDR_ANY, 8080, ZSOCK_LISTEN, true );
	zfd_set( fd, ACCEPT, NULL );

	while( zfd_select(0) );

	return EXIT_SUCCESS;
}
