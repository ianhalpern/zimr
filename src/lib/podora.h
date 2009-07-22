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

#ifndef _PD_PODORA_H
#define _PD_PODORA_H

#include "website.h"
#include "pcom.h"

typedef struct website_data {
	char* pubdir;
	int req_fd;
	char status;
} website_data_t;

const char* podora_version( );
const char* podora_build_date( );
int podora_init( );
void podora_shutdown( );
int podora_read_server_info( int* pid, int* res_fn );
void podora_start( );
void podora_send_cmd( int cmd, int key, void* message, int size );

void podora_request_handler( int req_fd, website_t* website );
void podora_file_handler( int fd, pcom_transport_t* transport );

website_t* podora_website_create( char* url );
void podora_website_destroy( website_t* website );
int podora_website_start( website_t* website );
int podora_website_stop( website_t* website );

#endif
