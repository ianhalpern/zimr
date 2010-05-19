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

/*void write_data_complete( int fd, int n, void* buffer, size_t s, void* udata ) {
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
		write( STDOUT_FILENO, buffer, n );
	else {
		//puts( "error: read_data_complete" );
		zfd_clr( fd, READ );
		zclose( fd );
		if ( n == 0 )
			exit(EXIT_SUCCESS);
	}
	//zwrite( fd, memdup( buffer, 1024 ), 1024, write_data_complete, NULL );
	//zwrite( fd, memdup( buffer, 1024 ), 1024, write_data_complete, NULL );
	//zfd_set( fd, WRITE, NULL );
	//free( buffer );
}

void read_data( int fd, void* udata ) {
	static char buff[ 1024 ];
	zread( fd, buff, 1024, read_data_complete, NULL );
}

void accept_conn_complete( int fd, struct sockaddr_in *cli_addr, socklen_t *cli_len, void* udata ) {
	free( cli_addr );
	free( cli_len );

	if ( fd <= 0 ) {
		//puts( "error: accept_conn_complete" );
		return;
	}

	zfd_set( fd, READ, NULL );
}

void accept_conn( int fd, void* udata ) {
	struct sockaddr_in *cli_addr = malloc( sizeof( struct sockaddr_in ) );
	socklen_t *cli_len = malloc( sizeof( socklen_t ) );
	*cli_len = sizeof( cli_addr );
	zaccept( fd, cli_addr, cli_len, accept_conn_complete, NULL );
}*/

void write_data( int fd ) {
	static char buff[ 1024 ];
	int n = read( STDIN_FILENO, buff, 1024 );
	if ( n > 0 )
		zwrite( fd, buff, n );
		//zclose( fd );
}

void zsocket_event_hdlr( int fd, zsocket_event_t event ) {
	switch ( event.type ) {
		case ZSE_ACCEPT_ERR:
			fprintf( stderr, "New Connection Failed\n" );
			break;
		case ZSE_ACCEPTED_CONNECTION:
			fprintf( stderr, "New Connection #%d on #%d\n", event.data.conn.fd, fd );
			zread( event.data.conn.fd, true );
			write_data(  event.data.conn.fd );
			break;
		case ZSE_READ_DATA:
			if ( event.data.read.buffer_used <= 0 )
				zclose( fd );
			else
				write( STDOUT_FILENO, event.data.read.buffer, event.data.read.buffer_used );
			break;
		case ZSE_WROTE_DATA:
			//fprintf( stderr, "Wrote %d bytes of data\n", (int)event.data.write.buffer_used );
			if ( event.data.write.buffer_used <= 0 )
				zclose( fd );
			else
				write_data( fd );
			break;
	}
}

int main( int argc, char **argv ) {
	zsocket_init();

	//zfd_register_type( ACCEPT, ZFD_R, ZFD_TYPE_HDLR accept_conn );
	//zfd_register_type( READ,   ZFD_R, ZFD_TYPE_HDLR read_data );
	//zfd_register_type( WRITE,  ZFD_W, ZFD_TYPE_HDLR write_data );

	//puts( "Hello World, I'm a server." );

	int fd = zsocket( INADDR_ANY, 8080, ZSOCK_LISTEN, zsocket_event_hdlr, true );
	//zfd_set( fd, ACCEPT, NULL );
	zaccept( fd, true );

	while( zfd_select(0) );

	return EXIT_SUCCESS;
}
