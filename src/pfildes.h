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

#ifndef _PD_PFILDES_H
#define _PD_PFILDES_H

#define PFD_TYPE_HDLR (void (*)( int, void* ))

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define PFD_TYPE_PCOM          0x01
#define PFD_TYPE_SOCK_LISTEN   0x02
#define PFD_TYPE_SOCK_ACCEPTED 0x03
#define PFD_TYPE_FILE          0x04


typedef struct {
	int type;
	void* udata;
} fd_info_t;

typedef struct {
	void (*handler)( int fd, void* udata );
} fd_type_t;

void pfd_set( int fd, int type, void* udata );
void pfd_clr( int fd );
void pfd_register_type( int type, void (*handler)( int, void* ) );
void pfd_start( );

#endif
