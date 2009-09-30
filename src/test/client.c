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

#define INREAD 0x5
#define INWRIT 0x6

msg_switch_t* msg_switch;

bool packet_recvd( msg_packet_t* packet ) {
	int fd;
	char filename[ 23 ];
	sprintf( filename, "test-outputs/%d", packet->msgid );

	if ( FL_ISSET( PACK_FL_FIRST, packet->flags ) )
		fd = creat( filename, S_IRUSR | S_IWUSR | O_WRONLY );
	else
		fd = open( filename, O_WRONLY | O_APPEND );

	if ( fd >= 0 ) {
		write( fd, packet->data, packet->size );
		close( fd );
	} else
		return false;

	return true;
}

void msg_event_handler( msg_switch_t* msg_switch, msg_event_t event ) {
	switch ( event.type ) {
		case MSG_EVT_RECV_LAST:
			// rename
			break;
		case MSG_EVT_RECV_KILL:
			// remove
			break;
		case MSG_EVT_RECV_PACK:
			if ( packet_recvd( event.data.packet ) )
				msg_switch_send_resp( msg_switch, event.data.packet->msgid, MSG_PACK_RESP_OK );
			else
				msg_switch_send_resp( msg_switch, event.data.packet->msgid, MSG_PACK_RESP_FAIL );
			break;
		case MSG_EVT_NEW:
		case MSG_EVT_DESTROY:
		case MSG_EVT_COMPLETE:
		case MSG_EVT_RECV_RESP:
		case MSG_EVT_BUF_FULL:
		case MSG_EVT_BUF_EMPTY:
		case MSG_SWITCH_EVT_NEW:
		case MSG_SWITCH_EVT_DESTROY:
		case MSG_SWITCH_EVT_IO_FAILED:
			break;
	}
}

int main( int argc, char* argv[ ] ) {
	zsocket_init( );
	msg_switch_init( INREAD, INWRIT );

	int sockfd = zsocket( inet_addr( ZM_PROXY_ADDR ), ZM_PROXY_PORT + 1, ZSOCK_CONNECT );

	msg_switch = msg_switch_new( sockfd, msg_event_handler );

	while( zfd_select( 0 ) );

	return 0;
}
