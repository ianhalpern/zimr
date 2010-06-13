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

//static int n_print = 0;

static int n_selectable = 0;

static void msg_update_status( msg_switch_t* msg_switch, int msgid );
static void msg_switch_push_resp( msg_switch_t* msg_switch, msg_resp_t* resp );
static void msg_switch_push_writ_msg( msg_switch_t* msg_switch, int msgid );

msg_switch_t* msg_switch_get_by_fd( int sockfd ) {
	zsocket_t* zs = zs_get_by_fd( sockfd );
	return zs ? (msg_switch_t*) zs->general.udata : NULL;
}

/////////////////////////////////////////////////////////////////////////////////////
// MSG functions ////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

static msg_t* msg_get( msg_switch_t* msg_switch, int msgid ) {
	return msg_switch->msgs[ msgid ];
}

static void msg_set( msg_switch_t* msg_switch, int msgid, msg_t* msg ) {
	msg_switch->msgs[ msgid ] = msg;
}

static bool msg_has_available_writ_packets( msg_switch_t* msg_switch, int msgid, int n ) {
	msg_t* msg = msg_get( msg_switch, msgid );
	return ( list_size( &msg->writ_queue ) >= n );
}

static bool msg_has_available_read_packets( msg_switch_t* msg_switch, int msgid, int n ) {
	msg_t* msg = msg_get( msg_switch, msgid );
	return ( list_size( &msg->read_queue ) >= n );
}

static msg_t* msg_create( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg = (msg_t*) malloc( sizeof( msg_t ) );

	msg->fd = msg_switch->sockfd;
	msg->msgid = msgid;
	msg->status = MSG_STAT_SPACE_AVAIL_FOR_READ | MSG_STAT_SPACE_AVAIL_FOR_WRIT;
	msg->n_writ = 0;
	msg->n_read = 0;
	msg->cur_read_packet_pos = 0;
	msg->incomplete_writ_packet = NULL;
	list_init( &msg->read_queue );
	list_init( &msg->writ_queue );

	msg_set( msg_switch, msgid, msg );

	n_selectable++;

	return msg;
}

static void msg_clear( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg = msg_get( msg_switch, msgid );

	if ( !FL_ISSET( msg->status, MSG_STAT_CONNECTED ) )
		n_selectable--;

	if ( FD_ISSET( msgid, &msg_switch->active_read_fd_set ) && FL_ISSET( msg->status, MSG_STAT_PACKET_AVAIL_TO_READ ) )
		n_selectable--;

	if ( FD_ISSET( msgid, &msg_switch->active_write_fd_set ) && FL_ISSET( msg->status, MSG_STAT_SPACE_AVAIL_FOR_WRIT ) )
		n_selectable--;

	FL_CLR( msg->status,
		MSG_STAT_PACKET_AVAIL_TO_READ|
		MSG_STAT_PACKET_AVAIL_TO_WRIT|
		MSG_STAT_SPACE_AVAIL_FOR_READ|
		MSG_STAT_SPACE_AVAIL_FOR_WRIT|
		MSG_STAT_WAITING_FOR_RESP    |
		MSG_STAT_NEED_TO_SEND_RESP
	);

	while ( list_size( &msg->writ_queue ) )
		free( list_fetch( &msg->writ_queue ) );

	while ( list_size( &msg->read_queue ) )
		free( list_fetch( &msg->read_queue ) );

	int i=0;
	if ( FL_ISSET( msg->status, MSG_STAT_WRITING ) ) {
		if ( ( i = list_locate( &msg_switch->pending_msgs, msg ) ) >= 0 )
			list_delete_at( &msg_switch->pending_msgs, i );
	}
}

static void msg_free( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg = msg_get( msg_switch, msgid );

	msg_clear( msg_switch, msgid );

	list_destroy( &msg->writ_queue );
	list_destroy( &msg->read_queue );

	if ( msg->incomplete_writ_packet )
		free( msg->incomplete_writ_packet );

	free( msg );
	msg_set( msg_switch, msgid, NULL );
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

	if ( status == MSG_RESP_OK ) FL_CLR( msg->status, MSG_STAT_NEED_TO_SEND_RESP );
}

