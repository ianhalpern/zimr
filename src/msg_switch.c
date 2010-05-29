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

static int n_print = 0;

static void msg_update_status( msg_switch_t* msg_switch, int msgid, char type, int status );
static void msg_switch_event( msg_switch_t* msg_switch, int type, ... );
static void msg_switch_push_resp( msg_switch_t* msg_switch, msg_resp_t* resp );

msg_switch_t* msg_switch_get_by_sockfd( int sockfd ) {
	zsocket_t* zs = zsocket_get_by_sockfd( sockfd );
	return zs ? (msg_switch_t*) zs->general.udata : NULL;
}

/////////////////////////////////////////////////////////////////////////////////////
// MSG functions ////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

static msg_t* msg_get( msg_switch_t* msg_switch, int msgid ) {
	return msg_switch->msgs[ msgid + FD_SETSIZE ];
}

static void msg_set( msg_switch_t* msg_switch, int msgid, msg_t* msg ) {
	msg_switch->msgs[ msgid + FD_SETSIZE ] = msg;
}

static bool msg_has_available_writ_packets( msg_switch_t* msg_switch, int msgid, int n ) {
	msg_t* msg = msg_get( msg_switch, msgid );
	return ( list_size( &msg->writ_queue ) >= n );
}

static bool msg_has_available_read_packets( msg_switch_t* msg_switch, int msgid, int n ) {
	msg_t* msg = msg_get( msg_switch, msgid );
	return ( list_size( &msg->read_queue ) >= n );
}

static void msg_create( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg = (msg_t*) malloc( sizeof( msg_t ) );

	msg->msgid = msgid;
	msg->status = 0;
	msg->n_writ = 0;
	msg->n_read = 0;
	msg->incomplete_writ_packet = NULL;
	list_init( &msg->read_queue );
	list_init( &msg->writ_queue );

	msg_set( msg_switch, msgid, msg );
	msg_update_status( msg_switch, msgid, SET, MSG_STAT_NEW );
}

static void msg_send_resp( msg_switch_t* msg_switch, int msgid, char status ) {
	msg_t* msg;
	assert( msg = msg_get( msg_switch, msgid ) );
	assert( status != MSG_RESP_OK || FL_ISSET( msg->status, MSG_STAT_NEED_TO_SEND_RESP ) );

	msg_resp_t* resp = (msg_resp_t*) malloc( sizeof( msg_resp_t ) );
	memset( resp, 0, sizeof( msg_resp_t ) ); // initializes all bytes including padded bytes

	resp->msgid = msgid;
	resp->status = status;

	msg_switch_push_resp( msg_switch, resp );
	//list_append( &msg_switch->pending_resps, resp );

	msg_update_status( msg_switch, msgid, CLR, MSG_STAT_NEED_TO_SEND_RESP );
}

static void msg_recv_resp( msg_switch_t* msg_switch, msg_resp_t resp ) {
	msg_t* msg;
	assert( msg = msg_get( msg_switch, resp.msgid ) );

	if ( resp.status == MSG_RESP_FAIL ) {
		if ( !FL_ISSET( msg->status, MSG_STAT_KILL_SENT ) ) {
			msg_send_resp( msg_switch, resp.msgid, MSG_RESP_FAIL );// if wasn't the sender of the kill must return the kill
		}
		msg_destroy( msg_switch->sockfd, resp.msgid );
		return;
	}

	assert( FL_ISSET( msg->status, MSG_STAT_WAITING_FOR_RESP ) );

	msg_update_status( msg_switch, resp.msgid, CLR, MSG_STAT_WAITING_FOR_RESP );
}

static void msg_push_writ_packet( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg;
	assert( msg = msg_get( msg_switch, msgid ) );
	assert( msg->incomplete_writ_packet );
	if( FL_ISSET( msg->status, MSG_STAT_KILL_SENT ) ) return;
	msg_update_status( msg_switch, msgid, CLR, MSG_STAT_NEW );

	list_append( &msg_get( msg_switch, msgid )->writ_queue, msg->incomplete_writ_packet );
	msg->incomplete_writ_packet = NULL;

	msg_update_status( msg_switch, msg->msgid, SET, MSG_STAT_PACKET_AVAIL_TO_WRIT );
	if ( msg_has_available_writ_packets( msg_switch, msg->msgid, MSG_N_BUFF_PACKETS_ALLOWED ) )
		msg_update_status( msg_switch, msg->msgid, CLR, MSG_STAT_SPACE_AVAIL_FOR_WRIT );
}

