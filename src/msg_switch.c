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

static bool initialized = false;

static int read_type = 0, writ_type = 0;

static msg_t* msg_get( msg_switch_t* msg_switch, int msgid ) {
	return msg_switch->msgs[ msgid + FD_SETSIZE ];
}

static void msg_set( msg_switch_t* msg_switch, int msgid, msg_t* msg ) {
	msg_switch->msgs[ msgid + FD_SETSIZE ] = msg;
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
		case MSG_EVT_DESTROY:
		case MSG_EVT_COMPLETE:
		case MSG_EVT_SENT:
		case MSG_RECV_EVT_KILL:
		case MSG_RECV_EVT_LAST:
		case MSG_RECV_EVT_FIRST:
		case MSG_EVT_BUF_FULL:
		case MSG_EVT_BUF_EMPTY:
			event.data.msgid = va_arg( ap, int );
			break;
		case MSG_RECV_EVT_RESP:
			event.data.resp = va_arg( ap, msg_packet_resp_t* );
			break;
		case MSG_RECV_EVT_PACK:
			event.data.packet = va_arg( ap, msg_packet_t* );
			break;
		case MSG_SWITCH_EVT_NEW:
		case MSG_SWITCH_EVT_DESTROY:
		case MSG_SWITCH_EVT_IO_FAILED:
			break;
		default:
			fprintf( stderr, "msg_switch_event: passed undefined event %d\n", event.type );
			goto quit;
	}

	msg_switch->event_handler( msg_switch, event );

quit:
	va_end( ap );
}

static void msg_destroy( msg_switch_t* msg_switch, int msgid ) {
	msg_switch_event( msg_switch, MSG_EVT_DESTROY, msgid );

	msg_t* msg = msg_get( msg_switch, msgid );
	while ( list_size( &msg->queue ) )
		free( list_fetch( &msg->queue ) );
	list_destroy( &msg->queue );
	free( msg );
	msg_set( msg_switch, msgid, NULL );
}

static void msg_update_status( msg_switch_t* msg_switch, int msgid, char type, int status ) {
	msg_t* msg = msg_get( msg_switch, msgid );

	if ( type == SET && !FL_ISSET( msg->status, status ) ) {
		FL_SET( msg->status, status );

		switch ( status ) {
			case MSG_STAT_NEW:
				msg_switch_event( msg_switch, MSG_EVT_NEW, msgid );
				break;

			case MSG_STAT_WRITE_PACK:
				list_append( &msg_switch->pending_msgs, msg );
				zfd_set( msg_switch->sockfd, writ_type, msg_switch );
				break;

			case MSG_STAT_PENDING_PACKS:
				msg_switch_event( msg_switch, MSG_EVT_BUF_FULL, msgid );
				if ( !FL_ISSET( msg->status, MSG_STAT_READ_RESP ) )
					msg_update_status( msg_switch, msgid, SET, MSG_STAT_WRITE_PACK );
				break;

			case MSG_STAT_READ_RESP:
				msg_update_status( msg_switch, msgid, CLR, MSG_STAT_WRITE_PACK );
				break;

			case MSG_STAT_COMPLETE:
				msg_switch_event( msg_switch, MSG_EVT_COMPLETE, msgid );
				if ( !FL_ISSET( msg->status, MSG_STAT_PENDING_PACKS )
				  && !FL_ISSET( msg->status, MSG_STAT_READ_RESP ) )
					msg_destroy( msg_switch, msgid );
				break;
		}
	}

	else if ( type == CLR && FL_ISSET( msg->status, status ) ) {
		FL_CLR( msg->status, status );

		switch ( status ) {
			case MSG_STAT_NEW:
				break;

			case MSG_STAT_WRITE_PACK:
				break;

			case MSG_STAT_PENDING_PACKS:
				msg_update_status( msg_switch, msgid, CLR, MSG_STAT_WRITE_PACK );

				if ( FL_ISSET( msg->status, MSG_STAT_COMPLETE )
				 && !FL_ISSET( msg->status, MSG_STAT_READ_RESP ) )
					msg_destroy( msg_switch, msgid );
				else if ( !FL_ISSET( msg->status, MSG_STAT_COMPLETE ) )
					msg_switch_event( msg_switch, MSG_EVT_BUF_EMPTY, msgid );
				break;

			case MSG_STAT_READ_RESP:
				if ( FL_ISSET( msg->status, MSG_STAT_PENDING_PACKS ) )
					msg_update_status( msg_switch, msgid, SET, MSG_STAT_WRITE_PACK );
				else if ( FL_ISSET( msg->status, MSG_STAT_COMPLETE ) ) {
					msg_switch_event( msg_switch, MSG_EVT_SENT, msgid );
					msg_destroy( msg_switch, msgid );
				}
				break;

			case MSG_STAT_COMPLETE:
				break;
		}
	}

}

