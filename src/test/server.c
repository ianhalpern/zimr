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

msg_switch_t* msg_switch;

void msg_event_handler( msg_switch_t* msg_switch, msg_event_t event ) {
	switch ( event.type ) {
		case MSG_EVT_NEW:
			zfd_set( event.data.msgid, EXREAD, NULL );
			break;
		case MSG_EVT_COMPLETE:
			zfd_clr( event.data.msgid, EXREAD );
			break;
		case MSG_EVT_DESTROY:
			zfd_clr( event.data.msgid, EXREAD );
			break;
		case MSG_RECV_EVT_KILL:
			zfd_clr( event.data.msgid, EXWRIT );
			close( event.data.msgid );
			break;
		case MSG_RECV_EVT_PACK:
			zfd_set( event.data.packet->msgid, EXWRIT, memdup( event.data.packet, sizeof( msg_packet_t ) ) );
			break;
		case MSG_EVT_BUF_FULL:
			//printf( "%d full\n", event.data.msgid );
			zfd_clr( event.data.msgid, EXREAD );
			break;
		case MSG_EVT_BUF_EMPTY:
			//printf( "%d empty\n", event.data.msgid );
			zfd_set( event.data.msgid, EXREAD, NULL );
			break;

		case MSG_SWITCH_EVT_NEW:
		case MSG_RECV_EVT_RESP:
		case MSG_SWITCH_EVT_DESTROY:
		case MSG_SWITCH_EVT_IO_FAILED:
			break;
	}
}

void inlisn_complete( int fd, struct sockaddr_in* cli_addr, socklen_t* cli_len, void* udata ) {
	if ( fd < 0 ) return;
	puts( "client connected" );
	msg_switch = msg_switch_new( fd, msg_event_handler );
}

void inlisn( int sockfd, void* udata ) {
	struct sockaddr_in* cli_addr = malloc( sizeof( struct sockaddr_in ) );
	socklen_t* cli_len = malloc( sizeof( socklen_t ) );
	*cli_len = sizeof( struct sockaddr_in );

	// Connection request on original socket.
	zaccept( sockfd, cli_addr, cli_len, inlisn_complete, NULL );
}

void exlisn_complete( int fd, struct sockaddr_in* cli_addr, socklen_t* cli_len, void* udata ) {
	if ( fd < 0 ) return;
	printf( "%d: message start: exlisn()\n", fd );
	msg_new( msg_switch, fd );
}

void exlisn( int sockfd, void* udata ) {
	struct sockaddr_in* cli_addr = malloc( sizeof( struct sockaddr_in ) );
	socklen_t* cli_len = malloc( sizeof( socklen_t ) );
	*cli_len = sizeof( struct sockaddr_in );

	// Connection request on original socket.
	zaccept( sockfd, cli_addr, cli_len, exlisn_complete, NULL );
}

void exread_complete( int sockfd, int ret, void* buf, size_t len, void* udata ) {
	if ( ret <= 0 ) printf( "%d: message end: exread()\n", sockfd );

	if ( ret == 0 )
		msg_end( msg_switch, sockfd );
	else if ( ret == -1 )
		msg_kill( msg_switch, sockfd );
	else
		// push onto queue for sockfd
		msg_push_data( msg_switch, sockfd, buf, ret );

	free( buf );
}

void exread( int sockfd, void* udata ) {
	void* buf = malloc( 2048 );
	zread( sockfd, buf, 2048, exread_complete, NULL );
	//zfd_clr( sockfd, EXREAD );
}

void exwrit( int sockfd, msg_packet_t* packet ) {
	// pop from queue and write, remove if none to write left
	int n = write( sockfd, packet->data, packet->size );
	if ( n <= 0 )
		msg_switch_send_pack_resp( msg_switch, packet, MSG_PACK_RESP_FAIL );
	else
		msg_switch_send_pack_resp( msg_switch, packet, MSG_PACK_RESP_OK );

	if ( PACK_IS_LAST( packet ) )
		close( sockfd );

	zfd_clr( sockfd, EXWRIT );
	free( packet );
}

int main( int argc, char* argv[ ] ) {
	zsocket_init( );
	msg_switch_init( INREAD, INWRIT );

	zfd_register_type( INLISN, ZFD_R, ZFD_TYPE_HDLR inlisn );
	zfd_register_type( EXLISN, ZFD_R, ZFD_TYPE_HDLR exlisn );
	zfd_register_type( EXREAD, ZFD_R, ZFD_TYPE_HDLR exread );
	zfd_register_type( EXWRIT, ZFD_W, ZFD_TYPE_HDLR exwrit );

	int insockfd = zsocket( inet_addr( ZM_PROXY_DEFAULT_ADDR ), ZM_PROXY_DEFAULT_PORT + 1, ZSOCK_LISTEN, false );

	int exsockfd = zsocket( inet_addr( ZM_PROXY_DEFAULT_ADDR ), 8080, ZSOCK_LISTEN, false );

	zfd_set( insockfd, INLISN, NULL );
	zfd_set( exsockfd, EXLISN, NULL );

	while( zfd_select( 0 ) );

	return 0;
}
