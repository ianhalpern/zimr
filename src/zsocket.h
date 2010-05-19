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

#define ZACCEPT 0x20
#define ZWRITE  0x22
#define ZREAD   0x23

#define ZCONNECTED          0x001
#define ZREAD_FROM_READ     0x002
#define ZREAD_FROM_WRITE    0x004
#define ZWRITE_FROM_WRITE   0x008
#define ZWRITE_FROM_READ    0x010
#define ZREAD_FROM_ACCEPT   0x020
#define ZWRITE_FROM_ACCEPT  0x040
#define ZREAD_READ          0x080

#define ZSE_ACCEPT_ERR 0x1
#define ZSE_ACCEPTED_CONNECTION 0x2
#define ZSE_READ_DATA 0x3
#define ZSE_WROTE_DATA 0x4

#define ZSOCK_HDLR (void (*)( int, int, void*, size_t, void* ))

#define RSA_SERVER_CERT "server.crt"
#define RSA_SERVER_KEY  "server.key"

typedef struct {
	char* buffer;
	size_t buffer_size;
	size_t buffer_used;
} zsock_rw_data_t;

typedef struct {
	int type;
	union {
		struct {
			int fd;
			in_addr_t addr;
		} conn;
		zsock_rw_data_t read;
		zsock_rw_data_t write;
	} data;
} zsocket_event_t;

typedef union {
	char type;
	struct {
		char type;
		int sockfd;
		in_addr_t addr;
		int portno;
		unsigned int flags;
		void (*event_hdlr)( int fd, zsocket_event_t event );
	} general;
	struct {
		char type;
		int sockfd;
		in_addr_t addr;
		int portno;
		unsigned int flags;
		void (*event_hdlr)( int fd, zsocket_event_t event );
		int n_open;
		SSL_CTX* ssl_ctx;
	} listen;
	struct {
		char type;
		int sockfd;
		in_addr_t addr;
		int portno;
		unsigned int flags;
		void (*event_hdlr)( int fd, zsocket_event_t event );
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

void zsocket_init();
int  zsocket( in_addr_t addr, int portno, int type, void (*zsocket_event_hdlr)( int fd, zsocket_event_t event ), bool ssl );
void zclose( int zsockfd );
void zaccept( int fd, bool toggle );
void zread( int fd, bool toggle );
void zwrite( int fd, const void* buf, size_t n );
zsocket_t* zsocket_get_by_info( in_addr_t addr, int portno );
zsocket_t* zsocket_get_by_sockfd( int sockfd );

#endif
