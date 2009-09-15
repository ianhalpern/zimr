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

#ifndef _ZM_ZTRANSPORT_H
#define _ZM_ZTRANSPORT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "zsocket.h"
#include "simclist.h"

#define ZT_IS_FIRST(X) ( ((zt_t*)(X))->flags & ZT_FIRST )
#define ZT_IS_LAST(X)  ( ((zt_t*)(X))->flags & ZT_LAST  )

#define ZT_FIRST 0x0001
#define ZT_LAST  0x0002

typedef struct {
	int msgid;
	int size;
	int flags;
	char message[ ZT_MSG_SIZE ];
} zt_t;

void ztinit( );
list_t ztopen( );
void ztpush( list_t ztset, void* message, int size );
void ztclose( list_t ztset );

#endif
