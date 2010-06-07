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

void zsocket_event_hdlr( int fd, int event ) {
	int n;
	char buf[1024];

	switch ( event ) {
		case ZS_EVT_ACCEPT_READY:
			n = zs_accept( fd );
			if ( n == -1 ) {
				perror( "zsocket_event_hdlr() error: zs_accept" );
				//zs_close( fd );
				break;
			}
			fprintf( stderr, "New Connection #%d on #%d\n", n, fd );
			zs_set_read( n );
			zs_set_write( n );
			break;
		case ZS_EVT_READ_READY:
			n = zs_read( fd, buf, sizeof( buf ) );
			if ( n <= 0 ) {
				if ( n == -1 )
					perror( "zsocket_event_hdlr() error: zs_read" );
				else
					fprintf( stderr, "%d: EOF\n", fd );
				//zs_close( fd );
				zs_clr_read( fd );
			} else
				write( STDOUT_FILENO, buf, n );
			break;
		case ZS_EVT_WRITE_READY:
			//fprintf( stderr, "write\n" );
			//fprintf( stderr, "Wrote %d bytes of data\n", (int)event.data.write.buffer_used );
			n = read( STDIN_FILENO, buf, sizeof( buf ) );
			if ( n <= 0 ) {
				if ( n == -1 )
					perror( "zsocket_event_hdlr() error: read" );
				else
					fprintf( stderr, "%d: EOF\n", fd );
				//zs_close( fd );
				zs_clr_write( fd );
				return;
			}
			n = zs_write( fd, buf, n );
			if ( n == -1 ) {
				perror( "zsocket_event_hdlr() error: zs_write" );
				zs_close( fd );
			}
			break;
	}
}

int main( int argc, char **argv ) {
	zs_init();

	int fd = zsocket( INADDR_ANY, 8080, ZSOCK_LISTEN, zsocket_event_hdlr, true );
	zs_set_read( fd );

	do {
		while ( zs_select() );
	} while( zfd_select(0) );

	return EXIT_SUCCESS;
}
