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

/* test-server used for msg_switch testing. Ment to be
 * used along with test-client and test-zsocket-client.
 */

#include "zfildes.h"
#include "zsocket.h"
#include "msg_switch.h"
#include "simclist.h"
#include "general.h"

typedef struct {
	char* rbuf;
	size_t rsize;
	char* wbuf;
	size_t wsize;
} rw_data_t;

rw_data_t conn_data[FD_SETSIZE];

int inconnfd;

void msg_event_handler( int fd, int msgid, int event ) {
	char buf[1024];
	int n;

	//printf( "%d msg event on %d %d\n", event, fd, msgid );

	switch ( event ) {
		case MSG_EVT_ACCEPT_READY:
			break;
		case MSG_EVT_WRITE_READY:
			msg_clr_write( fd, msgid );

			assert( conn_data[msgid].rbuf );

			n = msg_write( inconnfd, msgid, conn_data[msgid].rbuf, conn_data[msgid].rsize );
			free( conn_data[msgid].rbuf );
			conn_data[msgid].rbuf = NULL;

			if ( n == -1 ) {
				perror( "msg_event_handler() error: msg_write" );
				zs_close( msgid );
				msg_close( inconnfd, msgid );
			} else
				zs_set_read( msgid );
			break;
		case MSG_EVT_READ_READY:
			msg_clr_read( fd, msgid );

			assert( !conn_data[msgid].wbuf );

			n = msg_read( inconnfd, msgid, buf, sizeof( buf ) );
			if ( n <= 0 ) {
				if ( n == -1 ) {
					perror( "msg_event_handler() error: msg_read...closing" );
				} else
					fprintf( stderr, "%d: EOF...closing\n", fd );
				zs_close( msgid );
				msg_close( inconnfd, msgid );
			} else {
				conn_data[msgid].wbuf = malloc( n );
				conn_data[msgid].wsize = n;
				memcpy( conn_data[msgid].wbuf, buf, n );
				zs_set_write( msgid );
			}
			break;
		case MSG_SWITCH_EVT_IO_ERROR:
			for ( n = 0; n < FD_SETSIZE; n++ ) {
				if ( msg_exists( inconnfd, n ) ) {
					if ( conn_data[n].wbuf ) 
						free( conn_data[n].wbuf );
					if ( conn_data[n].rbuf ) 
						free( conn_data[n].rbuf );
					conn_data[n].wbuf = NULL;
					conn_data[n].rbuf = NULL;
					msg_close( inconnfd, n );
					zs_close( n );
				}
			}
			msg_switch_destroy( inconnfd );
			zs_close( inconnfd );
			break;
	}
}

void insock_event_hdlr( int fd, int event ) {
	switch ( event ) {
		case ZS_EVT_ACCEPT_READY:
			inconnfd = zs_accept( fd );
			if ( inconnfd == -1 ) {
				perror( "insock_event_hdlr() error: zs_accept" );
				zs_close( fd );
				break;
			}
			fprintf( stderr, "Accepted Internal Connection #%d on #%d\n", inconnfd, fd );
			msg_switch_create( inconnfd, msg_event_handler );
			break;
		default:
			fprintf( stderr, "Should not be reading or writing in this zsocket_hdlr\n" );
			break;
	}
}

void exsock_event_hdlr( int fd, int event ) {
	//printf( "%d exsock event on %d\n", event, fd );
	char buf[1024];
	int n;

	switch ( event ) {
		case ZS_EVT_ACCEPT_READY:
			n = zs_accept( fd );
			if ( n == -1 ) {
				perror( "exsock_event_hdlr() error: zs_accept" );
				//zs_close( fd );
				break;
			}
			fprintf( stderr, "Accepted External Connection #%d on #%d\n", n, fd );
			msg_open( inconnfd, n );
			zs_set_read( n );
			msg_set_read( inconnfd, n );
			break;
		case ZS_EVT_READ_READY:
			zs_clr_read( fd );

			assert( !conn_data[fd].rbuf );

			n = zs_read( fd, buf, sizeof( buf ) );
			if ( n <= 0 ) {
				if ( n == -1 ) {
					fprintf( stderr, "%d: exsock_event_hdlr() error: zs_read...closing\n", fd );
				} else
					fprintf( stderr, "%d: EOF...closing\n", fd );
				zs_clr_write( fd );
				zs_close( fd );
				msg_close( inconnfd, fd );
			} else {
				conn_data[fd].rbuf = malloc( n );
				conn_data[fd].rsize = n;
				memcpy( conn_data[fd].rbuf, buf, n );
				msg_set_write( inconnfd, fd );
			}
			break;
		case ZS_EVT_WRITE_READY:
			zs_clr_write( fd );
			assert( conn_data[fd].wbuf );
			//fprintf( stderr, "write\n" );
			//fprintf( stderr, "Wrote %d bytes of data\n", (int)event.data.write.buffer_used );
			n = zs_write( fd, conn_data[fd].wbuf, conn_data[fd].wsize );
			free( conn_data[fd].wbuf );
			conn_data[fd].wbuf = NULL;

			if ( n == -1 ) {
				perror( "exsock_event_hdlr() error: zs_write" );
				zs_clr_read( fd );
				zs_close( fd );
				msg_close( inconnfd, fd );
				msg_clr_read( inconnfd, fd );
				msg_clr_write( inconnfd, fd );
			} else
				msg_set_read( inconnfd, fd );

			//if ( PACK_IS_LAST( packet ) )
			//	close( sockfd );
			break;
	}
}

int main( int argc, char* argv[] ) {
	memset( conn_data, 0, sizeof( conn_data ) );
	zs_init();

	int insock = zsocket( inet_addr( ZM_PROXY_DEFAULT_ADDR ), ZM_PROXY_DEFAULT_PORT + 1, ZSOCK_LISTEN, insock_event_hdlr, false );
	int exsock = zsocket( inet_addr( ZM_PROXY_DEFAULT_ADDR ), 8080, ZSOCK_LISTEN, exsock_event_hdlr, false );

	zs_set_read( insock );
	zs_set_read( exsock );

	do {
		do {
			msg_switch_select();
			zs_select();
		} while ( msg_switch_need_select() || zs_need_select() );
	} while ( zfd_select(2) );

	return 0;
}
