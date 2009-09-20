
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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <openssl/ssl.h>

#include "general.h"
#include "zerr.h"

#define ZSOCK_LISTEN   0x01
#define ZSOCK_CONNECT  0x02
#define ZSOCK_ZLISTEN  0x03
#define ZSOCK_ZCONNECT 0x04

typedef struct {
	int sockfd;
	in_addr_t addr;
	int portno;
	int n_open;
	SSL_CTX* ssl; // TODO
} zsocket_t;

zsocket_t* zsockets[ FD_SETSIZE ];

void zsocket_init( );
int zsocket( in_addr_t addr, int portno, int type );
void zsocket_close( int zsockfd );
zsocket_t* zsocket_get_by_info( in_addr_t addr, int portno );
zsocket_t* zsocket_get_by_sockfd( int sockfd );

#endif
