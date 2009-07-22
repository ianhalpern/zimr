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

#ifndef _PD_REQUEST_H
#define _PD_REQUEST_H

#include "general.h"
#include "website.h"
#include "response.h"

typedef struct request {
	int sockfd;
	char ip[ 12 ];
	char hostname[ 128 ];
	char type;
	char http_version[ 4 ];
	char url[ 512 ];
	char full_url[ 512 ];
	char post_body[ 1024 ];
	website_t*  website;
	response_t response;
} request_t;

request_t request_create( website_t*, int, char*, size_t );

#endif
