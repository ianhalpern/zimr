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

#ifndef _ZM_PFILDES_H
#define _ZM_PFILDES_H

#define PFD_TYPE_HDLR (void (*)( int, void* ))

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

#define PFD_TYPE_INT_LISTEN    0x01
#define PFD_TYPE_EXT_LISTEN    0x02
#define PFD_TYPE_INT_CONNECTED 0x03
#define PFD_TYPE_EXT_CONNECTED 0x04
#define PFD_TYPE_FILE          0x05

typedef struct {
	int type;
	void* udata;
} fd_info_t;

typedef struct {
	void (*handler)( int fd, void* udata );
} fd_type_t;

void zfd_set( int fd, int type, void* udata );
void zfd_clr( int fd );
bool zfd_isset( int fd );
void zfd_register_type( int type, void (*handler)( int, void* ) );
void* zfd_udata( int fd );
int zfd_select( );
void zfd_unblock( );

#endif
