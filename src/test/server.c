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

#include "zfildes.h"
#include "zsocket.h"
#include "msg_switch.h"
#include "simclist.h"
#include "general.h"

#define EXLISN 0x1
#define EXREAD 0x2
#define EXWRIT 0x3
#define INLISN 0x4
#define INREAD 0x5
#define INWRIT 0x6

int inconnfd;

void msg_event_handler( msg_switch_t* msg_switch, msg_event_t event ) {
	switch ( event.type ) {
		case MSG_EVT_NEW:
		case MSG_EVT_SENT:
			break;
		case MSG_EVT_COMPLETE:
		case MSG_EVT_DESTROY:
			puts( "complete." );
			zread( event.data.msgid, false );
			break;
		case MSG_EVT_SPACE_FULL:
			zread( event.data.msgid, false );
			break;
		case MSG_EVT_SPACE_AVAIL:
			zread( event.data.msgid, true );
			break;
		case MSG_EVT_RECVD_PACKET:
			zwrite( event.data.packet->header.msgid, event.data.packet->data, event.data.packet->header.size );
			break;
		case MSG_SWITCH_EVT_IO_FAILED:
		case MSG_SWITCH_EVT_NEW:
		case MSG_SWITCH_EVT_DESTROY:
			break;
	}
}

void insock_event_hdlr( int fd, zsocket_event_t event ) {
	switch ( event.type ) {
		case ZSE_ACCEPT_ERR:
			fprintf( stderr, "New Connection Failed\n" );
			break;
		case ZSE_ACCEPTED_CONNECTION:
			fprintf( stderr, "New Connection #%d on #%d\n", event.data.conn.fd, fd );
			msg_switch_create( event.data.conn.fd, msg_event_handler );
			inconnfd = event.data.conn.fd;
			break;
		case ZSE_READ_DATA:
		case ZSE_WROTE_DATA:
			fprintf( stderr, "Should not be reading or writing in this zsocket_hdlr\n" );
			break;
	}
}

void exsock_event_hdlr( int fd, zsocket_event_t event ) {
	switch ( event.type ) {
		case ZSE_ACCEPT_ERR:
			fprintf( stderr, "New Connection Failed\n" );
			break;
		case ZSE_ACCEPTED_CONNECTION:
			printf( "%d: message start\n", event.data.conn.fd );
			msg_start( inconnfd, event.data.conn.fd );
			break;
		case ZSE_READ_DATA:
			if ( event.data.read.buffer_used <= 0 ) printf( "%d: message end: exread()\n", fd );

			if ( event.data.read.buffer_used == 0 ) {
				msg_end( inconnfd, fd );
			} else if ( event.data.read.buffer_used == -1 )
				msg_kill( inconnfd, fd );
			else
				// push onto queue for sockfd
				msg_send( inconnfd, fd, event.data.read.buffer, event.data.read.buffer_used );
			break;
		case ZSE_WROTE_DATA:
			if ( event.data.read.buffer_used <= 0 )
				msg_kill( inconnfd, fd );

			//if ( PACK_IS_LAST( packet ) )
			//	close( sockfd );
			break;
	}
}

int main( int argc, char* argv[] ) {
	zsocket_init();

	//zfd_register_type( EXLISN, ZFD_R, ZFD_TYPE_HDLR exlisn );
	//zfd_register_type( EXREAD, ZFD_R, ZFD_TYPE_HDLR exread );
	//zfd_register_type( EXWRIT, ZFD_W, ZFD_TYPE_HDLR exwrit );

	int insockfd = zsocket( inet_addr( ZM_PROXY_DEFAULT_ADDR ), ZM_PROXY_DEFAULT_PORT + 1, ZSOCK_LISTEN, insock_event_hdlr, false );
	int exsockfd = zsocket( inet_addr( ZM_PROXY_DEFAULT_ADDR ), 8080, ZSOCK_LISTEN, exsock_event_hdlr, true );

	zaccept( insockfd, true );
	zaccept( exsockfd, true );
	//zfd_set( exsockfd, EXLISN, NULL );

	while( zfd_select(0) );

	return 0;
}
