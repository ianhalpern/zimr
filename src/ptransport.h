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

#ifndef _PD_PTRANSPORT_H
#define _PD_PTRANSPORT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "psocket.h"

#define PT_MSG_IS_FIRST(X) ( ((ptransport_t *)(X))->header->flags & PT_MSG_FIRST )
#define PT_MSG_IS_LAST(X)  ( ((ptransport_t *)(X))->header->flags & PT_MSG_LAST  )

#define PT_RO 0x0001
#define PT_WO 0x0002

#define PT_MSG_FIRST 0x0001
#define PT_MSG_LAST  0x0002

typedef struct {
	int size;
	int msgid;
	int flags;
} ptransport_header_t;

typedef struct {
	int pd;
	int io_type;
	ptransport_header_t* header;
	void* message;
	char buffer[ PT_BUF_SIZE ];
} ptransport_t;

ptransport_t* ptransport_open( int pd, int io_type, int msgid );
void ptransport_reset( ptransport_t* transport, int flags );
int  ptransport_read( ptransport_t* transport );
int  ptransport_write( ptransport_t* transport, void* message, int size );
int  ptransport_flush( ptransport_t* transport );
int  ptransport_close( ptransport_t* transport );
void ptransport_free( ptransport_t* transport );

#endif
