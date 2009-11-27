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

#ifndef _ZM_MSG_SWITCH_H
#define _ZM_MSG_SWITCH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdarg.h>

#include "config.h"
#include "simclist.h"
#include "zfildes.h"

#define MSG_STAT_NEW           0x1
#define MSG_STAT_WRITE_PACK    0x2
#define MSG_STAT_PENDING_PACKS 0x4
#define MSG_STAT_COMPLETE      0x8
#define MSG_STAT_READ_RESP     0x10

#define MSG_EVT_NEW        0x1
#define MSG_EVT_DESTROY    0x2
#define MSG_EVT_COMPLETE   0x3
#define MSG_EVT_SENT       0x4
#define MSG_EVT_BUF_FULL   0x5
#define MSG_EVT_BUF_EMPTY  0x6

#define MSG_RECV_EVT_KILL  0x20
#define MSG_RECV_EVT_RESP  0x21
#define MSG_RECV_EVT_PACK  0x22
#define MSG_RECV_EVT_FIRST 0x23
#define MSG_RECV_EVT_LAST  0x24

#define MSG_SWITCH_EVT_NEW       0x40
#define MSG_SWITCH_EVT_DESTROY   0x41
#define MSG_SWITCH_EVT_IO_FAILED 0x42

#define PACK_DATA_SIZE ( 4 * 1024 - sizeof( int ) * 3 )

#define PACK_FL_FIRST 0x1
#define PACK_FL_LAST  0x2
#define PACK_FL_KILL  0x4

#define PACK_IS_FIRST(X) ( FL_ISSET( ( (msg_packet_t*)(X) )->flags, PACK_FL_FIRST ) )
#define PACK_IS_LAST(X)  ( FL_ISSET( ( (msg_packet_t*)(X) )->flags, PACK_FL_LAST ) )

#define MSG_PACK_RESP_OK   0x1
#define MSG_PACK_RESP_FAIL 0x2

#define MSG_TYPE_RESP 0x1
#define MSG_TYPE_PACK 0x2

#define MSG_TYPE_GET_SIZE(X) ( (X) == MSG_TYPE_PACK ? sizeof( msg_packet_t ) : sizeof( msg_packet_resp_t ) )

typedef struct {
	int msgid;
	char status;
} msg_packet_resp_t;

typedef struct {
	int msgid;
	int size;
	int flags;
	char data[ PACK_DATA_SIZE ];
} msg_packet_t;

typedef struct {
	int msgid;
	int status;
	list_t queue;
} msg_t;

typedef struct {
	int type;
	union {
		int msgid;
		msg_packet_t* packet;
		msg_packet_resp_t* resp;
	} data;
} msg_event_t;

typedef struct msg_switch {
	int sockfd;
	list_t pending_resps;
	list_t pending_msgs;
	msg_t* msgs[ FD_SETSIZE * 2 ];

	int read_data_type;
	union {
		msg_packet_t packet;
		msg_packet_resp_t resp;
	} read_data;
	int  read_data_size;

	int write_data_type;
	union {
		msg_packet_t packet;
		msg_packet_resp_t resp;
	} write_data;
	int  write_data_size;

	// events
	void (*event_handler)( struct msg_switch*, msg_event_t event );
} msg_switch_t;

void msg_switch_init( int read_type, int writ_type );

msg_switch_t* msg_switch_new(
	int sockfd,
	void (*event_handler)( struct msg_switch*, msg_event_t event )
);
void msg_switch_destroy( msg_switch_t* msg_switch );
bool msg_switch_has_pending_msgs( msg_switch_t* msg_switch );
bool msg_switch_has_pending_resps( msg_switch_t* msg_switch );
bool msg_switch_is_pending( msg_switch_t* msg_switch );
void msg_switch_send_pack_resp( msg_switch_t* msg_switch, msg_packet_t* packet, char status );

msg_packet_resp_t msg_switch_pop_resp( msg_switch_t* msg_switch );
msg_packet_t msg_switch_pop_packet( msg_switch_t* msg_switch );

void msg_new( msg_switch_t* msg_switch, int msgid );
void msg_end( msg_switch_t* msg_switch, int msgid );
void msg_kill( msg_switch_t* msg_switch, int msgid );
bool msg_is_pending( msg_switch_t* msg_switch, int msgid );
bool msg_is_complete( msg_switch_t* msg_switch, int msgid );
bool msg_is_sent( msg_switch_t* msg_switch, int msgid );
bool msg_exists( msg_switch_t* msg_switch, int msgid );

void msg_accept_resp( msg_switch_t* msg_switch, msg_packet_resp_t resp );
void msg_push_data( msg_switch_t* msg_switch, int msgid, void* data, int size );

#endif
