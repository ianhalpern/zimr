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

#ifndef _ZM_ZIMR_H
#define _ZM_ZIMR_H

#include <time.h>
#include <sys/stat.h>
#include <syslog.h>
#include <assert.h>
#include <stdbool.h>
#include <signal.h>
#include <dirent.h>

#include "website.h"
#include "ztransport.h"
#include "zfildes.h"
#include "connection.h"
#include "mime.h"
#include "zcnf.h"
#include "zerr.h"

#define DAEMON_NAME "zimr-application"
#define PAGE_HANDLER void (*)( connection_t*, const char*, void* )

typedef struct website_data {
	zsocket_t* socket;
	char  status;
	char* pubdir;
	void  (*connection_handler)( connection_t* connection );
	int   conn_tries;
	int   default_pages_count;
	char  default_pages[ 100 ][ 100 ];
	char* redirect_url;
	void* udata;
} website_data_t;

const char* zimr_version( );
const char* zimr_build_date( );

bool zimr_init( );
int  zimr_cnf_load( char* cnf_path );
void zimr_start( );
void zimr_shutdown( );
bool zimr_send_hello( );
bool zimr_send_cmd( zsocket_t* socket, int cmd, void* message, int size );

void zimr_connection_handler( int sockfd, website_t* website );
void zimr_file_handler( int fd, ztransport_t* transport );

website_t* zimr_website_create( char* url );
void  zimr_website_destroy( website_t* website );
bool  zimr_website_enable( website_t* website );
void  zimr_website_disable( website_t* website );
void  zimr_website_set_redirect( website_t* website, char* redirect_url );
void  zimr_website_set_pubdir( website_t* website, const char* pubdir );
char* zimr_website_get_pubdir( website_t* website );
void  zimr_website_set_connection_handler ( website_t* website, void (*connection_handler)( connection_t* ) );
void  zimr_website_unset_connection_handler( website_t* website );
void  zimr_website_insert_default_page( website_t* website, const char* default_page, int pos );

void zimr_connection_send_status( ztransport_t* transport, connection_t* connection );
void zimr_connection_send_headers( ztransport_t* transport, connection_t* connection );
void zimr_connection_send_file( connection_t* connection, char* filepath, bool use_pubdir );
void zimr_connection_send( connection_t* connection, void* message, int size );
void zimr_connection_send_error( connection_t* connection, short code );
void zimr_connection_send_redirect( connection_t* connection, char* url );
void zimr_connection_default_page_handler( connection_t* connection, char* filepath );

void zimr_register_page_handler( const char* page_type, void (*page_handler)( connection_t*, const char*, void* ), void* udata );

bool zimr_open_request_log( );
void zimr_log_request( connection_t* connection );
void zimr_close_request_log( );
#endif
