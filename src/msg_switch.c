#include "msg_switch.h"

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
}

msg_switch_t* msg_switch_new( ) {
	msg_switch_t* msg_switch = (msg_switch_t*) malloc( sizeof( msg_switch_t ) );
	memset( msg_switch->msgs, 0, sizeof( msg_switch->msgs ) );
	list_init( &msg_switch->pending_resps );
	list_init( &msg_switch->pending_msgs );
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

msg_packet_resp_t msg_switch_get_resp( msg_switch_t* msg_switch ) {
	msg_packet_resp_t resp = *(msg_packet_resp_t*) list_get_at( &msg_switch->pending_resps, 0 );
	free( list_fetch( &msg_switch->pending_resps ) );
	return resp;
}

void msg_accept_resp( msg_switch_t* msg_switch, msg_packet_resp_t resp ) {
	if ( resp.status == PACK_RESP_OK ) {
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

bool msg_switch_pending( msg_switch_t* msg_switch ) {
	if ( msg_switch_has_pending_msgs( msg_switch )
	  || msg_switch_has_pending_resps( msg_switch ) )
		return true;
	return false;
}

void msg_push( msg_switch_t* msg_switch, int msgid, void* data, int size ) {
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
bool msg_is_ready( msg_switch_t* msg_switch, int msgid ) {
	return msg_switch->msgs[ msgid ]->status == MSG_STAT_WRITE;
}
