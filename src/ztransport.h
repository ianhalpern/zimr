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

#ifndef _ZM_PTRANSPORT_H
#define _ZM_PTRANSPORT_H

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

#define ZT_MSG_IS_FIRST(X) ( ((ztransport_t *)(X))->header->flags & ZT_MSG_FIRST )
#define ZT_MSG_IS_LAST(X)  ( ((ztransport_t *)(X))->header->flags & ZT_MSG_LAST  )

#define ZT_RO 0x0001
#define ZT_WO 0x0002

#define ZT_MSG_FIRST 0x0001
#define ZT_MSG_LAST  0x0002

typedef struct {
	int size;
	int msgid;
	int flags;
} ztransport_header_t;

typedef struct {
	int pd;
	int io_type;
	ztransport_header_t* header;
	void* message;
	char buffer[ ZT_BUF_SIZE ];
} ztransport_t;

ztransport_t* ztransport_open( int pd, int io_type, int msgid );
void ztransport_reset( ztransport_t* transport, int flags );
int  ztransport_read( ztransport_t* transport );
int  ztransport_write( ztransport_t* transport, void* message, int size );
int  ztransport_flush( ztransport_t* transport );
int  ztransport_close( ztransport_t* transport );
void ztransport_free( ztransport_t* transport );

#endif
