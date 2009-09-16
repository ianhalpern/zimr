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

#include "msg_switch.h"

static bool initialized = false;

static int read_type = 0, writ_type = 0;

static void msg_switch_failed( msg_switch_t* msg_switch, int code, char* info ) {
	printf( "msg_switch_failed(): %d %s\n", code, info );
	if ( msg_switch->error_event )
		msg_switch->error_event( msg_switch, 0 );
	zfd_clr( msg_switch->sockfd, read_type );
	zfd_clr( msg_switch->sockfd, writ_type );
}

static void msg_switch_read( int sockfd, msg_switch_t* msg_switch ) {
	char type;
	msg_packet_resp_t resp;
	msg_packet_t packet;

	if ( read( sockfd, &type, sizeof( type ) ) != sizeof( type ) ) {
		msg_switch_failed( msg_switch, 0, "msg_switch_read(): read type" );
		return;
	}

	switch( type ) {
		case MSG_TYPE_RESP:
			printf( "got response\n" );
			if ( read( sockfd, &resp, sizeof( resp ) ) != sizeof( resp ) ) {
				msg_switch_failed( msg_switch, 0, "msg_switch_read(): read resp" );
				return;
			}
			msg_accept_resp( msg_switch, resp );

			if ( msg_switch->packet_resp_recvd_event )
				msg_switch->packet_resp_recvd_event( msg_switch, resp );
			break;

		case MSG_TYPE_PACK:
			printf( "read packet\n" );
			if ( read( sockfd, &packet, sizeof( packet ) ) != sizeof( packet ) ) {
				msg_switch_failed( msg_switch, 0, "msg_switch_read(): read packet" );
				return;
			}

			if ( msg_switch->packet_recvd_event )
				msg_switch->packet_recvd_event( msg_switch, packet );
			break;
	}

	if ( msg_switch_is_pending( msg_switch ) )
		zfd_set( sockfd, writ_type, msg_switch );
}

static void msg_switch_writ( int sockfd, msg_switch_t* msg_switch ) {
	char type;
	int size;
	void* data;

	msg_packet_resp_t resp;
	msg_packet_t packet;

	// check if waiting to send returns
	if ( msg_switch_has_pending_resps( msg_switch ) ) {
		printf( "sending read response\n" );
		type = MSG_TYPE_RESP;
		resp = msg_switch_pop_resp( msg_switch );
		size = sizeof( resp );
		data = &resp;
	}

	// pop from queue and write
	else {
		printf( "writing data\n" );
		type = MSG_TYPE_PACK;
		packet = msg_switch_pop_packet( msg_switch );
		size = sizeof( packet );
		data = &packet;
	}

	if ( write( sockfd, &type, sizeof( type ) ) != sizeof( type ) ) {
		msg_switch_failed( msg_switch, 0, "msg_switch_writ(): write type" );
		return;
	}

	if ( write( sockfd, data, size ) != size ) {
		msg_switch_failed( msg_switch, 0, "msg_switch_writ(): write data" );
		return;
	}

	if ( !msg_switch_is_pending( msg_switch ) ) {
		printf( "done writing...waiting\n" );
		zfd_clr( sockfd, writ_type );
	}
}

static msg_packet_t* msg_packet_new( msg_switch_t* msg_switch, int msgid, int flags ) {
	msg_packet_t* packet = (msg_packet_t*) malloc( sizeof( msg_packet_t ) );

	packet->msgid = msgid;
	packet->size = 0;
	packet->flags = flags;

	list_append( &msg_switch->msgs[ msgid ]->queue, packet );

	return packet;
}

static void msg_upd( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg = msg_switch->msgs[ msgid ];

	if ( list_size( &msg->queue ) > 1
	  || ((msg_packet_t*)list_get_at( &msg->queue, 0 ))->flags & PACK_FL_LAST ) {

		if ( msg->status != MSG_STAT_WRITE ) {
			msg->status = MSG_STAT_WRITE;
			list_append( &msg_switch->pending_msgs, msg );
		}
	}

	else
		msg->status = MSG_STAT_READ;

	if ( msg_is_pending( msg_switch, msgid ) ) {
		printf( "ready to write\n" );
		// add write to zfd and fire event
		zfd_set( msg_switch->sockfd, writ_type, msg_switch );

		if ( msg_switch->msg_is_writing_event )
			msg_switch->msg_is_writing_event( msg_switch, msgid );
	}
}

void msg_switch_init( int l_read_type, int l_writ_type ) {
	assert( !initialized );

	read_type = l_read_type;
	writ_type = l_writ_type;

	zfd_register_type( read_type, ZFD_R, ZFD_TYPE_HDLR msg_switch_read );
	zfd_register_type( writ_type, ZFD_W, ZFD_TYPE_HDLR msg_switch_writ );

	initialized = true;
}

