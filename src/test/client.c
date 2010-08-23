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

/* test-client used for msg_switch testing. test-client acts as a
 * simple ping client. Ment to be
 * used along with test-server and test-zsocket-client.
 */

#include <sys/stat.h>
#include <fcntl.h>

#include "zfildes.h"
#include "zsocket.h"
#include "msg_switch.h"
#include "config.h"

typedef struct {
	char* buf;
	size_t size;
} data_t;

data_t conn_data[FD_SETSIZE];

int sockfd;

void msg_event_handler( int fd, int msgid, int event ) {
	char buf[1024];
	int n;
	switch ( event ) {
		case MSG_EVT_ACCEPT_READY:
			n = msg_accept( fd, msgid );
			if ( !n ) {
				fprintf( stderr, "Accept Failed #%d on #%d: %s\n", msgid, fd, strerror( errno ) );
				break;
			}
			printf( "Accepted Message #%d on #%d\n", msgid, fd );
			msg_set_read( fd, msgid );
			break;
		case MSG_EVT_READ_READY:
			msg_clr_read( fd, msgid );

			n = msg_read( fd, msgid, buf, sizeof( buf ) );
			if ( n <= 0 ) {
				if ( n == -1 ) {
					perror( "msg_event_handler() error: msg_read...closing" );
				} else
					fprintf( stderr, "%d: EOF...closing\n", msgid );
				msg_close( fd, msgid );
			} else {
				conn_data[msgid].buf = malloc( n );
				conn_data[msgid].size = n;
				memcpy( conn_data[msgid].buf, buf, n );
				msg_set_write( fd, msgid );
			}
			break;
		case MSG_EVT_WRITE_READY:
			msg_clr_write( fd, msgid );
			//fprintf( stderr, "write\n" );
			//fprintf( stderr, "Wrote %d bytes of data\n", (int)event.data.write.buffer_used );
			n = msg_write( fd, msgid, conn_data[msgid].buf, conn_data[msgid].size );
			free( conn_data[msgid].buf );
			conn_data[msgid].buf = NULL;
			if ( n == -1 ) {
				perror( "msg_event_handler() error: msg_write" );
				msg_close( fd, msgid );
			} else
				msg_set_read( fd, msgid );
			break;
	}
}

int main( int argc, char* argv[] ) {
	memset( conn_data, 0, sizeof( conn_data ) );
	zs_init();

	sockfd = zsocket( inet_addr( ZM_PROXY_DEFAULT_ADDR ), ZM_PROXY_DEFAULT_PORT + 1, ZSOCK_CONNECT, NULL, false );
	puts( "connected to server" );
	msg_switch_create( sockfd, msg_event_handler );

	do {
		do {
			msg_switch_select();
			zs_select();
		} while ( msg_switch_need_select() || zs_need_select() );
	} while ( zfd_select(2) );

	return 0;
}
