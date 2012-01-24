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

/*
 * test-zsocket-server: a ping server used for testing the zsocket library
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

#include "zsocket.h"
#include "zfildes.h"

typedef struct {
	char* buf;
	size_t size;
} data_t;

data_t conn_data[FD_SETSIZE];

int connfd;

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
			//zs_set_write( n );
			break;
		case ZS_EVT_READ_READY:
			zs_clr_read( fd );

			n = zs_read( fd, buf, sizeof( buf ) );
			if ( n <= 0 ) {
				if ( n == -1 ) {
					perror( "zsocket_event_hdlr() error: zs_read...closing" );
				} else
					fprintf( stderr, "%d: EOF...closing\n", fd );
				zs_close( fd );
			} else {
				conn_data[fd].buf = malloc( n );
				conn_data[fd].size = n;
				memcpy( conn_data[fd].buf, buf, n );
				zs_set_write( fd );
			}
			break;
		case ZS_EVT_WRITE_READY:
			zs_clr_write( fd );
			//fprintf( stderr, "write\n" );
			//fprintf( stderr, "Wrote %d bytes of data\n", (int)event.data.write.buffer_used );
			n = zs_write( fd, conn_data[fd].buf, conn_data[fd].size );
			free( conn_data[fd].buf );
			conn_data[fd].buf = NULL;
			if ( n == -1 ) {
				perror( "zsocket_event_hdlr() error: zs_write" );
				zs_close( fd );
			} else
				zs_set_read( fd );
			break;
	}
}

int main( int argc, char **argv ) {
	memset( conn_data, 0, sizeof( conn_data ) );
	zs_init( NULL, NULL );

	connfd = zsocket( INADDR_ANY, 8080, ZSOCK_LISTEN, zsocket_event_hdlr, false );
	zs_set_read( connfd );

	do {
		while ( zs_select() );
	} while( zfd_select(0) );

	return EXIT_SUCCESS;
}
