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

#ifndef _ZM_FD_HASH_H
#define _ZM_FD_HASH_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct fd_hash_el {
	struct fd_hash_el* previous;
	struct fd_hash_el* next;
} fd_hash_el_t;

typedef struct {
	fd_hash_el_t table[ FD_SETSIZE ];
	fd_hash_el_t* head;
	fd_hash_el_t* tail;
} fd_hash_t;

void fd_hash_init( fd_hash_t* );
void fd_hash_add( fd_hash_t*, int fd );
void fd_hash_remove( fd_hash_t*, int fd );
int fd_hash_head( fd_hash_t* );
int fd_hash_next( fd_hash_t*, int fd );

#endif
