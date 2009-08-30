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

#ifndef _ZM_CONNECTION_H
#define _ZM_CONNECTION_H

#include <arpa/inet.h>

#include "website.h"
#include "headers.h"
#include "params.h"
#include "cookies.h"
#include "urldecoder.h"

typedef struct {
	char type;
	char url[ PATH_MAX ];
	char* post_body;
	headers_t headers;
	params_t* params;
} request_t;

typedef struct {
	short http_status;
	headers_t headers;
} response_t;

typedef struct {
	int sockfd;
	struct in_addr ip;
	char hostname[ 128 ];
	char http_version[ 4 ];
	website_t* website;
	request_t  request;
	response_t response;
	cookies_t  cookies;
	void* udata;
} connection_t;

connection_t* connection_create( website_t*, int, char*, size_t );
void connection_free( connection_t* connection );

const char* response_status( short );
void response_set_status( response_t* response, short status );

#endif
