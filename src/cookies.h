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

#ifndef _ZM_COOKIES_H
#define _ZM_COOKIES_H

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#include "config.h"

typedef struct {
	char name[ COOKIE_NAME_MAX_LEN ];
	char value[ COOKIE_VALUE_MAX_LEN ];
	char domain[ COOKIE_DOMAIN_MAX_LEN ];
	char path[ COOKIE_PATH_MAX_LEN ];
	time_t expires;
	int max_age;
	bool http_only;
	bool secure;
	char updated;
} cookie_t;


typedef struct {
	int num;
	cookie_t list[ HEADERS_MAX_NUM ];
} cookies_t;

cookies_t cookies_parse( char* raw );
void cookies_set_cookie( cookies_t* cookies, const char* name, const char* value, time_t expires, int max_age, const char* domain, const char* path, bool http_only, bool secure );
void cookies_del_cookie( cookies_t* cookies, const char* name );
cookie_t* cookies_get_cookie( cookies_t* cookies, const char* name );
char* cookies_to_string( cookies_t* cookies, char* cookies_str, int size );

#endif
