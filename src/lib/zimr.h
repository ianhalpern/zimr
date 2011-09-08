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

#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdbool.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <regex.h>

#include "website.h"
#include "zsocket.h"
#include "msg_switch.h"
#include "zfildes.h"
#include "connection.h"
#include "userdir.h"
#include "mime.h"
#include "zcnf.h"
#include "dlog.h"
#include "md5.h"

#define DAEMON_NAME ZM_APP_EXEC
#define PAGE_HANDLER void (*)( connection_t*, const char*, void* )

#define ZIMR_EVENT_INITIALIZING               1
#define ZIMR_EVENT_MODULE_LOADING             2
#define ZIMR_EVENT_MODULE_LOAD_FAILED         3
#define ZIMR_EVENT_WEBSITE_ENABLE_SUCCEEDED   4
#define ZIMR_EVENT_WEBSITE_ENABLE_FAILED      5
#define ZIMR_EVENT_WEBSITE_MODULE_INIT        6
#define ZIMR_EVENT_WEBSITE_MODULE_INIT_FAILED 7
#define ZIMR_EVENT_ALL_WEBSITES_ENABLED       8
#define ZIMR_EVENT_REGISTERING_WEBSITES       9
#define ZIMR_EVENT_LOADING_CNF                10
#define ZIMR_EVENT_RESTART_REQUEST            11

typedef struct {
	int fd;
	int range_start;
	int range_end;
} fileread_data_t;

typedef struct {
	size_t size_received;
	size_t size;
	char* data;
	connection_t* connection;
	void (*onclose_event)( connection_t*, void* );
	void* onclose_udata;
	fileread_data_t fileread_data;
} conn_data_t;

typedef struct website_data {
	msg_switch_t* msg_switch;
	struct {
		char ip[16];
		int port;
	} proxy;
	char  status;
	void (*connection_handler)( connection_t* connection );
	void (*error_handler)( connection_t*, int error_code, char* error_message, size_t len );
	int   conn_tries;
	list_t pubdirs;
	list_t default_pages;
	list_t ignored_regexs;
	list_t page_handlers;
	list_t module_data;
	char* redirect_url;
	conn_data_t* connections[ FD_SETSIZE ];
	void* udata;
} website_data_t;

typedef struct {
	char name[ ZM_MODULE_NAME_MAX_LEN ];
	void* handle;
} module_t;

typedef struct {
	void* udata;
	module_t* module;
	int argc;
	char* argv[ ZM_MODULE_MAX_ARGS ];
} module_website_data_t;

const char* zimr_version();
const char* zimr_build_date();

bool zimr_init();
int  zimr_cnf_load( char* cnf_path );
void zimr_start();
void zimr_shutdown();
void zimr_restart();
void zimr_register_event_handler( void (*handler)( int, va_list ap ) );
module_t* zimr_load_module( const char* module_name );
module_t* (*zimr_get_module)( const char* module_name );
void  zimr_unload_module( module_t* );
void  zimr_reload_module( const char* module_name );

bool zimr_connection_handler( website_t* website, int msgid, void* buf, size_t len );
void zimr_file_handler( int fd, connection_t* connection );

website_t* zimr_website_create( char* url );
void  zimr_website_destroy( website_t* website );
bool  zimr_website_enable( website_t* website );
void  zimr_website_disable( website_t* website );
void  zimr_website_load_module( website_t*, module_t*, int, char** );
void  zimr_website_set_redirect( website_t* website, char* redirect_url );
void  zimr_website_insert_pubdir( website_t* website, char* pubdir, int pos );
//char* zimr_website_get_pubdir( website_t* website );
void  zimr_website_set_proxy( website_t* website, char* ip, int port );
void  zimr_website_set_connection_handler ( website_t* website, void (*connection_handler)( connection_t* ) );
void  zimr_website_set_error_handler( website_t* website, void (*error_handler)( connection_t*, int error_code, char* error_message, size_t len ) );
void  zimr_website_unset_connection_handler( website_t* website );
void  zimr_website_insert_default_page( website_t* website, const char* default_page, int pos );
void  zimr_website_insert_ignored_regex( website_t* website, const char* ignored_regex );
bool zimr_website_connection_exists( int fd, int msgid );
bool zimr_website_connection_sent( int fd, int msgid );

void zimr_connection_send_status( connection_t* connection );
void zimr_connection_send_headers( connection_t* connection );
void zimr_connection_send_file( connection_t* connection, char* filepath, bool use_pubdir );
void zimr_connection_send( connection_t* connection, void* message, int size );
void zimr_connection_send_error( connection_t* connection, short code, char* message, size_t message_size );
void zimr_connection_send_redirect( connection_t* connection, char* url );
void zimr_connection_write( connection_t* connection, void* message, int size );
void zimr_connection_close( connection_t* connection );
void zimr_connection_default_page_handler( connection_t* connection, char* filepath );
void zimr_connection_set_onclose_event( connection_t* connection, void (*onclose_handler)( connection_t*, void* ), void* );

void zimr_website_register_page_handler( website_t*, const char* page_type, void (*page_handler)( connection_t*, const char*, void* ), void* udata );

bool zimr_open_request_log();
void zimr_log_request( connection_t* connection );
void zimr_close_request_log();

#endif
