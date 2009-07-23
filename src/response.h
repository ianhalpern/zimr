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
 *
 */

#ifndef _PD_RESPONSE_H
#define _PD_RESPONSE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "general.h"
#include "website.h"
#include "headers.h"

typedef struct response {
	int sockfd;
	char http_version[ 4 ];
	short http_status;
	website_t* website;
	headers_t  headers;
} response_t;

const char* response_status( short );

response_t response_create( int sockfd, website_t* website, char http_version[ ] );
void response_set_status( response_t* response, short status );

#endif