static void msg_switch_failed( msg_switch_t* msg_switch, int code, const char* info ) {
	//printf( "msg_switch_failed(): %d %s\n", code, info );
	zfd_clr( msg_switch->sockfd, read_type );
	zfd_clr( msg_switch->sockfd, writ_type );

	msg_switch_event( msg_switch, MSG_SWITCH_EVT_IO_FAILED, &code, info );
}

static void msg_switch_read( int sockfd, msg_switch_t* msg_switch ) {
	char type;
	msg_packet_resp_t resp;

	int n;

	if ( msg_switch->read_packet_size )
		type = MSG_TYPE_PACK;

	else if ( read( sockfd, &type, sizeof( type ) ) != sizeof( type ) ) {
		msg_switch_failed( msg_switch, 0, "msg_switch_read(): read type" );
		return;
	}

	switch( type ) {
		case MSG_TYPE_RESP:
			if ( read( sockfd, &resp, sizeof( resp ) ) != sizeof( resp ) ) {
				msg_switch_failed( msg_switch, 0, "msg_switch_read(): read resp" );
				return;
			}
			//printf( "[%d] %04d: got response, status %d\n", resp.msgid, n_print++, resp.status );

			if ( msg_get( msg_switch, resp.msgid ) ) {

				msg_switch_event( msg_switch, MSG_RECV_EVT_RESP, &resp );

				msg_accept_resp( msg_switch, resp );
			}
			break;

		case MSG_TYPE_PACK:
			n = read( sockfd, &msg_switch->read_packet + msg_switch->read_packet_size, sizeof( msg_packet_t ) - msg_switch->read_packet_size );
			if ( n <= 0 ) {
				msg_switch->read_packet_size = 0;
				msg_switch_failed( msg_switch, 0, "msg_switch_read(): read packet" );
				return;
			}

			msg_switch->read_packet_size += n;
			if ( msg_switch->read_packet_size == sizeof( msg_packet_t ) ) {
				//printf( "[%d] %04d: got complete packet, flags %x\n", msg_switch->read_packet.msgid, n_print++, msg_switch->read_packet.flags );
				msg_switch->read_packet_size = 0;

				if ( !FL_ISSET( msg_switch->read_packet.flags, PACK_FL_KILL ) ) {
					if ( FL_ISSET( msg_switch->read_packet.flags, PACK_FL_FIRST ) )
						msg_switch_event( msg_switch, MSG_RECV_EVT_FIRST, msg_switch->read_packet.msgid );

					msg_switch_event( msg_switch, MSG_RECV_EVT_PACK, &msg_switch->read_packet );

				} else
					msg_switch_event( msg_switch, MSG_RECV_EVT_KILL, msg_switch->read_packet.msgid );

			}// else
			//	printf( "[%d] %04d: got partial packet\n", msg_switch->read_packet.msgid, n_print++ );
			break;
	}
}

static void msg_switch_writ( int sockfd, msg_switch_t* msg_switch ) {
	char type;
	int size, n;
	void* data;

	msg_packet_resp_t resp;

	if ( !msg_switch->write_packet_size ) {
		// check if waiting to send returns
		if ( msg_switch_has_pending_resps( msg_switch ) ) {
			type = MSG_TYPE_RESP;
			resp = msg_switch_pop_resp( msg_switch );
			size = sizeof( resp );
			data = &resp;
			//printf( "[%d] %04d: sending read response, status %d\n", resp.msgid, n_print++, resp.status );
		}

		// pop from queue and write
		else {
			type = MSG_TYPE_PACK;
			msg_switch->write_packet = msg_switch_pop_packet( msg_switch );
			msg_switch->write_packet_size = sizeof( msg_switch->write_packet );
			//printf( "[%d] %04d: sending packet, flags %x\n", packet.msgid, n_print++, packet.flags );
		}

		if ( write( sockfd, &type, sizeof( type ) ) != sizeof( type ) ) {
			msg_switch_failed( msg_switch, 0, "msg_switch_writ(): write type" );
			return;
		}
	}

	else type = MSG_TYPE_PACK;

	if ( type == MSG_TYPE_PACK ) {
		size = msg_switch->write_packet_size;
		data = (&msg_switch->write_packet) + ( sizeof( msg_switch->write_packet ) - size );
	}

	n = write( sockfd, data, size );
	if ( n <= 0 || ( n != size && type == MSG_TYPE_RESP ) ) {
		msg_switch_failed( msg_switch, 0, "msg_switch_writ(): write data" );
		return;
	}

	else if ( type == MSG_TYPE_PACK ) {
		msg_switch->write_packet_size -= n;
	}

	if ( !msg_switch->write_packet_size && !msg_switch_is_pending( msg_switch ) ) {
		zfd_clr( sockfd, writ_type );
	}
}