static void msg_recv_resp( msg_switch_t* msg_switch, int msgid, msg_resp_t resp ) {
	msg_t* msg;
	assert( msg = msg_get( msg_switch, msgid ) );

	if ( resp.status == MSG_RESP_DISCON ) {
		if ( !FL_ISSET( msg->status, MSG_STAT_CLOSED ) ) {
			// if wasn't the sender of the disconnect must return a disconnect
			msg_send_resp( msg_switch, msgid, MSG_RESP_DISCON );
			FL_SET( msg->status, MSG_STAT_DISCONNECTED );
			msg_clear( msg_switch, msgid );
			n_selectable++;
		} else msg_free( msg_switch, msgid );
		return;
	}

	assert( FL_ISSET( msg->status, MSG_STAT_WAITING_FOR_RESP ) );

	FL_CLR( msg->status, MSG_STAT_WAITING_FOR_RESP );

	if ( !FL_ISSET( msg->status, MSG_STAT_PACKET_AVAIL_TO_WRIT ) ) {
	// If nothing has become available since last write, flush whatever is available
		msg_flush( msg_switch->sockfd, msgid );
		if ( !FL_ISSET( msg->status, MSG_STAT_PACKET_AVAIL_TO_WRIT ) && FL_ISSET( msg->status, MSG_STAT_CLOSED ) )
			msg_send_resp( msg_switch, msgid, MSG_RESP_DISCON );
	}

	msg_update_status( msg_switch, msgid );
}

static void msg_push_writ_packet( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg;
	assert( msg = msg_get( msg_switch, msgid ) );
	assert( msg->incomplete_writ_packet );

	//printf( "push writ packet %d %d\n", msg_switch->sockfd, msgid );
	//if( FL_ISSET( msg->status, MSG_STAT_DESTROY_SENT ) ) return;
	//msg_update_status( msg_switch, msgid, CLR, MSG_STAT_NEW );

	list_append( &msg->writ_queue, msg->incomplete_writ_packet );
	msg->incomplete_writ_packet = NULL;

	FL_SET( msg->status, MSG_STAT_PACKET_AVAIL_TO_WRIT );

	if ( msg_has_available_writ_packets( msg_switch, msgid, MSG_N_BUFF_PACKETS_ALLOWED ) ) {

		if ( FD_ISSET( msgid, &msg_switch->active_write_fd_set ) && FL_ISSET( msg->status, MSG_STAT_SPACE_AVAIL_FOR_WRIT ) )
			n_selectable--;

		FL_CLR( msg->status, MSG_STAT_SPACE_AVAIL_FOR_WRIT );
	}

	msg_update_status( msg_switch, msgid );
}

static msg_packet_t msg_pop_writ_packet( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg;
	assert( msg = msg_get( msg_switch, msgid ) );

	//printf( "pop writ packet %d %d, %d packets \n", msg_switch->sockfd, msgid, list_size( &msg->writ_queue ) );
	msg_packet_t packet = *(msg_packet_t*) list_get_at( &msg->writ_queue, 0 );
	free( list_fetch( &msg->writ_queue ) );

	FL_SET( msg->status, MSG_STAT_WAITING_FOR_RESP );
	FL_CLR( msg->status, MSG_STAT_WRITING );

	if ( !msg_has_available_writ_packets( msg_switch, msg->msgid, MSG_N_BUFF_PACKETS_ALLOWED ) ) {

		if ( FD_ISSET( msgid, &msg_switch->active_write_fd_set ) && !FL_ISSET( msg->status, MSG_STAT_SPACE_AVAIL_FOR_WRIT ) )
			n_selectable++;

		FL_SET( msg->status, MSG_STAT_SPACE_AVAIL_FOR_WRIT );
	}

	if ( !msg_has_available_writ_packets( msg_switch, msg->msgid, 1 ) ) {
		FL_CLR( msg->status, MSG_STAT_PACKET_AVAIL_TO_WRIT );
	}

	msg_update_status( msg_switch, msgid );
	return packet;
}