static msg_packet_t msg_pop_writ_packet( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg;
	assert( msg = msg_get( msg_switch, msgid ) );
	assert( !FL_ISSET( msg->status, MSG_STAT_KILL_SENT ) );
	assert( !FL_ISSET( msg->status, MSG_STAT_WAITING_FOR_RESP ) );

	msg_packet_t packet = *(msg_packet_t*) list_get_at( &msg->writ_queue, 0 );
	free( list_fetch( &msg->writ_queue ) );

	msg_update_status( msg_switch, msg->msgid, SET, MSG_STAT_WAITING_FOR_RESP );

	if ( !msg_has_available_writ_packets( msg_switch, msg->msgid, MSG_N_BUFF_PACKETS_ALLOWED ) )
		msg_update_status( msg_switch, msg->msgid, SET, MSG_STAT_SPACE_AVAIL_FOR_WRIT );
	if ( !msg_has_available_writ_packets( msg_switch, msg->msgid, 1 ) )
		msg_update_status( msg_switch, msg->msgid, CLR, MSG_STAT_PACKET_AVAIL_TO_WRIT );

	return packet;
}

static void msg_push_read_packet( msg_switch_t* msg_switch, int msgid, msg_packet_t* packet ) {
	msg_t* msg;
	assert( msg = msg_get( msg_switch, msgid ) );
	assert( !FL_ISSET( msg->status, MSG_STAT_READ_COMPLETED ) );
	if( FL_ISSET( msg->status, MSG_STAT_KILL_SENT ) ) return;
	msg_update_status( msg_switch, msgid, CLR, MSG_STAT_NEW );
	msg->n_read++;

	if ( FL_ISSET( packet->header.flags, PACK_FL_FIRST ) )
		msg_update_status( msg_switch, msgid, SET, MSG_STAT_READ_STARTED );

	assert( FL_ISSET( msg->status, MSG_STAT_READ_STARTED ) );

	list_append( &msg_get( msg_switch, msgid )->read_queue, memdup( packet, sizeof( msg_packet_t ) ) );

	int sockfd = msg_switch->sockfd;
	msg_update_status( msg_switch, msg->msgid, SET, MSG_STAT_PACKET_AVAIL_TO_READ );
	if( !msg_switch_get_by_sockfd( sockfd ) ) return;
	if ( msg_has_available_read_packets( msg_switch, msg->msgid, MSG_N_BUFF_PACKETS_ALLOWED ) )
		msg_update_status( msg_switch, msg->msgid, CLR, MSG_STAT_SPACE_AVAIL_FOR_READ );
	msg_update_status( msg_switch, msg->msgid, SET, MSG_STAT_NEED_TO_SEND_RESP );

	if ( FL_ISSET( packet->header.flags, PACK_FL_LAST ) )
		msg_update_status( msg_switch, msgid, SET, MSG_STAT_READ_COMPLETED );
}

static msg_packet_t msg_pop_read_packet( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg = msg_get( msg_switch, msgid );
	assert( !FL_ISSET( msg->status, MSG_STAT_KILL_SENT ) );

	msg_packet_t packet = *(msg_packet_t*) list_get_at( &msg->read_queue, 0 );
	free( list_fetch( &msg->read_queue ) );

	if ( !msg_has_available_read_packets( msg_switch, msg->msgid, MSG_N_BUFF_PACKETS_ALLOWED ) )
		msg_update_status( msg_switch, msg->msgid, SET, MSG_STAT_SPACE_AVAIL_FOR_READ );
	if ( !msg_has_available_read_packets( msg_switch, msg->msgid, 1 ) )
		msg_update_status( msg_switch, msg->msgid, CLR, MSG_STAT_PACKET_AVAIL_TO_READ );

	return packet;
}

static msg_packet_t* msg_packet_new( msg_switch_t* msg_switch, int msgid, int flags ) {
	msg_t* msg;
	assert( msg = msg_get( msg_switch, msgid ) );
	if ( msg->incomplete_writ_packet )
		msg_push_writ_packet( msg_switch, msgid );

	msg_packet_t* packet = (msg_packet_t*) malloc( sizeof( msg_packet_t ) );

	packet->header.msgid = msgid;
	packet->header.size = 0;
	packet->header.flags = flags;
	msg->incomplete_writ_packet = packet;
	msg->n_writ++;
	return packet;
}

