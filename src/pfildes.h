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

#ifndef _PD_PFILDES_H
#define _PD_PFILDES_H

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

void pfd_set( int fd, int type, void* udata );
void pfd_clr( int fd );
void pfd_register_type( int type, void (*handler)( int, void* ) );
void* pfd_udata( int fd );
int pfd_select( );
void pfd_unblock( );

#endif
