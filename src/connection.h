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

#ifndef _PD_CONNECTION_H
#define _PD_CONNECTION_H

#include "website.h"
#include "headers.h"
#include "params.h"
#include "cookies.h"

typedef struct request {
	char type;
	char url[ 512 ];
	char full_url[ 512 ];
	char post_body[ 1024 ];
	headers_t headers;
	params_t params;
} request_t;

typedef struct response {
	short http_status;
	headers_t headers;
} response_t;

typedef struct {
	int sockfd;
	char ip[ 12 ];
	char hostname[ 128 ];
	char http_version[ 4 ];
	website_t* website;
	request_t  request;
	response_t response;
	cookies_t  cookies;
} connection_t;

connection_t connection_create( website_t*, int, char*, size_t );

const char* response_status( short );
void response_set_status( response_t* response, short status );

#endif