static void msg_clear( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg = msg_get( msg_switch, msgid );

	while ( list_size( &msg->writ_queue ) )
		free( list_fetch( &msg->writ_queue ) );

	while ( list_size( &msg->read_queue ) )
		free( list_fetch( &msg->read_queue ) );
}

// External MSG functions /////////////////////////////////////////////////////////////

bool msg_exists( int sockfd, int msgid ) {
	msg_switch_t* msg_switch;
	assert( msg_switch = msg_switch_get_by_sockfd( sockfd ) );

	return msg_get( msg_switch, msgid );
}

void msg_start( int fd, int msgid ) {
	msg_switch_t* msg_switch;
	assert( msg_switch = msg_switch_get_by_sockfd( fd ) );

	if ( !msg_exists( fd, msgid ) )
		msg_create( msg_switch, msgid );

	msg_update_status( msg_switch, msgid, SET, MSG_STAT_WRIT_STARTED );
}

void msg_end( int fd, int msgid ) {
	msg_switch_t* msg_switch;
	assert( msg_switch = msg_switch_get_by_sockfd( fd ) );

	msg_t* msg;
	assert( msg = msg_get( msg_switch, msgid ) );
	assert( !FL_ISSET( msg->status, MSG_STAT_WRIT_COMPLETED ) );

	msg_packet_t* packet = NULL;

	if ( msg->incomplete_writ_packet )
		packet = msg->incomplete_writ_packet;
	else if ( !list_size( &msg->writ_queue ) )
		packet = msg_packet_new( msg_switch, msgid,
		  !msg->n_writ ? PACK_FL_FIRST : 0 );
	else
		packet = list_get_at( &msg->writ_queue, list_size( &msg->writ_queue ) - 1 );

	memset( packet->data + packet->header.size, 0, sizeof( packet->data ) - packet->header.size );
	FL_SET( packet->header.flags, PACK_FL_LAST );

	if ( msg->incomplete_writ_packet ) msg_push_writ_packet( msg_switch, msgid );
	msg_update_status( msg_switch, msgid, SET, MSG_STAT_WRIT_COMPLETED );
}

void msg_kill( int fd, int msgid ) {
	msg_switch_t* msg_switch;
	assert( msg_switch = msg_switch_get_by_sockfd( fd ) );

	msg_t* msg;
	assert( msg = msg_get( msg_switch, msgid ) );
	assert( !FL_ISSET( msg->status, MSG_STAT_KILL_SENT ) );

	msg_send_resp( msg_switch, msgid, MSG_RESP_FAIL );
	msg_update_status( msg_switch, msgid, SET, MSG_STAT_KILL_SENT );

	msg_clear( msg_switch, msgid );
	//msg_destroy( msg_switch->sockfd, msgid );
}

void msg_send( int fd, int msgid, void* data, int size ) {
	msg_switch_t* msg_switch;
	assert( msg_switch = msg_switch_get_by_sockfd( fd ) );

	//printf( "pushing %d bytes\n", size );
	msg_t* msg;
	assert( msg = msg_get( msg_switch, msgid ) );
	assert( !FL_ISSET( msg->status, MSG_STAT_WRIT_COMPLETED ) );

	msg_packet_t* packet = NULL;

	if ( !msg->incomplete_writ_packet )
		packet = msg_packet_new( msg_switch, msgid,
		  !msg->n_writ ? PACK_FL_FIRST : 0 );
	else
		packet = msg->incomplete_writ_packet;

	int chunk = 0;
	while ( size ) {

		if ( packet->header.size == PACK_DATA_SIZE )
			packet = msg_packet_new( msg_switch, msgid, 0 );

		if ( size / ( PACK_DATA_SIZE - packet->header.size ) )
			chunk = PACK_DATA_SIZE - packet->header.size;
		else
			chunk = size % ( PACK_DATA_SIZE - packet->header.size );

		memcpy( packet->data + packet->header.size, data, chunk );

		size -= chunk;
		data += chunk;
		packet->header.size += chunk;

	}
}

