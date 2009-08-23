/*   Poroda - Next Generation Web Server
 *
 *+  Copyright (c) 2009 Ian Halpern
 *@  http://Poroda.org
 *
 *   This file is part of Poroda.
 *
 *   Poroda is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Poroda is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Poroda.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef _PD_PORODA_H
#define _PD_PORODA_H

#include <time.h>
#include <sys/stat.h>
#include <syslog.h>
#include <assert.h>
#include <stdbool.h>
#include <signal.h>

#include "website.h"
#include "ptransport.h"
#include "pfildes.h"
#include "connection.h"
#include "mime.h"
#include "pcnf.h"
#include "pderr.h"

#define DAEMON_NAME "poroda-application"
#define PAGE_HANDLER void (*)( connection_t*, const char*, void* )

typedef struct website_data {
	psocket_t* socket;
	char status;
	char* pubdir;
	void (*connection_handler)( connection_t* connection );
	void* udata;
	int conn_tries;
	int default_pages_count;
	char default_pages[ 100 ][ 100 ];
} website_data_t;

const char* poroda_version( );
const char* poroda_build_date( );

bool poroda_init( );
int  poroda_cnf_load( );
void poroda_start( );
void poroda_shutdown( );
bool poroda_send_hello( );
bool poroda_send_cmd( psocket_t* socket, int cmd, void* message, int size );

void poroda_connection_handler( int sockfd, website_t* website );
void poroda_file_handler( int fd, ptransport_t* transport );

website_t* poroda_website_create( char* url );
void  poroda_website_destroy( website_t* website );
bool  poroda_website_enable( website_t* website );
void  poroda_website_disable( website_t* website );
void  poroda_website_set_pubdir( website_t* website, const char* pubdir );
char* poroda_website_get_pubdir( website_t* website );
void  poroda_website_set_connection_handler ( website_t* website, void (*connection_handler)( connection_t* ) );
void  poroda_website_unset_connection_handler( website_t* website );
void  poroda_website_insert_default_page( website_t* website, const char* default_page, int pos );

void poroda_connection_send_status( ptransport_t* transport, connection_t* connection );
void poroda_connection_send_headers( ptransport_t* transport, connection_t* connection );
void poroda_connection_send_file( connection_t* connection, char* filepath, bool use_pubdir );
void poroda_connection_send( connection_t* connection, void* message, int size );
void poroda_connection_send_error( );
void poroda_connection_default_page_handler( connection_t* connection, char* filepath );

void poroda_register_page_handler( const char* page_type, void (*page_handler)( connection_t*, const char*, void* ), void* udata );
#endif