static msg_packet_t* msg_packet_new( msg_switch_t* msg_switch, int msgid, int flags ) {
	msg_packet_t* packet = (msg_packet_t*) malloc( sizeof( msg_packet_t ) );

	packet->msgid = msgid;
	packet->size = 0;
	packet->flags = flags;

	list_append( &msg_get( msg_switch, msgid )->queue, packet );

	msg_update_status( msg_switch, msgid, CLR, MSG_STAT_NEW );
	return packet;
}

static bool msg_has_available_packets( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg = msg_get( msg_switch, msgid );
	return ( list_size( &msg->queue ) && ( ((msg_packet_t*)list_get_at( &msg->queue, 0 ))->size == PACK_DATA_SIZE
	  || FL_ISSET(((msg_packet_t*)list_get_at( &msg->queue, 0 ))->flags, PACK_FL_LAST ) ) );
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
	void (*event_handler)( struct msg_switch*, msg_event_t event )
 ) {
	assert( initialized );

	msg_switch_t* msg_switch = (msg_switch_t*) malloc( sizeof( msg_switch_t ) );
	memset( msg_switch->msgs, 0, sizeof( msg_switch->msgs ) );
	list_init( &msg_switch->pending_resps );
	list_init( &msg_switch->pending_msgs );
	msg_switch->sockfd = sockfd;
	msg_switch->read_packet_size = 0;
	msg_switch->write_packet_size = 0;
	msg_switch->event_handler = event_handler;

	zfd_set( msg_switch->sockfd, read_type, msg_switch );

	msg_switch_event( msg_switch, MSG_SWITCH_EVT_NEW );

	return msg_switch;
}

void msg_switch_destroy( msg_switch_t* msg_switch ) {

	msg_switch_event( msg_switch, MSG_SWITCH_EVT_DESTROY );

	while ( list_size( &msg_switch->pending_resps ) )
		free( list_fetch( &msg_switch->pending_resps ) );
	list_destroy( &msg_switch->pending_resps );

	while ( list_fetch( &msg_switch->pending_msgs ) );
	list_destroy( &msg_switch->pending_msgs );

	int i;
	for ( i = 0; i < FD_SETSIZE * 2; i++ ) {
		if ( msg_switch->msgs[ i ] ) {
			msg_destroy( msg_switch, i - FD_SETSIZE );
		}
	}

	zfd_clr( msg_switch->sockfd, read_type );
	zfd_clr( msg_switch->sockfd, writ_type );
	free( msg_switch );
}

void msg_switch_send_pack_resp( msg_switch_t* msg_switch, msg_packet_t* packet, char status ) {
	msg_packet_resp_t* resp = (msg_packet_resp_t*) malloc( sizeof( msg_packet_resp_t ) );
	memset( resp, 0, sizeof( msg_packet_resp_t ) ); // initializes all bytes including padded bytes

	resp->msgid = packet->msgid;
	resp->status = status;

	list_append( &msg_switch->pending_resps, resp );
	//printf( "ready to write resps\n" );
	// add write to zfd and fire event
	zfd_set( msg_switch->sockfd, writ_type, msg_switch );

	if ( FL_ISSET( packet->flags, PACK_FL_LAST ) )
		msg_switch_event( msg_switch, MSG_RECV_EVT_LAST, packet->msgid );
}

msg_packet_t msg_switch_pop_packet( msg_switch_t* msg_switch ) {
	msg_t* msg = list_fetch( &msg_switch->pending_msgs );

	msg_packet_t packet = *(msg_packet_t*) list_get_at( &msg->queue, 0 );
	free( list_fetch( &msg->queue ) );

	if ( !FL_ISSET( packet.flags, PACK_FL_KILL ) )
		msg_update_status( msg_switch, msg->msgid, SET, MSG_STAT_READ_RESP );

	if ( !msg_has_available_packets( msg_switch, msg->msgid ) )
		msg_update_status( msg_switch, msg->msgid, CLR, MSG_STAT_PENDING_PACKS );

	return packet;
}

