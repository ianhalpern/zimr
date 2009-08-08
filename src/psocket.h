
/*   Podora - Next Generation Web Server
 *
 *+  Copyright (c) 2009 Ian Halpern
 *@  http://Podora.org
 *
 *   This file is part of Podora.
 *
 *   Podora is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Podora is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Podora.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef _PD_PSOCKET_H
#define _PD_PSOCKET_H

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <openssl/ssl.h>

#include "config.h"

#define PSOCK_LISTEN  0x01
#define PSOCK_CONNECT 0x02

typedef struct psocket {
	int sockfd;
	in_addr_t addr;
	int portno;
	int n_open;
	SSL_CTX* ssl; // TODO
	struct psocket* next;
	struct psocket* prev;
	void* udata;
} psocket_t;

psocket_t* psocket_open( in_addr_t addr, int portno );
psocket_t* psocket_connect( in_addr_t addr, int portno );
int psocket_init( in_addr_t addr, int portno, int type );
psocket_t* psocket_create( int sockfd, in_addr_t addr, int portno );
void psocket_close( psocket_t* p );
psocket_t* psocket_get_by_info( in_addr_t addr, int portno );
psocket_t* psocket_get_by_sockfd( int sockfd );
psocket_t* psocket_get_root( );

#endif
