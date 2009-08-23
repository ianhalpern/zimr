/*   Poroda - Next Generation Web Server
 *
 *+  Copyright (c) 2009 Ian Halpern
 *@  http://Poroda.org
 *
 *   This file is part of Poroda.
 *
 *   Poroda is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Poroda is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Poroda.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef _PD_COOKIES_H
#define _PD_COOKIES_H

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "config.h"

typedef struct {
	char name[ HEADER_NAME_MAX_LEN ];
	char value[ HEADER_VALUE_MAX_LEN ];
	char domain[ 128 ];
	char path[ 256 ];
	char expires[ 30 ];
	char updated;
} cookie_t;

typedef struct {
	int num;
	cookie_t list[ HEADERS_MAX_NUM ];
} cookies_t;

cookies_t cookies_parse( char* raw );
void cookies_set_cookie( cookies_t* cookies, const char* name, const char* value, time_t expires, const char* domain, const char* path );
cookie_t* cookies_get_cookie( cookies_t* cookies, const char* name );
char* cookies_to_string( cookies_t* cookies, char* cookies_str, int size );

#endif