msg_packet_resp_t msg_switch_pop_resp( msg_switch_t* msg_switch ) {
	msg_packet_resp_t resp = *(msg_packet_resp_t*) list_get_at( &msg_switch->pending_resps, 0 );
	free( list_fetch( &msg_switch->pending_resps ) );
	return resp;
}

bool msg_switch_is_pending( msg_switch_t* msg_switch ) {
	if ( msg_switch_has_pending_msgs( msg_switch )
	  || msg_switch_has_pending_resps( msg_switch ) )
		return true;
	return false;
}

bool msg_switch_has_pending_msgs( msg_switch_t* msg_switch ) {
	return list_size( &msg_switch->pending_msgs );
}

bool msg_switch_has_pending_resps( msg_switch_t* msg_switch ) {
	return list_size( &msg_switch->pending_resps );
}

void msg_new( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg = (msg_t*) malloc( sizeof( msg_t ) );

	msg->msgid = msgid;
	msg->status = 0;
	list_init( &msg->queue );

	msg_set( msg_switch, msgid, msg );

	msg_update_status( msg_switch, msgid, SET, MSG_STAT_NEW );
}

void msg_end( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg = msg_get( msg_switch, msgid );
	msg_packet_t* packet = NULL;

	assert( !FL_ISSET( msg->status, MSG_STAT_COMPLETE ) );

	if ( !list_size( &msg->queue ) ) {
		packet = msg_packet_new( msg_switch, msgid,
		  FL_ISSET( msg->status, MSG_STAT_NEW ) ? PACK_FL_FIRST : 0 );
	} else
		packet = list_get_at( &msg->queue, list_size( &msg->queue ) - 1 );

	memset( packet->data + packet->size, 0, sizeof( packet->data ) - packet->size );
	FL_SET( packet->flags, PACK_FL_LAST );

	msg_update_status( msg_switch, msgid, SET, MSG_STAT_PENDING_PACKS );
	msg_update_status( msg_switch, msgid, SET, MSG_STAT_COMPLETE );
}

void msg_kill( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg = msg_get( msg_switch, msgid );
	msg_packet_t* packet = NULL;

	if ( FL_ISSET( msg->status, MSG_STAT_NEW )
	  || ( list_size( &msg->queue ) == 1
		 && PACK_IS_FIRST( list_get_at( &msg->queue, 0 ) ) ) ) {
		// Nothing has been sent yet
		msg_destroy( msg_switch, msgid );
	}

	else {
		while( list_size( &msg->queue ) )
			free( list_fetch( &msg->queue ) );
		packet = msg_packet_new( msg_switch, msgid, PACK_FL_LAST | PACK_FL_KILL );

		msg_update_status( msg_switch, msgid, SET, MSG_STAT_PENDING_PACKS );
		msg_update_status( msg_switch, msgid, SET, MSG_STAT_COMPLETE );
	}
}

void msg_accept_resp( msg_switch_t* msg_switch, msg_packet_resp_t resp ) {
	if ( resp.status == MSG_PACK_RESP_FAIL ) {
		msg_destroy( msg_switch, resp.msgid );
	}

	else
		msg_update_status( msg_switch, resp.msgid, CLR, MSG_STAT_READ_RESP );
}

void msg_push_data( msg_switch_t* msg_switch, int msgid, void* data, int size ) {
	//printf( "pushing %d bytes\n", size );
	msg_t* msg = msg_get( msg_switch, msgid );
	msg_packet_t* packet = NULL;

	if ( !list_size( &msg->queue ) ) {
		packet = msg_packet_new( msg_switch, msgid,
		  FL_ISSET( msg->status, MSG_STAT_NEW ) ? PACK_FL_FIRST : 0 );
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

	if ( msg_has_available_packets( msg_switch, msgid ) )
		msg_update_status( msg_switch, msgid, SET, MSG_STAT_PENDING_PACKS );
}

bool msg_has_pending_packets( msg_switch_t* msg_switch, int msgid ) {
	return FL_ISSET( msg_get( msg_switch, msgid )->status, MSG_STAT_PENDING_PACKS );
}

bool msg_is_complete( msg_switch_t* msg_switch, int msgid ) {
	return FL_ISSET( msg_get( msg_switch, msgid )->status, MSG_STAT_COMPLETE );
}

bool msg_exists( msg_switch_t* msg_switch, int msgid ) {
	return msg_get( msg_switch, msgid );
}