static void msg_push_read_packet( msg_switch_t* msg_switch, int msgid, msg_packet_t* packet ) {
	msg_t* msg;
	assert( msg = msg_get( msg_switch, msgid ) );
	msg->n_read++;
	//printf( "push read packet %d %d\n", msg_switch->sockfd, msgid );

	FL_SET( msg->status, MSG_STAT_NEED_TO_SEND_RESP );

	if ( !FL_ISSET( msg->status, MSG_STAT_CLOSED ) )
		list_append( &msg->read_queue, memdup( packet, sizeof( msg_packet_t ) ) );

	if ( FD_ISSET( msgid, &msg_switch->active_read_fd_set ) && !FL_ISSET( msg->status, MSG_STAT_PACKET_AVAIL_TO_READ ) )
		n_selectable++;

	FL_SET( msg->status, MSG_STAT_PACKET_AVAIL_TO_READ );
	if ( msg_has_available_read_packets( msg_switch, msg->msgid, MSG_N_BUFF_PACKETS_ALLOWED ) )
		FL_CLR( msg->status, MSG_STAT_SPACE_AVAIL_FOR_READ );

	msg_update_status( msg_switch, msgid );
}

static msg_packet_t* msg_get_read_packet( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg;
	assert( msg = msg_get( msg_switch, msgid ) );

	return (msg_packet_t*) list_get_at( &msg->read_queue, 0 );
}

static void msg_pop_read_packet( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg = msg_get( msg_switch, msgid );

	free( list_fetch( &msg->read_queue ) );
	//printf( "pop read packet %d %d\n", msg_switch->sockfd, msgid );

	msg->cur_read_packet_pos = 0;

	if ( !msg_has_available_read_packets( msg_switch, msg->msgid, MSG_N_BUFF_PACKETS_ALLOWED ) )
		FL_SET( msg->status, MSG_STAT_SPACE_AVAIL_FOR_READ );
	if ( !msg_has_available_read_packets( msg_switch, msg->msgid, 1 ) ) {

		if ( FD_ISSET( msgid, &msg_switch->active_read_fd_set ) && FL_ISSET( msg->status, MSG_STAT_PACKET_AVAIL_TO_READ ) )
			n_selectable--;

		FL_CLR( msg->status, MSG_STAT_PACKET_AVAIL_TO_READ );
	}

	msg_update_status( msg_switch, msgid );
}

static msg_packet_t* msg_packet_new( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg;
	assert( msg = msg_get( msg_switch, msgid ) );

	if ( msg->incomplete_writ_packet )
		msg_push_writ_packet( msg_switch, msgid );

	msg_packet_t* packet = (msg_packet_t*) malloc( sizeof( msg_packet_t ) );

	packet->header.msgid = msgid;
	packet->header.size = 0;
	msg->incomplete_writ_packet = packet;
	msg->n_writ++;

	return packet;
}

static void msg_update_status( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg;
	assert( msg = msg_get( msg_switch, msgid ) );

	if ( FL_ISSET( msg->status, MSG_STAT_NEED_TO_SEND_RESP | MSG_STAT_SPACE_AVAIL_FOR_READ ) )
		msg_send_resp( msg_switch, msgid, MSG_RESP_OK );

	if ( FL_ISSET( msg->status, MSG_STAT_PACKET_AVAIL_TO_WRIT ) && !FL_ARESOMESET( msg->status, MSG_STAT_WAITING_FOR_RESP | MSG_STAT_WRITING ) ) {
		FL_SET( msg->status, MSG_STAT_WRITING );
		msg_switch_push_writ_msg( msg_switch, msgid );
	}

}

// External MSG functions /////////////////////////////////////////////////////////////
void msg_set_read( int fd, int msgid ) {
	msg_switch_t* msg_switch;
	assert( msg_switch = msg_switch_get_by_fd( fd ) );

	msg_t* msg;
	assert( msg = msg_get( msg_switch, msgid ) );

	if ( !FD_ISSET( msgid, &msg_switch->active_read_fd_set ) && FL_ISSET( msg->status, MSG_STAT_PACKET_AVAIL_TO_READ ) )
		n_selectable++;

	FD_SET( msgid, &msg_switch->active_read_fd_set );
}

