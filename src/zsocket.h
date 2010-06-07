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

#ifndef _ZM_ZSOCKET_H
#define _ZM_ZSOCKET_H

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "general.h"
#include "zfildes.h"

#define ZSOCK_LISTEN   0x01
#define ZSOCK_CONNECT  0x02
//#define ZSOCK_ZLISTEN  0x03
//#define ZSOCK_ZCONNECT 0x04

#define ZS_STAT_CONNECTED               0x00001
#define ZS_STAT_WANT_READ_FROM_READ     0x00002
#define ZS_STAT_WANT_READ_FROM_WRITE    0x00004
#define ZS_STAT_WANT_WRITE_FROM_WRITE   0x00008
#define ZS_STAT_WANT_WRITE_FROM_READ    0x00010
#define ZS_STAT_WANT_READ_FROM_ACCEPT   0x00020
#define ZS_STAT_WANT_WRITE_FROM_ACCEPT  0x00040
#define ZS_STAT_WANT_READ_FROM_CONNECT  0x00080
#define ZS_STAT_WANT_WRITE_FROM_CONNECT 0x00100
#define ZS_STAT_READABLE                0x00200
#define ZS_STAT_WRITABLE                0x00400
#define ZS_STAT_ACCEPTING               0x00800
#define ZS_STAT_CONNECTING              0x01000
#define ZS_STAT_READING                 0x02000
#define ZS_STAT_WRITING                 0x04000
#define ZS_STAT_EOF                     0x08000
#define ZS_STAT_CLOSED                  0x10000

#define ZS_EVT_READ_READY   0x1
#define ZS_EVT_WRITE_READY  0x2
#define ZS_EVT_ACCEPT_READY 0x3

#define ZSOCK_HDLR (void (*)( int, int, void*, ssize_t, void* ))

#define RSA_SERVER_CERT "server.crt"
#define RSA_SERVER_KEY  "server.key"

typedef struct {
	char* buffer;
	size_t size;
	ssize_t used;
	size_t pos;
	int rw_errno;
} zsock_rw_data_t;

typedef union {
	char type;
	struct {
		char type;
		int sockfd;
		struct sockaddr_in addr;
		int portno;
		unsigned int status;
		void (*event_hdlr)( int fd, int event );
		void* udata;
	} general;
	struct {
		char type;
		int sockfd;
		struct sockaddr_in addr;
		int portno;
		unsigned int status;
		void (*event_hdlr)( int fd, int event );
		void* udata;
		////////////////////
		int n_open;
		int accepted_fd;
		int accepted_errno;
		SSL_CTX* ssl_ctx;
	} listen;
	struct {
		char type;
		int sockfd;
		struct sockaddr_in addr;
		int portno;
		unsigned int status;
		void (*event_hdlr)( int fd, int event );
		void* udata;
		////////////////////
		int parent_fd;
		zsock_rw_data_t read;
		zsock_rw_data_t write;
		SSL* ssl;
	} connect;
} zsocket_t;

typedef struct {
	struct sockaddr_in cli_addr;
	socklen_t cli_len;
} zsock_a_data_t;

zsocket_t* zsockets[ FD_SETSIZE ];

void zs_init();
int  zsocket( in_addr_t addr, int portno, int type, void (*zs_event_hdlr)( int fd, int event ), bool ssl );

int     zs_accept( int fd );
ssize_t zs_read( int fd, void* buf, size_t size );
ssize_t zs_write( int fd, const void* buf, size_t n );
int     zs_close( int zsockfd );

int zs_select();

void zs_set_read( int fd );
void zs_clr_read( int fd );
bool zs_isset_read( int fd );
void zs_set_write( int fd );
void zs_clr_write( int fd );
bool zs_isset_write( int fd );

bool zs_is_ssl( int fd );
zsocket_t* zs_get_by_info( in_addr_t addr, int portno );
zsocket_t* zs_get_by_fd( int sockfd );
void zs_set_event_hdlr( int fd, void (*zs_event_hdlr)( int fd, int event ) );

#endif