void msg_destroy( int fd, int msgid ) {
	msg_switch_t* msg_switch;
	assert( msg_switch = msg_switch_get_by_sockfd( fd ) );

	msg_switch_event( msg_switch, MSG_EVT_DESTROY, msgid );

	msg_t* msg = msg_get( msg_switch, msgid );

	msg_clear( msg_switch, msgid );
	list_destroy( &msg->writ_queue );
	list_destroy( &msg->read_queue );

	if ( msg->incomplete_writ_packet )
		free( msg->incomplete_writ_packet );

	free( msg );
	msg_set( msg_switch, msgid, NULL );
}

void msg_want_packet( int fd, int msgid ) {
	msg_switch_t* msg_switch;
	assert( msg_switch = msg_switch_get_by_sockfd( fd ) );

	msg_update_status( msg_switch, msgid, SET, MSG_STAT_WANT_PACK );
}

/////////////////////////////////////////////////////////////////////////////////////
// MSG_SWITCH functions /////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

static void msg_switch_failed( msg_switch_t* msg_switch, int code, const char* info ) {
	printf( "msg_switch_failed() on %d: %d %s\n", msg_switch->sockfd, code, info );

	msg_switch_event( msg_switch, MSG_SWITCH_EVT_IO_FAILED, &code, info );
}

static bool msg_switch_has_pending_msgs( msg_switch_t* msg_switch ) {
	return list_size( &msg_switch->pending_msgs );
}

static bool msg_switch_has_pending_resps( msg_switch_t* msg_switch ) {
	return list_size( &msg_switch->pending_resps );
}

static bool msg_switch_has_avail_writs( msg_switch_t* msg_switch ) {
	if ( msg_switch_has_pending_msgs( msg_switch )
	  || msg_switch_has_pending_resps( msg_switch ) )
		return true;
	return false;
}

static void msg_switch_writ( int fd );

static void msg_switch_push_writ_msg( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg = msg_get( msg_switch, msgid );
	list_append( &msg_switch->pending_msgs, msg );
	if ( !msg_switch->write.type )
		msg_switch_writ( msg_switch->sockfd );
}

static msg_packet_t msg_switch_pop_writ_packet( msg_switch_t* msg_switch ) {
	msg_t* msg = list_fetch( &msg_switch->pending_msgs );
	return msg_pop_writ_packet( msg_switch, msg->msgid );
}

static void msg_switch_push_resp( msg_switch_t* msg_switch, msg_resp_t* resp ) {
	list_append( &msg_switch->pending_resps, resp );
	if ( !msg_switch->write.type )
		msg_switch_writ( msg_switch->sockfd );
}

static msg_resp_t msg_switch_pop_resp( msg_switch_t* msg_switch ) {
	msg_resp_t resp = *(msg_resp_t*) list_get_at( &msg_switch->pending_resps, 0 );
	free( list_fetch( &msg_switch->pending_resps ) );
	return resp;
}

static void msg_switch_writ( int fd ) {
	msg_switch_t* msg_switch;
	assert( msg_switch = msg_switch_get_by_sockfd( fd ) );
	assert( !msg_switch->write.type );

	// check if waiting to send returns
	if ( msg_switch_has_pending_resps( msg_switch ) ) {
		msg_switch->write.type = MSG_TYPE_RESP;
		msg_switch->write.data.resp = msg_switch_pop_resp( msg_switch );
		//printf( "[%d] %04d: sending read response, status %d\n", resp.msgid, n_print++, resp.status );
	}

	// pop from queue and write
	else {
		msg_switch->write.type = MSG_TYPE_PACK;
		msg_switch->write.data.packet = msg_switch_pop_writ_packet( msg_switch );

		//printf( "[%d] %04d: sending packet, flags %x\n", packet.msgid, n_print++, packet.flags );
	}

	zwrite( fd, (void*) &msg_switch->write, MSG_TYPE_GET_SIZE( msg_switch->write.type ) + sizeof( msg_switch->write.type ) );
}

static void msg_switch_onwrit( int fd, void* buff, ssize_t len ) {
	msg_switch_t* msg_switch;
	assert( msg_switch = msg_switch_get_by_sockfd( fd ) );

	if ( len <= 0 ) {
		msg_switch_failed( msg_switch, len, "msg_switch_writ_complete()" );
		return;
	}

	msg_switch->write.type = 0;

	if ( msg_switch_has_avail_writs( msg_switch ) )
		msg_switch_writ( msg_switch->sockfd );
}