void msg_clr_read( int fd, int msgid ) {
	msg_switch_t* msg_switch;
	assert( msg_switch = msg_switch_get_by_fd( fd ) );

	msg_t* msg;
	assert( msg = msg_get( msg_switch, msgid ) );

	if ( FD_ISSET( msgid, &msg_switch->active_read_fd_set ) && FL_ISSET( msg->status, MSG_STAT_PACKET_AVAIL_TO_READ ) )
		n_selectable--;

	FD_CLR( msgid, &msg_switch->active_read_fd_set );
}

bool msg_isset_read( int fd, int msgid ) {
	msg_switch_t* msg_switch;
	assert( msg_switch = msg_switch_get_by_fd( fd ) );

	return FD_ISSET( msgid, &msg_switch->active_read_fd_set );
}

void msg_set_write( int fd, int msgid ) {
	msg_switch_t* msg_switch;
	assert( msg_switch = msg_switch_get_by_fd( fd ) );

	msg_t* msg;
	assert( msg = msg_get( msg_switch, msgid ) );

	if ( !FD_ISSET( msgid, &msg_switch->active_write_fd_set ) && FL_ISSET( msg->status, MSG_STAT_SPACE_AVAIL_FOR_WRIT ) )
		n_selectable++;

	FD_SET( msgid, &msg_switch->active_write_fd_set );
}

void msg_clr_write( int fd, int msgid ) {
	msg_switch_t* msg_switch;
	assert( msg_switch = msg_switch_get_by_fd( fd ) );

	msg_t* msg;
	assert( msg = msg_get( msg_switch, msgid ) );

	if ( FD_ISSET( msgid, &msg_switch->active_write_fd_set ) && FL_ISSET( msg->status, MSG_STAT_SPACE_AVAIL_FOR_WRIT ) )
		n_selectable--;

	FD_CLR( msgid, &msg_switch->active_write_fd_set );
}

bool msg_isset_write( int fd, int msgid ) {
	msg_switch_t* msg_switch;
	assert( msg_switch = msg_switch_get_by_fd( fd ) );

	return FD_ISSET( msgid, &msg_switch->active_write_fd_set );
}

bool msg_exists( int sockfd, int msgid ) {
	msg_switch_t* msg_switch;
	assert( msg_switch = msg_switch_get_by_fd( sockfd ) );

	return msg_get( msg_switch, msgid );
}

int msg_open( int fd, int msgid ) {
	msg_switch_t* msg_switch = msg_switch_get_by_fd( fd );

	if ( !msg_switch ) {
		errno = EINVAL;
		return -1;
	}

	msg_t* msg = msg_get( msg_switch, msgid );
	if ( msg ) {
		errno = EEXIST;
		return -1;
	}

	msg = msg_create( msg_switch, msgid );
	FL_SET( msg->status, MSG_STAT_CONNECTED );
	n_selectable--;

	return 0;
}

int msg_accept( int fd, int msgid ) {
	msg_switch_t* msg_switch = msg_switch_get_by_fd( fd );

	if ( !msg_switch ) {
		errno = EINVAL;
		return -1;
	}

	msg_t* msg = msg_get( msg_switch, msgid );
	if ( !msg || FL_ISSET( msg->status, MSG_STAT_CLOSED ) ) {
		errno = EBADMSG;
		return -1;
	}

	if ( FL_ISSET( msg->status, MSG_STAT_CONNECTED ) ) {
		errno = EEXIST;
		return -1;
	}

	if ( FL_ISSET( msg->status, MSG_STAT_DISCONNECTED ) ) {
		errno = ECONNABORTED;
		msg_free( msg_switch, msgid );
		return -1;
	}

	FL_SET( msg->status, MSG_STAT_CONNECTED );
	n_selectable--;

	return msg->msgid;
}

