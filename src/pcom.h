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

typedef struct {
	int size;
	int id;
	int flags;
	int key;
} pcom_header_t;

typedef struct {
	int pd;
	int io_type;
	struct flock lock;
	pcom_header_t* header;
	void* message;
	char buffer[ PCOM_BUF_SIZE ];
} pcom_transport_t;

int pcom_create( );
int pcom_connect( int pid, int pd );

pcom_transport_t* pcom_open( int pd, int io_type, int id, int key );
void pcom_reset_header( pcom_transport_t* transport, int flags );
int  pcom_read( pcom_transport_t* transport );
int  pcom_write( pcom_transport_t* transport, void* message, int size );
int  pcom_flush( pcom_transport_t* transport );
int  pcom_close( pcom_transport_t* transport );
void pcom_free( pcom_transport_t* transport );

#endif