static void msg_switch_onread( int fd, void* buff, ssize_t len ) {
	msg_switch_t* msg_switch;
	assert( msg_switch = msg_switch_get_by_sockfd( fd ) );

	if ( len <= 0 ) {
		msg_switch_failed( msg_switch, len, "msg_switch_read_complete()" );
		return;
	}

	while ( len > 0 ) {

		if ( !msg_switch->read.type ) {
			if ( sizeof( msg_switch->read.type ) > len )
				msg_switch_failed( msg_switch, len, "msg_switch_read_complete()" );

			memcpy( &msg_switch->read.type, buff, sizeof( msg_switch->read.type ) );
			buff += sizeof( msg_switch->read.type );
			len  -= sizeof( msg_switch->read.type );
		}

		assert( msg_switch->read.type == MSG_TYPE_RESP || msg_switch->read.type == MSG_TYPE_PACK );

		int len_used = len < MSG_TYPE_GET_SIZE( msg_switch->read.type ) - msg_switch->curr_read_size ? len :
		  MSG_TYPE_GET_SIZE( msg_switch->read.type ) - msg_switch->curr_read_size;

		memcpy( ((void*) &msg_switch->read.data) + msg_switch->curr_read_size, buff, len_used );
		msg_switch->curr_read_size += len_used;
		buff += len_used;

		if ( msg_switch->curr_read_size < MSG_TYPE_GET_SIZE( msg_switch->read.type ) )
			return;

		len -= len_used;

		switch( msg_switch->read.type ) {
			case MSG_TYPE_RESP:
				if ( msg_get( msg_switch, msg_switch->read.data.resp.msgid ) )
					msg_recv_resp( msg_switch, msg_switch->read.data.resp );
				break;
			case MSG_TYPE_PACK:
				if ( !msg_exists( msg_switch->sockfd, msg_switch->read.data.packet.header.msgid ) )
					msg_create( msg_switch, msg_switch->read.data.packet.header.msgid );

				msg_push_read_packet( msg_switch, msg_switch->read.data.packet.header.msgid, &msg_switch->read.data.packet );
				break;
		}

		msg_switch->read.type = 0;
		msg_switch->curr_read_size = 0;
	}
}


static void msg_zsocket_event_hdlr( int fd, zsocket_event_t event ) {
	switch ( event.type ) {
		case ZSE_ACCEPT_ERR:
		case ZSE_ACCEPTED_CONNECTION:
			fprintf( stderr, "Should not be accepting on sock #%d\n", fd );
			break;
		case ZSE_READ_DATA:
			msg_switch_onread( fd, event.data.read.buffer, event.data.read.buffer_used );
			break;
		case ZSE_WROTE_DATA:
			//fprintf( stderr, "Wrote %d bytes of data\n", (int)event.data.write.buffer_used );
			msg_switch_onwrit( fd, event.data.write.buffer, event.data.write.buffer_used );
			break;
	}
}

// External MSG functions /////////////////////////////////////////////////////////////

void msg_switch_create( int fd, void (*event_handler)( struct msg_switch*, msg_event_t event ) ) {
	assert( !msg_switch_get_by_sockfd( fd ) );

	msg_switch_t* msg_switch = (msg_switch_t*) malloc( sizeof( msg_switch_t ) );
	memset( msg_switch->msgs, 0, sizeof( msg_switch->msgs ) );
	list_init( &msg_switch->pending_resps );
	list_init( &msg_switch->pending_msgs );
	msg_switch->sockfd = fd;
	msg_switch->read.type = 0;
	msg_switch->curr_read_size = 0;
	msg_switch->write.type = 0;
	msg_switch->event_handler = event_handler;

	zsocket_set_event_hdlr( fd, msg_zsocket_event_hdlr );
	zread( fd, true );

	zsocket_t* zs = zsocket_get_by_sockfd( fd );
	zs->general.udata = msg_switch;

	msg_switch_event( msg_switch, MSG_SWITCH_EVT_NEW );
}