msg_switch_t* msg_switch_new(
	int sockfd,
	void (*packet_resp_recvd_event)( msg_switch_t*, msg_packet_resp_t ),
	void (*packet_recvd_event)( msg_switch_t*, msg_packet_t ),
	void (*msg_is_writing_event)( msg_switch_t*, int ),
	void (*error_event)( msg_switch_t*, int )
 ) {
	assert( initialized );

	msg_switch_t* msg_switch = (msg_switch_t*) malloc( sizeof( msg_switch_t ) );
	memset( msg_switch->msgs, 0, sizeof( msg_switch->msgs ) );
	list_init( &msg_switch->pending_resps );
	list_init( &msg_switch->pending_msgs );
	msg_switch->sockfd = sockfd;
	msg_switch->packet_resp_recvd_event = packet_resp_recvd_event;
	msg_switch->packet_recvd_event = packet_recvd_event;
	msg_switch->msg_is_writing_event = msg_is_writing_event;
	msg_switch->error_event = error_event;

	zfd_set( msg_switch->sockfd, read_type, msg_switch );

	return msg_switch;
}

void msg_new( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg = (msg_t*) malloc( sizeof( msg_t ) );

	msg->msgid = msgid;
	msg->status = MSG_STAT_NEW;
	list_init( &msg->queue );

	msg_switch->msgs[ msgid ] = msg;
}

bool msg_switch_has_pending_msgs( msg_switch_t* msg_switch ) {
	return list_size( &msg_switch->pending_msgs );
}

bool msg_switch_has_pending_resps( msg_switch_t* msg_switch ) {
	return list_size( &msg_switch->pending_resps );
}

msg_packet_resp_t msg_switch_pop_resp( msg_switch_t* msg_switch ) {
	msg_packet_resp_t resp = *(msg_packet_resp_t*) list_get_at( &msg_switch->pending_resps, 0 );
	free( list_fetch( &msg_switch->pending_resps ) );
	return resp;
}

void msg_accept_resp( msg_switch_t* msg_switch, msg_packet_resp_t resp ) {
	if ( resp.status == MSG_PACK_RESP_OK ) {
		msg_upd( msg_switch, resp.msgid );
	}
}

void msg_switch_send_resp( msg_switch_t* msg_switch, int msgid, char status ) {
	msg_packet_resp_t* resp = (msg_packet_resp_t*) malloc( sizeof( msg_packet_resp_t ) );
	resp->msgid = msgid;
	resp->status = status;
	list_append( &msg_switch->pending_resps, resp );
}

msg_packet_t msg_switch_pop_packet( msg_switch_t* msg_switch ) {
	msg_t* msg = list_fetch( &msg_switch->pending_msgs );
	msg->status = MSG_STAT_RECVRESP;

	msg_packet_t packet = *(msg_packet_t*) list_get_at( &msg->queue, 0 );
	free( list_fetch( &msg->queue ) );
	return packet;
}

bool msg_switch_is_pending( msg_switch_t* msg_switch ) {
	if ( msg_switch_has_pending_msgs( msg_switch )
	  || msg_switch_has_pending_resps( msg_switch ) )
		return true;
	return false;
}

void msg_push_data( msg_switch_t* msg_switch, int msgid, void* data, int size ) {
	printf( "pushing %d bytes\n", size );
	msg_t* msg = msg_switch->msgs[ msgid ];
	msg_packet_t* packet = NULL;

	if ( !list_size( &msg->queue ) ) {
		packet = msg_packet_new( msg_switch, msgid,
		  msg->status == MSG_STAT_NEW ? PACK_FL_FIRST : 0 );
	} else
		packet = list_get_at( &msg->queue, list_size( &msg->queue ) - 1 );

	int chunk = 0;

	while ( size ) {

		if ( packet->size == PACK_DATA_SIZE )
			packet = msg_packet_new( msg_switch, msgid, 0 );

		if ( size / ( PACK_DATA_SIZE - packet->size ) )
			chunk = PACK_DATA_SIZE - packet->size;
		else
			chunk = size % ( PACK_DATA_SIZE - packet->size );

		memcpy( packet->data + packet->size, data, chunk );

		size -= chunk;
		data += chunk;
		packet->size += chunk;

	}

	msg_upd( msg_switch, msgid );
}
/*
void ztmsg_close( ztmsg_t* msg ) {
	ztpacket_t* packet = NULL;

	if ( !list_size( &msg->queue ) ) {
		list_append( &msg->queue, ( packet = ztpacket_new(
		  msg->msgid, msg->status == ZTMSG_STAT_NEW ? ZTPACK_FL_FIRST : 0 ) ) );
	}

	else
		packet = list_get_at( &msg->queue, list_size( &msg->queue ) - 1 );

	packet->flags |= ZTPACK_FL_LAST;

	msg->status = ZTMSG_STAT_WRITE;
}
*/
bool msg_is_pending( msg_switch_t* msg_switch, int msgid ) {
	return msg_switch->msgs[ msgid ]->status == MSG_STAT_WRITE;
}
