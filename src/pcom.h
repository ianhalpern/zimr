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

#ifndef _PD_PCOM_H
#define _PD_PCOM_H

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

#define PCOM_MSG_IS_FIRST(X) ( ((pcom_transport_t *)(X))->header->flags & PCOM_MSG_FIRST )
#define PCOM_MSG_IS_LAST(X)  ( ((pcom_transport_t *)(X))->header->flags & PCOM_MSG_LAST  )

#define PCOM_NO_SID 0
#define PCOM_NO_TID 0

#define PCOM_RO 0x0001
#define PCOM_WO 0x0002

#define PCOM_MSG_FIRST 0x0001
#define PCOM_MSG_LAST  0x0002

typedef struct {
	int size;
	int id;
	int flags;
	int key;
} pcom_header_t;

typedef struct {
	int pd;
	int io_type;
	pcom_header_t* header;
	void* message;
	char buffer[ PCOM_BUF_SIZE ];
} pcom_transport_t;

int pcom_create( );
int pcom_connect( int pid, int pd );
void pcom_destroy( int fd );

pcom_transport_t* pcom_open( int pd, int io_type, int id, int key );
void pcom_reset_header( pcom_transport_t* transport, int flags );
int  pcom_read( pcom_transport_t* transport );
int  pcom_write( pcom_transport_t* transport, void* message, int size );
int  pcom_flush( pcom_transport_t* transport );
int  pcom_close( pcom_transport_t* transport );
void pcom_free( pcom_transport_t* transport );

#endif