int msg_close( int fd, int msgid ) {
	msg_switch_t* msg_switch = msg_switch_get_by_fd( fd );

	if ( !msg_switch ) {
		errno = EINVAL;
		return -1;
	}

	msg_t* msg = msg_get( msg_switch, msgid );

	if ( !msg || FL_ISSET( msg->status, MSG_STAT_CLOSED ) ) {
		errno = EBADMSG;
		return -1;
	}

	msg_clr_read( msg_switch->sockfd, msgid );
	msg_clr_write( msg_switch->sockfd, msgid );

	if ( !msg_switch->connected ) {
		msg_free( msg_switch, msgid );
		return 0;
	}

	if ( FL_ISSET( msg->status, MSG_STAT_DISCONNECTED ) || !msg_switch->connected ) {
		msg_free( msg_switch, msgid );
		n_selectable--;
		return 0;
	}

	msg_flush( fd, msgid );

	FL_SET( msg->status, MSG_STAT_CLOSED );

	if ( !FL_ISSET( msg->status, MSG_STAT_PACKET_AVAIL_TO_WRIT ) ) {
		if ( !msg->n_writ && !msg->n_read )
			msg_free( msg_switch, msgid );
		else
			msg_send_resp( msg_switch, msgid, MSG_RESP_DISCON );
	} else while ( FL_ISSET( msg->status, MSG_STAT_PACKET_AVAIL_TO_READ ) )
		msg_pop_read_packet( msg_switch, msgid );

	return 0;
}

int msg_flush( int fd, int msgid ) {
	msg_switch_t* msg_switch = msg_switch_get_by_fd( fd );

	if ( !msg_switch ) {
		errno = EINVAL;
		return -1;
	}

	msg_t* msg = msg_get( msg_switch, msgid );

	if ( !msg || FL_ISSET( msg->status, MSG_STAT_CLOSED ) ) {
		errno = EBADMSG;
		return -1;
	}

	if ( msg->incomplete_writ_packet ) {
		msg_packet_t* packet = msg->incomplete_writ_packet;
		memset( packet->data + packet->header.size, 0, sizeof( packet->data ) - packet->header.size );
		msg_push_writ_packet( msg_switch, msgid );
	}

	return 0;
}

ssize_t msg_write( int fd, int msgid, const void* buf, size_t size ) {
	msg_switch_t* msg_switch = msg_switch_get_by_fd( fd );
	void* data = (void*) buf;

	if ( !msg_switch ) {
		errno = EINVAL;
		return -1;
	}

	msg_t* msg = msg_get( msg_switch, msgid );

	if ( !msg || FL_ISSET( msg->status, MSG_STAT_CLOSED ) ) {
		errno = EBADMSG;
		return -1;
	}

	if ( FL_ISSET( msg->status, MSG_STAT_DISCONNECTED ) ) {
		errno = EPIPE;
		return -1;
	}

	if ( !FL_ISSET( msg->status, MSG_STAT_SPACE_AVAIL_FOR_WRIT ) ) {
		errno = EAGAIN;
		return -1;
	}

	msg_packet_t* packet = NULL;

	if ( !msg->incomplete_writ_packet )
		packet = msg_packet_new( msg_switch, msgid );
	else
		packet = msg->incomplete_writ_packet;

	//printf( "writing %d size to queue on %d %d\n", size, fd, msgid );
	size_t orig_size = size;
	int chunk = 0;
	while ( size ) {
		if ( packet->header.size == PACK_DATA_SIZE )
			packet = msg_packet_new( msg_switch, msgid );

		if ( size > ( PACK_DATA_SIZE - packet->header.size ) )
			chunk = PACK_DATA_SIZE - packet->header.size;
		else
			chunk = size;

		memcpy( packet->data + packet->header.size, data, chunk );

		size -= chunk;
		data += chunk;
		packet->header.size += chunk;

	}

	return orig_size;
}

