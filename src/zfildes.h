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

#define ZFD_TYPE_HDLR (void (*)( int, void* ))

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

#define ZFD_R 0x01
#define ZFD_W 0x02

typedef struct {
	void (*handler)( int fd, void* udata );
	void* udata;
} fd_info_t;

void zfd_set( int fd, char io_type, void (*handler)( int, void* ), void* udata );
void zfd_clr( int fd, char io_type );
bool zfd_isset( int fd, char io_type );
int zfd_select();
void zfd_unblock();

#endif
