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

#ifndef _PD_HEADER_H
#define _PD_HEADER_H

#include <string.h>

#include "general.h"

typedef struct {
	char name[ HEADER_NAME_MAX_LEN ];
	char value[ HEADER_VALUE_MAX_LEN ];
} header_t;

typedef struct {
	int num;
	header_t list[ HEADERS_MAX_NUM ];
} headers_t;

headers_t headers_parse( char* raw );
char* headers_to_string( headers_t* headers, char* headers_str );
void headers_set_header( headers_t* headers, char* name, char* value );
header_t* headers_get_header( headers_t* headers, char* name );

#endif
