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

#define MSG_STAT_CONNECTED            0x0001
#define MSG_STAT_PACKET_AVAIL_TO_READ 0x0002
#define MSG_STAT_PACKET_AVAIL_TO_WRIT 0x0004
#define MSG_STAT_SPACE_AVAIL_FOR_READ 0x0008
#define MSG_STAT_SPACE_AVAIL_FOR_WRIT 0x0010
#define MSG_STAT_WAITING_FOR_RESP     0x0020
#define MSG_STAT_NEED_TO_SEND_RESP    0x0040
#define MSG_STAT_WRITING              0x0080
#define MSG_STAT_CLOSED               0x0100
#define MSG_STAT_DISCONNECTED         0x0200

//#define MSG_STAT_DESTROY_SENT         0x2000
//#define MSG_STAT_WAITING_TO_FREE      0x4000

#define MSG_EVT_ACCEPT_READY    0x1
#define MSG_EVT_READ_READY      0x2
#define MSG_EVT_WRITE_READY     0x3
#define MSG_SWITCH_EVT_IO_ERROR 0x4

//#define MSG_SWITCH_EVT_NEW                 0x200
//#define MSG_SWITCH_EVT_DESTROY             0x800

//#define PACK_FL_FIRST 0x1
//#define PACK_FL_LAST  0x2

//#define PACK_IS_FIRST(X) ( FL_ISSET( ( (msg_packet_t*)(X) )->header.flags, PACK_FL_FIRST ) )
//#define PACK_IS_LAST(X)  ( FL_ISSET( ( (msg_packet_t*)(X) )->header.flags, PACK_FL_LAST ) )

#define MSG_RESP_OK     0x1
#define MSG_RESP_DISCON 0x2

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
//	int flags;
} msg_packet_header_t;

typedef struct {
	msg_packet_header_t header;
	char data[ PACK_DATA_SIZE ];
} msg_packet_t;

typedef struct {
	int fd;
	int msgid;
	int status;
	int n_writ;
	int n_read;
	msg_packet_t* incomplete_writ_packet;
	int cur_read_packet_pos;
	list_t read_queue;
	list_t writ_queue;
} msg_t;

typedef struct msg_switch {
	int sockfd;
	bool connected;
	list_t pending_resps;
	list_t pending_msgs;
	msg_t* msgs[ FD_SETSIZE ];
	fd_set active_read_fd_set;
	fd_set active_write_fd_set;

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
	void (*evt_hdlr)( int fd, int msgid, int event );
} msg_switch_t;

int msg_switch_create( int fd, void (*event_handler)( int fd, int msgid, int event ) );
int msg_switch_destroy( int fd );
bool msg_exists( int fd, int msgid );

void msg_switch_toggle_accept( bool );

int     msg_accept( int fd, int i );
int     msg_open( int fd, int msgid );
ssize_t msg_write( int fd, int msgid, const void* buf, size_t size );
ssize_t msg_read( int fd, int msgid, void* buf, size_t size );
int     msg_flush( int fd, int msgid );
int     msg_close( int fd, int msgid );

bool msg_switch_need_select();
int msg_switch_select();

void msg_set_read( int fd, int msgid );
void msg_clr_read( int fd, int msgid );
bool msg_isset_read( int fd, int msgid );
void msg_set_write( int fd, int msgid );
void msg_clr_write( int fd, int msgid );
bool msg_isset_write( int fd, int msgid );

#endif
