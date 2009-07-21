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

#ifndef _PD_WEBSITE_H
#define _PD_WEBSITE_H

#include "general.h"

typedef struct website {
	char* url;
	key_t shmkey;
	int id;
	struct website* next;
	struct website* prev;
	void* data;
} website_t;

website_t* website_add ( int, char* );
void website_remove ( website_t* );
website_t* website_get_by_url( char* url );
website_t* website_get_by_id( int );
website_t* website_get_root( );

#endif
