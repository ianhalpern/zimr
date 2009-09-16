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

void packet_resp_recvd_event( msg_switch_t* msg_switch, msg_packet_resp_t resp ) {
	zfd_reset( resp.msgid, EXREAD );
}

void packet_recvd_event( msg_switch_t* msg_switch, msg_packet_t packet ) {
	zfd_set( packet.msgid, EXWRIT, memdup( &packet, sizeof( packet ) ) );
}

void msg_is_writing_event( msg_switch_t* msg_switch, int msgid ) {
	zfd_clr( msgid, EXREAD );
}

void inlisn( int sockfd, void* udata ) {
	struct sockaddr_in cli_addr;
	unsigned int cli_len = sizeof( cli_addr );
	int clientfd;

	// Connection request on original socket.
	if ( ( clientfd = accept( sockfd, (struct sockaddr*) &cli_addr, &cli_len ) ) < 0 ) {
		return;
	}

	msg_switch = msg_switch_new( clientfd, packet_resp_recvd_event, packet_recvd_event, msg_is_writing_event, NULL );
}

void exlisn( int sockfd, void* udata ) {
	struct sockaddr_in cli_addr;
	unsigned int cli_len = sizeof( cli_addr );
	int newsockfd;

	// Connection request on original socket.
	if ( ( newsockfd = accept( sockfd, (struct sockaddr*) &cli_addr, &cli_len ) ) < 0 ) {
		return;
	}

	msg_new( msg_switch, newsockfd );
	zfd_set( newsockfd, EXREAD, NULL );
}

void exread( int sockfd, void* udata ) {
	char buf[ 2048 ];
	int n = read( sockfd, buf, sizeof( buf ) );

	// push onto queue for sockfd
	msg_push_data( msg_switch, sockfd, buf, n );
}

void exwrit( int sockfd, msg_packet_t* packet ) {
	// pop from queue and write, remove if none to write left
	write( sockfd, packet->data, packet->size );
	msg_switch_send_resp( msg_switch, sockfd, MSG_PACK_RESP_OK );
	zfd_clr( sockfd, EXWRIT );
}

int main( int argc, char* argv[ ] ) {
	zsocket_init( );
	msg_switch_init( INREAD, INWRIT );

	zfd_register_type( INLISN, ZFD_R, ZFD_TYPE_HDLR inlisn );
	zfd_register_type( EXLISN, ZFD_R, ZFD_TYPE_HDLR exlisn );
	zfd_register_type( EXREAD, ZFD_R, ZFD_TYPE_HDLR exread );
	zfd_register_type( EXWRIT, ZFD_W, ZFD_TYPE_HDLR exwrit );

	int insockfd = zsocket( inet_addr( ZM_PROXY_ADDR ), ZM_PROXY_PORT + 1, ZSOCK_LISTEN );

	int exsockfd = zsocket( inet_addr( ZM_PROXY_ADDR ), 8080, ZSOCK_LISTEN );

	zfd_set( insockfd, INLISN, NULL );
	zfd_set( exsockfd, EXLISN, NULL );

	while( zfd_select( 0 ) );

	return 0;
}
