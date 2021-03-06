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

#ifndef _ZM_WEBSITE_H
#define _ZM_WEBSITE_H

#include <assert.h>

#include "simclist.h"
#include "general.h"

#define WS_STATUS_ENABLED       0x01
#define WS_STATUS_WANT_ENABLE   0x02
#define WS_STATUS_DISABLED      0x03
#define WS_STATUS_ENABLING      0x04

#define WS_OPTION_PARSE_MULTIPARTS  0x01

typedef struct website {
	char* url;
	char* full_url;
	char* path;
	int sockfd;
	char ip[16];
	unsigned int options;
	void* udata;
} website_t;

list_t websites;

void website_init();
website_t* website_add ( int, char*, char* );
void website_remove( website_t* );
website_t* website_get_by_full_url( char* url, char* ip );
website_t* website_get_by_sockfd( int );
website_t* website_find( char* url, char* protocol, char* ip );
const char* website_protocol( website_t* );
void website_options_set(  website_t*, unsigned int, bool );
bool website_options_get( website_t*, unsigned int );
void website_options_reset( website_t* );

#endif