ssize_t msg_read( int fd, int msgid, void* buf, size_t size ) {
	msg_switch_t* msg_switch = msg_switch_get_by_fd( fd );

	if ( !msg_switch ) {
		errno = EINVAL;
		return -1;
	}

	msg_t* msg = msg_get( msg_switch, msgid );

	if ( !msg || FL_ISSET( msg->status, MSG_STAT_CLOSED ) ) {
		errno = EBADMSG;
		return -1;
	}

	if ( FL_ISSET( msg->status, MSG_STAT_DISCONNECTED ) ) {
		return 0; // EOF
	}

	if ( !FL_ISSET( msg->status, MSG_STAT_PACKET_AVAIL_TO_READ ) ) {
		errno = EAGAIN;
		return -1;
	}

	msg_packet_t* packet = NULL;

	size_t orig_size = size;
	int chunk = 0;
	while ( size ) {

		if ( !packet )
			packet = msg_get_read_packet( msg_switch, msgid );

		if ( packet->header.size - msg->cur_read_packet_pos > size )
			chunk = size;
		else
			chunk = packet->header.size - msg->cur_read_packet_pos;

		memcpy( buf + ( orig_size - size ), packet->data + msg->cur_read_packet_pos, chunk );

		size -= chunk;
		msg->cur_read_packet_pos += chunk;

		if ( msg->cur_read_packet_pos == packet->header.size ) {
			msg_pop_read_packet( msg_switch, msgid );
			packet = NULL;
			if ( !FL_ISSET( msg->status, MSG_STAT_PACKET_AVAIL_TO_READ ) )
				break;

		}
	}

	if ( orig_size - size == 0 )
		fprintf( stderr, "problem %d %d\n", orig_size, size );

	return orig_size - size;
}

/////////////////////////////////////////////////////////////////////////////////////
// MSG_SWITCH functions /////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