void msg_switch_destroy( int fd ) {
	msg_switch_t* msg_switch;
	assert( msg_switch = msg_switch_get_by_sockfd( fd ) );

	msg_switch_event( msg_switch, MSG_SWITCH_EVT_DESTROY );

	while ( list_size( &msg_switch->pending_resps ) )
		free( list_fetch( &msg_switch->pending_resps ) );
	list_destroy( &msg_switch->pending_resps );

	//while ( list_fetch( &msg_switch->pending_msgs ) );
	list_destroy( &msg_switch->pending_msgs );

	int i;
	for ( i = 0; i < FD_SETSIZE * 2; i++ ) {
		if ( msg_switch->msgs[ i ] ) {
			msg_destroy( msg_switch->sockfd, i - FD_SETSIZE );
		}
	}

	zpause( fd, true );
	//zclose( msg_switch->sockfd );
	free( msg_switch );
	zsocket_t* zs = zsocket_get_by_sockfd( fd );
	zs->general.udata = NULL;
}

static void msg_update_status( msg_switch_t* msg_switch, int msgid, char type, int status ) {
	int sockfd = msg_switch->sockfd;
	msg_t* msg = msg_get( msg_switch, msgid );

	bool unset_working = false;
	if ( !FL_ISSET( msg->status, MSG_STAT_WORKING ) ) {
		FL_SET( msg->status, MSG_STAT_WORKING );
		unset_working = true;
	}

	if ( type == SET && !FL_ISSET( msg->status, status ) ) {
		FL_SET( msg->status, status );

		switch ( status ) {
			case MSG_STAT_NEW:
				msg_switch_event( msg_switch, MSG_EVT_NEW, msgid );
				msg_update_status( msg_switch, msgid, SET, MSG_STAT_SPACE_AVAIL_FOR_READ );
				break;

			case MSG_STAT_READ_STARTED:
				break;

			case MSG_STAT_WRIT_STARTED:
				msg_update_status( msg_switch, msgid, SET, MSG_STAT_SPACE_AVAIL_FOR_WRIT );
				break;

			case MSG_STAT_READ_COMPLETED:
				if ( !FL_ISSET( msg->status, MSG_STAT_PACKET_AVAIL_TO_READ ) )
					msg_switch_event( msg_switch, MSG_EVT_RECVD, msgid );
				if ( msg_exists( msg_switch->sockfd, msgid ) && FL_ISSET( msg->status, MSG_STAT_WRIT_COMPLETED ) )
					msg_update_status( msg_switch, msgid, SET, MSG_STAT_FINISHED );
				break;

			case MSG_STAT_WRIT_COMPLETED:
				if ( !FL_ISSET( msg->status, MSG_STAT_PACKET_AVAIL_TO_WRIT ) )
					msg_switch_event( msg_switch, MSG_EVT_SENT, msgid );
				if ( msg_exists( msg_switch->sockfd, msgid ) && FL_ISSET( msg->status, MSG_STAT_READ_COMPLETED ) )
					msg_update_status( msg_switch, msgid, SET, MSG_STAT_FINISHED );
				break;

			case MSG_STAT_FINISHED:
check_to_destroy:
				if ( FL_ISSET( msg->status, MSG_STAT_FINISHED )
				  && !FL_ISSET( msg->status, MSG_STAT_PACKET_AVAIL_TO_READ )
				  && !FL_ISSET( msg->status, MSG_STAT_PACKET_AVAIL_TO_WRIT )
				  && !FL_ISSET( msg->status, MSG_STAT_NEED_TO_SEND_RESP )
				  && !FL_ISSET( msg->status, MSG_STAT_WAITING_FOR_RESP )
				  && !FL_ISSET( msg->status, MSG_STAT_WORKING ) )
					msg_destroy( msg_switch->sockfd, msgid );
				break;

			case MSG_STAT_PACKET_AVAIL_TO_READ:
			case MSG_STAT_WANT_PACK:
				if ( FL_ISSET( msg->status, MSG_STAT_PACKET_AVAIL_TO_READ | MSG_STAT_WANT_PACK ) ) {
					msg_update_status( msg_switch, msgid, CLR, MSG_STAT_WANT_PACK );
					msg_packet_t packet = msg_pop_read_packet( msg_switch, msgid );
					msg_switch_event( msg_switch, MSG_EVT_RECVD_PACKET, &packet );
				}
				break;

			case MSG_STAT_PACKET_AVAIL_TO_WRIT:
				if ( !FL_ISSET( msg->status, MSG_STAT_WAITING_FOR_RESP ) ) {
					 msg_switch_push_writ_msg( msg_switch, msgid );
				}
				break;

			case MSG_STAT_SPACE_AVAIL_FOR_READ:
			case MSG_STAT_NEED_TO_SEND_RESP:
				if ( FL_ISSET( msg->status, MSG_STAT_NEED_TO_SEND_RESP | MSG_STAT_SPACE_AVAIL_FOR_READ ) )
					msg_send_resp( msg_switch, msgid, MSG_RESP_OK );
				break;

			case MSG_STAT_SPACE_AVAIL_FOR_WRIT:
				if ( !FL_ISSET( msg->status, MSG_STAT_WRIT_COMPLETED ) )
					msg_switch_event( msg_switch, MSG_EVT_SPACE_AVAIL, msgid );
				break;

			case MSG_STAT_WAITING_FOR_RESP:
				//msg_update_status( msg_switch, msgid, CLR, MSG_STAT_WRITE_PACK );
				break;
		}
	}

	else if ( type == CLR && FL_ISSET( msg->status, status ) ) {
		FL_CLR( msg->status, status );

		switch ( status ) {
			case MSG_STAT_WRIT_STARTED:
			case MSG_STAT_WRIT_COMPLETED:
			case MSG_STAT_READ_STARTED:
			case MSG_STAT_READ_COMPLETED:
				abort(); // These should never be cleared

			case MSG_STAT_NEW:
				break;

			case MSG_STAT_PACKET_AVAIL_TO_READ:
				if ( FL_ISSET( msg->status, MSG_STAT_READ_COMPLETED ) )
					msg_switch_event( msg_switch, MSG_EVT_RECVD, msgid );
				goto check_to_destroy;

			case MSG_STAT_PACKET_AVAIL_TO_WRIT:
				if ( FL_ISSET( msg->status, MSG_STAT_WRIT_COMPLETED ) )
					msg_switch_event( msg_switch, MSG_EVT_SENT, msgid );
				goto check_to_destroy;

			case MSG_STAT_SPACE_AVAIL_FOR_READ:
				break;

			case MSG_STAT_SPACE_AVAIL_FOR_WRIT:
				msg_switch_event( msg_switch, MSG_EVT_SPACE_FULL, msgid );
				break;

			case MSG_STAT_WAITING_FOR_RESP:
				if ( FL_ISSET( msg->status, MSG_STAT_PACKET_AVAIL_TO_WRIT ) ) {
					 msg_switch_push_writ_msg( msg_switch, msgid );
				} else
					goto check_to_destroy;
				break;

			case MSG_STAT_NEED_TO_SEND_RESP:
				goto check_to_destroy;

			case MSG_STAT_WANT_PACK:
				break;
		}
	}

	if ( unset_working ) {
		unset_working = false;
		FL_CLR( msg->status, MSG_STAT_WORKING );
		goto check_to_destroy;
	}

}

static void msg_switch_event( msg_switch_t* msg_switch, int type, ... ) {
	if ( !msg_switch->event_handler ) return;

	va_list ap;

	msg_event_t event;
	memset( &event, 0, sizeof( event ) );
	event.type = type;

	va_start( ap, type );
	switch ( event.type ) {
		case MSG_EVT_NEW:
		case MSG_EVT_SENT:
		case MSG_EVT_RECVD:
		case MSG_EVT_DESTROY:
		case MSG_EVT_SPACE_FULL:
		case MSG_EVT_SPACE_AVAIL:
			event.data.msgid = va_arg( ap, int );
			break;
		case MSG_EVT_RECVD_PACKET:
			event.data.packet = va_arg( ap, msg_packet_t* );
			break;
		case MSG_SWITCH_EVT_IO_FAILED:
		case MSG_SWITCH_EVT_NEW:
		case MSG_SWITCH_EVT_DESTROY:
			break;
		//case MSG_EVT_KILL:
		//	break;
		default:
			fprintf( stderr, "msg_switch_event: passed undefined event %d\n", event.type );
			goto quit;
	}

	msg_switch->event_handler( msg_switch, event );

quit:
	va_end( ap );
}
