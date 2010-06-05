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

#include <sys/stat.h>
#include <fcntl.h>

#include "zfildes.h"
#include "zsocket.h"
#include "msg_switch.h"
#include "config.h"

int sockfd;

bool packet_recvd( msg_packet_t* packet ) {
	int fd;
	char filename[ 23 ];
	sprintf( filename, "test-outputs/%d", packet->header.msgid );

	if ( FL_ISSET( packet->header.flags, PACK_FL_FIRST ) ) {
		fd = creat( filename, S_IRUSR | S_IWUSR | O_WRONLY );
		printf( "%d: message start\n", packet->header.msgid );
	} else
		fd = open( filename, O_WRONLY | O_APPEND );

	if ( FL_ISSET( packet->header.flags, PACK_FL_LAST ) )
		printf( "%d: message end\n", packet->header.msgid );

	if ( fd >= 0 ) {
		write( fd, packet->data, packet->header.size );
		close( fd );
	} else
		return false;

	msg_want_data( sockfd, packet->header.msgid );
	return true;
}

void msg_event_handler( msg_switch_t* msg_switch, msg_event_t* event ) {
	switch ( event->type ) {
		case MSG_EVT_READ_START:
			msg_want_data( sockfd, event->data.msgid );
			break;
		case MSG_EVT_WRITE_END:
			break;
		case MSG_EVT_READ_END:
			msg_destroy( sockfd, event->data.msgid );
			break;
		case MSG_EVT_DESTROYED:
			puts( "destroy" );
			break;
		case MSG_EVT_WRITE_SPACE_FULL:
		case MSG_EVT_WRITE_SPACE_AVAIL:
			break;
		case MSG_EVT_RECVD_DATA:
			if ( !packet_recvd( &event->data.packet ) )
				msg_destroy( sockfd, event->data.packet.header.msgid );
			break;
		case MSG_SWITCH_EVT_NEW:
		case MSG_SWITCH_EVT_DESTROY:
			break;
		case MSG_SWITCH_EVT_IO_FAILED:
			msg_switch_destroy( sockfd );
			break;
	}
}

int main( int argc, char* argv[] ) {
	zsocket_init();

	sockfd = zsocket( inet_addr( ZM_PROXY_DEFAULT_ADDR ), ZM_PROXY_DEFAULT_PORT + 1, ZSOCK_CONNECT, NULL, false );
	puts( "connected to server" );
	msg_switch_create( sockfd, msg_event_handler );
	do {
		msg_switch_fire_all_events();
	} while( zfd_select(0) );

	return 0;
}
