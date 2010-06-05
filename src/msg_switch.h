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
#include "zsocket.h"
#include "zfildes.h"

#define MSG_STAT_NEW                       0x0001
#define MSG_STAT_READ_STARTED              0x0002
#define MSG_STAT_WRIT_STARTED              0x0004
#define MSG_STAT_READ_COMPLETED            0x0008
#define MSG_STAT_WRIT_COMPLETED            0x0010
#define MSG_STAT_FINISHED                  0x0020
#define MSG_STAT_PACKET_AVAIL_TO_READ      0x0040
#define MSG_STAT_PACKET_AVAIL_TO_WRIT      0x0080
#define MSG_STAT_SPACE_AVAIL_FOR_READ      0x0100
#define MSG_STAT_SPACE_AVAIL_FOR_WRIT      0x0200
#define MSG_STAT_WAITING_FOR_RESP          0x0400
#define MSG_STAT_NEED_TO_SEND_RESP         0x0800
#define MSG_STAT_WANT_PACK                 0x1000
#define MSG_STAT_DESTROY_SENT              0x2000

#define MSG_EVT_READ_START                 0x001
#define MSG_EVT_RECVD_DATA                 0x002
#define MSG_EVT_READ_END                   0x004
#define MSG_EVT_WRITE_START                0x008
#define MSG_EVT_WRITE_SPACE_AVAIL          0x010
#define MSG_EVT_WRITE_SPACE_FULL           0x020
#define MSG_EVT_WRITE_END                  0x040
#define MSG_EVT_DESTROYED                  0x080
#define MSG_SWITCH_EVT_NEW                 0x100
#define MSG_SWITCH_EVT_IO_FAILED           0x200
#define MSG_SWITCH_EVT_DESTROY             0x400

#define PACK_FL_FIRST 0x1
#define PACK_FL_LAST  0x2

#define PACK_IS_FIRST(X) ( FL_ISSET( ( (msg_packet_t*)(X) )->header.flags, PACK_FL_FIRST ) )
#define PACK_IS_LAST(X)  ( FL_ISSET( ( (msg_packet_t*)(X) )->header.flags, PACK_FL_LAST ) )

#define MSG_RESP_OK      0x1
#define MSG_RESP_DESTROY 0x2

#define MSG_TYPE_RESP 0x1
#define MSG_TYPE_PACK 0x2

#define MSG_TYPE_GET_SIZE(X) ( (X) == MSG_TYPE_PACK ? sizeof( msg_packet_t ) : sizeof( msg_resp_t ) )

typedef struct {
	int msgid;
	char status;
} msg_resp_t;

typedef struct {
	int msgid;
	int size;
	int flags;
} msg_packet_header_t;

typedef struct {
	msg_packet_header_t header;
	char data[ PACK_DATA_SIZE ];
} msg_packet_t;

typedef struct {
	int msgid;
	int status;
	int n_writ;
	int n_read;
	msg_packet_t* incomplete_writ_packet;
	list_t read_queue;
	list_t writ_queue;
} msg_t;

typedef struct {
	int type;
	union {
		int msgid;
		msg_packet_t packet;
	} data;
} msg_event_t;

typedef struct msg_switch {
	int sockfd;
	list_t pending_resps;
	list_t pending_msgs;
	list_t pending_events;
	msg_t* msgs[ FD_SETSIZE * 2 ];

	struct {
		int type;
		union {
			msg_packet_t packet;
			msg_resp_t resp;
		} data;
	} read;
	int curr_read_size;

	struct {
		int type;
		union {
			msg_packet_t packet;
			msg_resp_t resp;
		} data;
	} write;

	// events
	void (*event_handler)( struct msg_switch*, msg_event_t* event );
} msg_switch_t;

void msg_switch_create( int sockfd, void (*event_handler)( struct msg_switch*, msg_event_t* event ) );
void msg_switch_destroy( int sockfd );
bool msg_exists( int sockfd, int msgid );
void msg_start( int sockfd, int msgid );
void msg_end( int sockfd, int msgid );
void msg_send( int sockfd, int msgid, const void* data, int size );
void msg_want_data( int sockfd, int msgid );
//void msg_kill( int sockfd, int msgid );
void msg_destroy( int sockfd, int msgid );
void msg_switch_fire_all_events();

#endif
