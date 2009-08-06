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

#include <time.h>
#include <sys/stat.h>
#include <syslog.h>
#include <assert.h>
#include <yaml.h>

#include "website.h"
#include "pcom.h"
#include "pfildes.h"
#include "connection.h"
#include "mime.h"
#include "daemonize.h"

#define DAEMON_NAME "podora-website"
#define PAGE_HANDLER void (*)( connection_t*, const char*, void* )

typedef struct website_data {
	int req_fd;
	char status;
	char* pubdir;
	void (*connection_handler)( connection_t* connection );
	void* udata;
} website_data_t;

const char* podora_version( );
const char* podora_build_date( );

int  podora_init( );
void podora_shutdown( );
int  podora_read_server_info( int* pid, int* res_fn );
int  podora_daemonize( int flags );
int  podora_cnf_load( );
void podora_start( );
void podora_send_cmd( int cmd, int key, void* message, int size );

void podora_connection_handler( int req_fd, website_t* website );
void podora_file_handler( int fd, pcom_transport_t* transport );

website_t* podora_website_create( char* url );
void  podora_website_destroy( website_t* website );
int   podora_website_enable( website_t* website );
int   podora_website_disable( website_t* website );
void  podora_website_set_pubdir( website_t* website, const char* pubdir );
char* podora_website_get_pubdir( website_t* website );
void  podora_website_set_connection_handler ( website_t* website, void (*connection_handler)( connection_t* ) );
void  podora_website_unset_connection_handler( website_t* website );
void  podora_website_default_connection_handler( connection_t* connection );

void podora_connection_send_status( pcom_transport_t* transport, connection_t* connection );
void podora_connection_send_headers( pcom_transport_t* transport, connection_t* connection );
void podora_connection_send_file( connection_t* connection, char* filepath, unsigned char use_pubdir );
void podora_connection_send( connection_t* connection, void* message, int size );
void podora_connection_send_error( );
void podora_connection_default_page_handler( connection_t* connection, char* filepath );

void podora_register_page_handler( const char* page_type, void (*page_handler)( connection_t*, const char*, void* ), void* udata );

#endif
