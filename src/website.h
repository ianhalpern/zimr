/*   Pacoda - Next Generation Web Server
 *
 *+  Copyright (c) 2009 Ian Halpern
 *@  http://Pacoda.org
 *
 *   This file is part of Pacoda.
 *
 *   Pacoda is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Pacoda is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Pacoda.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef _PD_WEBSITE_H
#define _PD_WEBSITE_H

#include "general.h"

#define WS_STATUS_ENABLED   0x01
#define WS_STATUS_DISABLED  0x02
#define WS_STATUS_ENABLING  0x03

typedef struct website {
	char* url;
	int sockfd;
	struct website* next;
	struct website* prev;
	void* udata;
} website_t;

website_t* website_add ( int, char* );
void website_remove( website_t* );
website_t* website_get_by_url( char* url );
website_t* website_get_by_sockfd( int );
website_t* website_get_root( );

#endif