static void msg_switch_failed( msg_switch_t* msg_switch, int code, const char* info ) {
	//printf( "msg_switch_failed() on %d: %d %s\n", msg_switch->sockfd, code, info );
	zs_clr_read( msg_switch->sockfd );
	zs_clr_write( msg_switch->sockfd );
	msg_switch->connected = false;
	msg_switch->evt_hdlr( msg_switch->sockfd, code, MSG_SWITCH_EVT_IO_ERROR );
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

static void msg_switch_push_writ_msg( msg_switch_t* msg_switch, int msgid ) {
	msg_t* msg = msg_get( msg_switch, msgid );
	list_append( &msg_switch->pending_msgs, msg );

	zs_set_write( msg_switch->sockfd );
}

static msg_packet_t msg_switch_pop_writ_packet( msg_switch_t* msg_switch ) {
	msg_t* msg = list_fetch( &msg_switch->pending_msgs );
	return msg_pop_writ_packet( msg_switch, msg->msgid );
}

static void msg_switch_push_resp( msg_switch_t* msg_switch, msg_resp_t* resp ) {
	list_append( &msg_switch->pending_resps, resp );

	zs_set_write( msg_switch->sockfd );
}

static msg_resp_t msg_switch_pop_resp( msg_switch_t* msg_switch ) {
	msg_resp_t resp = *(msg_resp_t*) list_get_at( &msg_switch->pending_resps, 0 );
	free( list_fetch( &msg_switch->pending_resps ) );
	return resp;
}

/*static void msg_switch_onwritready( int fd, void* udata ) {
	msg_switch_t* msg_switch;
	assert( msg_switch = msg_switch_get_by_fd( fd ) );
	assert( msg_switch->write.type );
	zfd_clr( fd, ZFD_W );
	printf( "writing %d\n", msg_switch->write.type );
}*/

static void msg_switch_write( int fd ) {
	msg_switch_t* msg_switch;
	assert( msg_switch = msg_switch_get_by_fd( fd ) );

	// check if waiting to send returns
	if ( msg_switch_has_pending_resps( msg_switch ) ) {
		msg_switch->write.type = MSG_TYPE_RESP;
		msg_switch->write.data.resp = msg_switch_pop_resp( msg_switch );
		//printf( "writing resp to socket on %d %d\n", fd, msg_switch->write.data.resp.msgid );
		//printf( "[%d] %04d: sending read response, status %d\n", resp.msgid, n_print++, resp.status );
	}

	// pop from queue and write
	else {
		msg_switch->write.type = MSG_TYPE_PACK;
		msg_switch->write.data.packet = msg_switch_pop_writ_packet( msg_switch );
		//printf( "writing packet to socket on %d %d\n", fd, msg_switch->write.data.packet.header.msgid );

		//printf( "[%d] %04d: sending packet, flags %x\n", packet.msgid, n_print++, packet.flags );
	}

	if ( !msg_switch_has_avail_writs( msg_switch ) )
		zs_clr_write( msg_switch->sockfd );

	//printf( "write: %d\n", msg_switch->write.type );
	//zfd_set( fd, ZFD_W, msg_switch_onwritready, NULL );
	ssize_t n;
	n = zs_write( fd, (void*) &msg_switch->write, MSG_TYPE_GET_SIZE( msg_switch->write.type ) + sizeof( msg_switch->write.type ) );
	if ( n < 0 ) {
		msg_switch_failed( msg_switch, n, "msg_switch_write_complete()" );
		return;
	}
}

static void msg_switch_read( int fd ) {
	msg_switch_t* msg_switch;
	assert( msg_switch = msg_switch_get_by_fd( fd ) );

	char buf[1024],* ptr = buf;
	ssize_t n;

	n = zs_read( fd, buf, 1024 );

	if ( n <= 0 ) {
		msg_switch_failed( msg_switch, n, "msg_switch_read_complete()" );
		return;
	}

	while ( n > 0 ) {

		if ( !msg_switch->read.type ) {
			//if ( n < sizeof( msg_switch->read.type ) )
			//	msg_switch_failed( msg_switch, n, "msg_switch_read_complete()" );

			memcpy( &msg_switch->read.type, ptr, sizeof( msg_switch->read.type ) );
			ptr += sizeof( msg_switch->read.type );
			n   -= sizeof( msg_switch->read.type );
		}

		assert( msg_switch->read.type == MSG_TYPE_RESP || msg_switch->read.type == MSG_TYPE_PACK );

		int len_used = n < MSG_TYPE_GET_SIZE( msg_switch->read.type ) - msg_switch->curr_read_size ? n :
		  MSG_TYPE_GET_SIZE( msg_switch->read.type ) - msg_switch->curr_read_size;

		memcpy( ((void*) &msg_switch->read.data) + msg_switch->curr_read_size, ptr, len_used );
		msg_switch->curr_read_size += len_used;
		ptr += len_used;

		if ( msg_switch->curr_read_size < MSG_TYPE_GET_SIZE( msg_switch->read.type ) )
			return;

		n -= len_used;

		switch( msg_switch->read.type ) {
			case MSG_TYPE_RESP:
				if ( msg_get( msg_switch, msg_switch->read.data.resp.msgid ) )
					msg_recv_resp( msg_switch, msg_switch->read.data.resp.msgid, msg_switch->read.data.resp );
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

static void msg_zsocket_event_hdlr( int fd, int event ) {
	switch ( event ) {
		case ZS_EVT_ACCEPT_READY:
			fprintf( stderr, "Should not be accepting on sock #%d\n", fd );
			break;
		case ZS_EVT_READ_READY:
			msg_switch_read( fd );
			break;
		case ZS_EVT_WRITE_READY:
			//fprintf( stderr, "Wrote %d bytes of data\n", (int)event.data.write.buffer_used );
			msg_switch_write( fd );
			break;
	}
}

// External MSG SWITCH functions /////////////////////////////////////////////////////////////

void msg_switch_toggle_accept( bool toggle ) {
}

int msg_switch_create( int fd, void (*event_handler)( int fd, int msgid, int event ) ) {
	if ( msg_switch_get_by_fd( fd ) ) {
		errno = EEXIST;
		return -1;
	}

	zsocket_t* zs = zs_get_by_fd( fd );

	if ( !zs ) {
		errno = EBADF;
		return -1;
	}

	msg_switch_t* msg_switch = (msg_switch_t*) malloc( sizeof( msg_switch_t ) );
	memset( msg_switch->msgs, 0, sizeof( msg_switch->msgs ) );
	list_init( &msg_switch->pending_resps );
	list_init( &msg_switch->pending_msgs );
	msg_switch->sockfd = fd;
	msg_switch->connected = true;
	msg_switch->read.type = 0;
	msg_switch->curr_read_size = 0;
	msg_switch->write.type = 0;
	msg_switch->evt_hdlr = event_handler;

	FD_ZERO( &msg_switch->active_read_fd_set );
	FD_ZERO( &msg_switch->active_write_fd_set );

	zs_set_event_hdlr( fd, msg_zsocket_event_hdlr );
	zs_set_read( fd );

	zs->general.udata = msg_switch;

	return 0;
}

int msg_switch_destroy( int fd ) {
	msg_switch_t* msg_switch = msg_switch_get_by_fd( fd );

	if ( !msg_switch ) {
		errno = EINVAL;
		return -1;
	}

	zsocket_t* zs = zs_get_by_fd( fd );
	zs->general.udata = NULL;

	int i;
	for ( i = 0; i < FD_SETSIZE; i++ ) {
		if ( msg_switch->msgs[ i ] ) {
			msg_free( msg_switch, i - FD_SETSIZE );
		}
	}

	while ( list_size( &msg_switch->pending_resps ) )
		free( list_fetch( &msg_switch->pending_resps ) );
	list_destroy( &msg_switch->pending_resps );

	list_destroy( &msg_switch->pending_msgs );

	zs_clr_read( fd );
	zs_clr_write( fd );
	//zclose( msg_switch->sockfd );

	free( msg_switch );

	return 0;
}

bool msg_switch_need_select() {
	return n_selectable;
}

int msg_switch_select() {
	int fd;
	for ( fd = 0; fd < FD_SETSIZE; fd++ ) {
		msg_switch_t* msg_switch = msg_switch_get_by_fd( fd );
		if ( !msg_switch ) continue;

		fd_set read_fd_set  = msg_switch->active_read_fd_set;
		fd_set write_fd_set = msg_switch->active_write_fd_set;

		int msgid;
		for ( msgid = 0; msgid < FD_SETSIZE; msgid++ ) {
			msg_t* msg = msg_get( msg_switch, msgid );

			if ( msg && !FL_ISSET( msg->status, MSG_STAT_CONNECTED ) )
				msg_switch->evt_hdlr( msg_switch->sockfd, msgid, MSG_EVT_ACCEPT_READY );

			if ( FD_ISSET( msgid, &read_fd_set ) && FD_ISSET( msgid, &msg_switch->active_read_fd_set )
			  && ( !msg || FL_ARESOMESET( msg->status, MSG_STAT_PACKET_AVAIL_TO_READ | MSG_STAT_DISCONNECTED ) ) ) {

				msg_switch->evt_hdlr( msg_switch->sockfd, msgid, MSG_EVT_READ_READY );
			}

			if ( FD_ISSET( msgid, &write_fd_set ) && FD_ISSET( msgid, &msg_switch->active_write_fd_set )
			  && ( !msg || FL_ARESOMESET( msg->status, MSG_STAT_SPACE_AVAIL_FOR_WRIT | MSG_STAT_DISCONNECTED ) ) ) {
				msg_switch->evt_hdlr( msg_switch->sockfd, msgid, MSG_EVT_WRITE_READY );
			}

			/*if (
			( FD_ISSET( msgid, &msg_switch->active_read_fd_set ) &&  FL_ISSET( msg->status, MSG_STAT_PACKET_AVAIL_TO_READ ) ) ||
			( FD_ISSET( msgid, &msg_switch->active_write_fd_set ) && FL_ISSET( msg->status, MSG_STAT_SPACE_AVAIL_FOR_WRIT ) )
			)
				rw_still_avail++;*/
		}
	}

	return msg_switch_need_select();
}

//////////////////////////////////////////////////////////////////////////////////////////////
