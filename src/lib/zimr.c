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

#include "zimr.h"
static bool initialized = false;
static int reqlogfd = -1;

typedef struct {
	char page_type[ 10 ];
	void (*handler)( connection_t*, const char*, void* );
	void* udata;
} page_handler_t;

static list_t loaded_modules;

// module function definitions //////////////////
static void  (*modzimr_init)();
static void  (*modzimr_destroy)();
static void* (*modzimr_website_init)( website_t*, int, char** );
static void  (*modzimr_website_destroy)( website_t*, void* );
static void  (*modzimr_connection_new)( connection_t*, void* );
static void  (*modzimr_connection_closed)( connection_t*, void* );
static bool  (*modzimr_err_occured)();
/////////////////////////////////////////////////

static void (*event_handler)( int, va_list ap ) = NULL;

void command_response_handler( int fd, int msgid, void* buf, size_t len );
void cleanup_connection( int fd, int msgid );

void zimr_event( int evt, ... ) {
	if ( event_handler ) {
		va_list ap;
		va_start( ap, evt );
		event_handler( evt, ap );
		va_end( ap );
	}
}

const char* zimr_version() {
	return ZIMR_VERSION;
}

const char* zimr_build_date() {
	return BUILD_DATE;
}

bool zimr_init() {
	if ( initialized ) return true;

	zimr_event( ZIMR_EVENT_INITIALIZING );

	// call any needed library init functions
	assert( userdir_init( getuid() ) );
	website_init();
	zs_init( NULL, NULL );

	list_init( &loaded_modules );

	if ( !zimr_open_request_log() )
		return false;

	// Register file descriptor type handlers
	//zfd_register_type( INT_CONNECTED, ZFD_R, PFD_TYPE_HDLR zimr_connection_handler );
	//zfd_register_type( FILEREAD, ZFD_R, ZFD_TYPE_HDLR zimr_file_handler );

	initialized = true;
	return true;
}

void zimr_shutdown() {
	if ( !initialized ) return;

	initialized = false;

	while ( list_size( &websites ) )
		zimr_website_destroy( list_get_at( &websites, 0 ) );

	while ( list_size( &loaded_modules ) )
		zimr_unload_module( list_get_at( &loaded_modules, 0 ) );

	list_destroy( &websites );
	list_destroy( &loaded_modules );
	zimr_close_request_log();
}

void zimr_restart() {
	zimr_event( ZIMR_EVENT_RESTART_REQUEST );
}

void zimr_register_event_handler( void (*handler)( int, va_list ap ) ) {
	event_handler = handler;
}

int zimr_cnf_load( char* cnf_path ) {
	zimr_event( ZIMR_EVENT_LOADING_CNF, cnf_path );

	if ( access( cnf_path, F_OK ) == -1 ) return 0;

	zcnf_app_t* cnf = zcnf_app_load( cnf_path );
	if ( !cnf ) return 0;

	int j = 0;
	for ( ; j < list_size( &cnf->websites ); j++ ) {
		zcnf_website_t* website_cnf = list_get_at( &cnf->websites, j );
		int i;
		if ( website_cnf->url ) {

			website_t* website = zimr_website_create( website_cnf->url );

			zimr_website_set_proxy( website, cnf->proxy.ip, cnf->proxy.port );

			//if ( list_size( &website_cnf->pubdirs ) )
				//zimr_website_insert_pubdir( website, NULL, 0 );

			for ( i = 0; i < list_size( &website_cnf->pubdirs ); i++ )
				zimr_website_insert_pubdir( website, list_get_at( &website_cnf->pubdirs, i ), i );

			if ( website_cnf->redirect_url )
				zimr_website_set_redirect( website, website_cnf->redirect_url );

			if ( website_cnf->ip_address )
				strcpy( website->ip, website_cnf->ip_address );

			for ( i = 0; i < list_size( &website_cnf->modules ); i++ ) {
				zcnf_module_t* module_cnf = list_get_at( &website_cnf->modules, i );
				module_t* module = zimr_load_module( module_cnf->name );
				if ( !module ) return 0;
				zimr_website_load_module( website, module, module_cnf->argc, module_cnf->argv );
			}

			for ( i = 0; i < list_size( &website_cnf->ignore ); i++ )
				zimr_website_insert_ignored_regex( website, list_get_at( &website_cnf->ignore, i ) );

			zimr_website_enable( website );
		}
	}

	zcnf_app_free( cnf );
	return 1;
}

void zimr_start() {
	zimr_event( ZIMR_EVENT_REGISTERING_WEBSITES, websites );

	int i = 0;
	website_t* website;
	website_data_t* website_data;

	// TODO: also check to see if there are any enabled websites.
	if ( !list_size( &websites ) )
		dlog( stderr, "zimr_start() failed: No websites created" );

	do {
		do {
			msg_switch_select();
			zs_select();
		} while ( msg_switch_need_select() || zs_need_select() );

		int n_enabled = 0;

		// Test and Start website proxies
		for ( i = 0; i < list_size( &websites ); i++ ) {
			website = list_get_at( &websites, i );
			website_data = (website_data_t*) website->udata;

			if ( website->sockfd == -1 && website_data->status == WS_STATUS_WANT_ENABLE && website_data->conn_tries != -1 ) {
				/* connect to the Zimr Daemon Proxy for routing external
				   requests or commands to this process. */
				if ( !zimr_website_enable( website ) ) {
					if ( website_data->conn_tries == 0 ) {
						zimr_event( ZIMR_EVENT_WEBSITE_ENABLE_FAILED, website->full_url, website_data->proxy.ip, website_data->proxy.port,
						"could not connect to proxy...will retry." );
						dlog( stderr, "%s could not connect to proxy...will retry.", website->full_url );
					}


					// giving up ...
					/*else {
						dlog( stderr, "\"%s\" is destroying: %s", website->url, strerror( errno ) );
						//zimr_website_disable( website );
						zimr_website_destroy( website );
					}*/
				}
			} else if ( website_data->status == WS_STATUS_ENABLED ) {
				n_enabled++;
			}

		}

		if ( n_enabled && n_enabled == list_size( &websites ) )
			zimr_event( ZIMR_EVENT_ALL_WEBSITES_ENABLED );

	} while ( list_size( &websites ) && zfd_select(2) );

}

module_t* zimr_load_module( const char* module_name ) {
	if ( strlen( module_name ) >= ZM_MODULE_NAME_MAX_LEN ) {
		dlog( stderr, "module name excedes max length: %s\n", module_name );
		return NULL;
	}

	module_t* module = NULL;

	int i;
	for ( i = 0; i < list_size( &loaded_modules ); i++ ) {
		module = list_get_at( &loaded_modules, i );
		if ( strcmp( module_name, module->name ) == 0 )
			break;
		module = NULL;
	}

	if ( !module ) {
		zimr_event( ZIMR_EVENT_MODULE_LOADING, module_name );

		char module_filename[ strlen( module_name ) + 6 ];
		strcpy( module_filename, "mod" );
		strcat( module_filename, module_name );
		strcat( module_filename, ".so" );

		void* handle = dlopen( module_filename, RTLD_NOW | RTLD_GLOBAL );
		if ( handle == NULL ) {

			char module_fullpath[ 256 ];
			expand_tilde( ZM_USR_MODULE_DIR, module_fullpath, sizeof( module_fullpath ), getuid() );
			strcat( module_fullpath, module_filename );
			handle = dlopen( module_fullpath, RTLD_NOW | RTLD_GLOBAL );
			if ( handle == NULL ) {
				// report error ...
				fprintf( stderr, "%s\n", dlerror() );
				return NULL;
			}
		}

		module = malloc( sizeof( module_t ) );
		strcpy( module->name, module_name );
		module->handle = handle;

		list_append( &loaded_modules, module );

		*(void**) (&modzimr_init) = dlsym( module->handle, "modzimr_init" );
		*(bool**) (&modzimr_err_occured) = dlsym( module->handle, "modzimr_err_occured" );
		if ( modzimr_init ) {
			(*modzimr_init)();
			if ( modzimr_err_occured && (*modzimr_err_occured)() ) {
				zimr_event( ZIMR_EVENT_MODULE_LOAD_FAILED, module_name );
				return NULL;
			}
		}
	}

	return module;
}

module_t* (*zimr_get_module)( const char* module_name ) = zimr_load_module;

void zimr_unload_module( module_t* module ) {

	int i;
	for ( i = 0; i < list_size( &loaded_modules ); i++ ) {
		if ( strcmp( module->name, ((module_t*)list_get_at( &loaded_modules, i ))->name ) == 0 ) {
			list_extract_at( &loaded_modules, i );
			break;
		}
	}

	*(void**) (&modzimr_destroy) = dlsym( module->handle, "modzimr_destroy" );
	if ( modzimr_destroy ) (*modzimr_destroy)();

	dlclose( module->handle );
	free( module );
}

void zimr_reload_module( const char* module_name ) {
	int i, j, k = 0;
	website_t* module_websites[10];
	module_website_data_t* module_data_list[10];

	for ( i = 0; i < list_size( &websites ); i++ ) {

		website_t* website = list_get_at( &websites, i );
		website_data_t* website_data = website->udata;

		for ( j = 0; j < list_size( &website_data->module_data ); j++ ) {
			module_website_data_t* module_data = list_get_at( &website_data->module_data, j );
			if ( strcmp( module_data->module->name, module_name ) == 0 ) {
				list_extract_at( &website_data->module_data, j );
				*(void **)(&modzimr_website_destroy) = dlsym( module_data->module->handle, "modzimr_website_destroy" );
				if ( modzimr_website_destroy ) (*modzimr_website_destroy)( website, module_data->udata );
				module_data_list[k] = module_data;
				module_websites[k++] = website;
				break;
			}
		}
	}

	zimr_unload_module( zimr_get_module( module_name ) );

	module_t* module = zimr_load_module( module_name );

	for ( i = 0; i < k; i++ ) {
		// TODO: store and pass argc, argv
		zimr_website_load_module( module_websites[i], module, module_data_list[i]->argc, module_data_list[i]->argv );
		for ( j = 0; j < module_data_list[i]->argc; j++ )
			free( module_data_list[i]->argv[j] );
	}
}

int ignored_regex_check( const char* path, regex_t* ignored_regex ) {
	regmatch_t result;
	if ( regexec( ignored_regex, path, 1, &result, 0 ) == 0 )
		return 0;
	return 1;
}

website_t* zimr_website_create( char* url ) {
	website_t* website;
	website_data_t* website_data;

	if ( !( website = website_add( -1, url, NULL ) ) )
		return NULL;

	website_data = (website_data_t*) malloc( sizeof( website_data_t ) );
	website->udata = (void*) website_data;

	website_data->msg_switch = NULL;
	website_data->status = WS_STATUS_DISABLED;
	website_data->connection_handler = NULL;
	website_data->error_handler = NULL;
	website_data->conn_tries = 0; //ZM_NUM_PROXY_DEATH_RETRIES - 1;
	website_data->redirect_url = NULL;
	strcpy( website_data->proxy.ip, ZM_PROXY_DEFAULT_ADDR );
	website_data->proxy.port = ZM_PROXY_DEFAULT_PORT;
	memset( website_data->connections, 0, sizeof( website_data->connections ) );
	list_init( &website_data->pubdirs );
	list_init( &website_data->default_pages );
	list_init( &website_data->ignored_regexs );
	list_init( &website_data->page_handlers );
	list_init( &website_data->module_data );

	list_attributes_comparator( &website_data->ignored_regexs, (element_comparator) ignored_regex_check );

	zimr_website_insert_pubdir( website, "./", 0 );

	zimr_website_insert_default_page( website, "default.html", 0 );
	zimr_website_insert_ignored_regex( website, "^zimr\\.webapp$" );
	zimr_website_insert_ignored_regex( website, "^zimr\\.log$" );
	zimr_website_insert_ignored_regex( website, "^zimr\\.request\\.log$" );

	return website;
}

void zimr_website_destroy( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->udata;

	zimr_website_disable( website );

	while ( list_size( &website_data->module_data ) ) {
		module_website_data_t* module_data = list_fetch( &website_data->module_data );
		*(void **)(&modzimr_website_destroy) = dlsym( module_data->module->handle, "modzimr_website_destroy" );
		if ( modzimr_website_destroy ) (*modzimr_website_destroy)( website, module_data->udata );
		int j;
		for ( j = 0; j < module_data->argc; j++ )
			free( module_data->argv[j] );
		free( module_data );
	}
	list_destroy( &website_data->module_data );

	while ( list_size( &website_data->pubdirs ) )
		free( list_fetch( &website_data->pubdirs ) );

	// TODO: free memory before destroying
	list_destroy( &website_data->default_pages );
	list_destroy( &website_data->ignored_regexs );
	list_destroy( &website_data->page_handlers );

	free( website_data );
	website_remove( website );
}

void command_response_handler( int fd, int msgid, void* buf, size_t len ) {
	website_t* website = website_get_by_sockfd( fd );
	website_data_t* website_data = website->udata;
	switch ( msg_get_type( fd, msgid ) ) {
		case ZM_CMD_WS_START:
			if ( strcmp( buf, "OK" ) == 0 ) {
				// Everything is good, start listening for requests.
				website_data->status = WS_STATUS_ENABLED;

				zimr_event( ZIMR_EVENT_WEBSITE_ENABLE_SUCCEEDED, website->full_url, website_data->proxy.ip, website_data->proxy.port );
				dlog( stderr, "%s is enabled.", website->full_url );
				website_data->conn_tries = 0;

			} else {
				zimr_event( ZIMR_EVENT_WEBSITE_ENABLE_FAILED, website->full_url, website_data->proxy.ip, website_data->proxy.port, buf + 5 );
				dlog( stderr, "%s failed to enable:\n%s", website->full_url, buf + 5 );
				// WS_START_CMD failed...close socket
				//website_data->status = WS_STATUS_WANT_ENABLE;
				zimr_website_disable( website );
				//msg_clr_read( website->sockfd, msgid );
				//zs_close( website->sockfd );
				//website->sockfd = -1;
				//website_data->conn_tries = -1; // we do not want to retry
			}
			break;
	}

	msg_close( fd, msgid );
}

void cleanup_fileread( website_data_t* website_data, int sockfd ) {
	if ( website_data->connections[ sockfd ]->fileread_data.fd != -1 ) {
		zfd_clr( website_data->connections[ sockfd ]->fileread_data.fd, ZFD_R );
		close( website_data->connections[ sockfd ]->fileread_data.fd );
		website_data->connections[ sockfd ]->fileread_data.fd = -1;
		//printf( "[%d] closing filereadfd %d\n", sockfd, website_data->connections[ sockfd ]->filereadfd );
	}
}

void cleanup_connection( int fd, int msgid ) {
	//printf( "cleanup %d %d\n", fd, msgid );
//	printf( "connection %d cleanup\n", msgid );
	website_t* website = website_get_by_sockfd( fd );
	website_data_t* website_data = website->udata;

	msg_close( fd, msgid );
	cleanup_fileread( website_data, msgid );

	int i = 0;
	for ( i = 0; i < list_size( &website_data->module_data ); i++ ) {
		module_website_data_t* module_data = list_get_at( &website_data->module_data, i );
		*(void **)(&modzimr_connection_closed) = dlsym( module_data->module->handle, "modzimr_connection_closed" );
		if ( modzimr_connection_closed ) (*modzimr_connection_closed)( website_data->connections[ msgid ]->connection, module_data->udata );
	}

	if ( website_data->connections[ msgid ]->onclose_event )
		website_data->connections[ msgid ]->onclose_event( website_data->connections[ msgid ]->connection,
		  website_data->connections[ msgid ]->onclose_udata );

	if ( website_data->connections[ msgid ]->connection )
		connection_free( website_data->connections[ msgid ]->connection );
	free( website_data->connections[ msgid ]->data );
	free( website_data->connections[ msgid ] );
	website_data->connections[ msgid ] = NULL;
}

bool zimr_website_connection_exists( int fd, int msgid ) {
	website_t* website = website_get_by_sockfd( fd );
	website_data_t* website_data = website->udata;
	return website_data->connections[ msgid ];
}

bool zimr_website_connection_sent( int fd, int msgid ) {
	website_t* website = website_get_by_sockfd( fd );
	website_data_t* website_data = website->udata;
	if ( !website_data->connections[ msgid ] )
		return true;
	if ( FL_ISSET( website_data->connections[ msgid ]->connection->status, CONN_STATUS_SENT_HEADERS ) )
		return true;
	return false;
}

void msg_event_handler( int fd, int msgid, int event ) {
	website_t* website = website_get_by_sockfd( fd );
	website_data_t* website_data = website->udata;
	char* buf[PACK_DATA_SIZE];
	ssize_t n;
	//printf( "event 0x%03x for msg %d\n", event, msgid );

	switch ( event ) {
		case MSG_EVT_ACCEPT_READY:
			n = msg_accept( fd, msgid );
			if ( n != 0 )
				msg_set_read( fd, msgid );
			break;
		case MSG_EVT_WRITE_READY:
			if ( msgid >= 0 && website_data->connections[ msgid ]->fileread_data.fd != -1 ) {
				msg_clr_write( fd, msgid );
				zfd_set( website_data->connections[ msgid ]->fileread_data.fd, ZFD_R,
				  ZFD_HDLR zimr_file_handler, website_data->connections[ msgid ]->connection );
			}
			break;
		case MSG_EVT_READ_READY:
			n = msg_read( fd, msgid, buf, sizeof( buf ) );
			if ( n <= 0 ) {
				cleanup_connection( fd, msgid );
				return;
			}

			if ( msg_get_type( fd, msgid ) < 0 )
				command_response_handler( fd, msgid, buf, n );
			else if ( !zimr_connection_handler( website, msgid, buf, n ) ) {
				cleanup_connection( fd, msgid );
				return;
			}
			break;
		case MSG_SWITCH_EVT_IO_ERROR:
			zimr_website_disable( website );
			zimr_website_enable( website );
			break;
	}
}

bool zimr_website_enable( website_t* website ) {
	website_data_t* website_data = website->udata;

	if ( website_data->status != WS_STATUS_WANT_ENABLE && website_data->status != WS_STATUS_DISABLED ) {
		return false;
	}

	website_data->status = WS_STATUS_WANT_ENABLE;

	website->sockfd = zsocket( inet_addr( website_data->proxy.ip ), website_data->proxy.port, ZSOCK_CONNECT, NULL, false );

	if ( website->sockfd == -1 ) {
		return false;
	}

	website_data->status = WS_STATUS_ENABLING;

	msg_switch_create( website->sockfd, msg_event_handler );

	// send website enable command
	int msgid = msg_open( website->sockfd, ZM_CMD_WS_START );
	msg_write( website->sockfd, msgid, website->full_url, strlen( website->full_url ) + 1 );
	msg_write( website->sockfd, msgid, website->ip, strlen( website->ip ) + 1 );
	msg_flush( website->sockfd, msgid );
	msg_set_read( website->sockfd, msgid );

	return true;
}

void zimr_website_disable( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->udata;

	/* we only need to send command if the socket is connected. */
	if ( website->sockfd != -1 ) {

		int i;
		for ( i = 0; i < FD_SETSIZE; i++ ) {
			if ( website_data->connections[ i ] )
				cleanup_connection( website->sockfd, i );
		}

		if ( website_data->status == WS_STATUS_ENABLED ) {
			int msgid = msg_open( website->sockfd, ZM_CMD_WS_STOP );
			msg_write( website->sockfd, msgid, "STOP", 5 );
			msg_close( website->sockfd, msgid );
		}

		msg_switch_destroy( website->sockfd );
		zs_close( website->sockfd );
		website->sockfd = -1;
	}

	website_data->status = WS_STATUS_DISABLED;
	dlog( stderr, "%s is disabled.", website->full_url );
}

static void zimr_website_default_connection_handler( connection_t* connection ) {
	zimr_connection_send_file( connection, connection->request.url, true );
}

bool zimr_connection_handler( website_t* website, int msgid, void* buf, size_t len ) {
	website_data_t* website_data = website->udata;
	conn_data_t* conn_data;

	if ( !website_data->connections[ msgid ] ) {
		website_data->connections[ msgid ] = (conn_data_t*) malloc( sizeof( conn_data_t ) );
		website_data->connections[ msgid ]->connection = NULL;
		website_data->connections[ msgid ]->onclose_event = NULL;
		website_data->connections[ msgid ]->onclose_udata = NULL;
		website_data->connections[ msgid ]->fileread_data.fd = -1;
		website_data->connections[ msgid ]->data = strdup( "" );
		website_data->connections[ msgid ]->size_received = 0;
		website_data->connections[ msgid ]->size = *(size_t*)buf;
		buf += sizeof( size_t );
		len -= sizeof( size_t );
	}

	conn_data = website_data->connections[ msgid ];
//	printf( "1: %zu of %zu received\n", conn_data->size_received, conn_data->size );

	if ( conn_data->size_received >= conn_data->size ) return false;

	char* tmp;
	tmp = conn_data->data;
	conn_data->data = (char*) malloc( len + conn_data->size_received );
	memset( conn_data->data, 0, len + conn_data->size_received );
	memcpy( conn_data->data, tmp, conn_data->size_received );
	memcpy( conn_data->data + conn_data->size_received, buf, len );
	conn_data->size_received += len;
	free( tmp );

//	printf( "2: %zu of %zu received\n", conn_data->size_received, conn_data->size );
	if ( conn_data->size_received > conn_data->size ) return false;

	if ( conn_data->size_received == conn_data->size ) {
		conn_data->connection = connection_create( website, msgid, conn_data->data, conn_data->size );

		if ( !conn_data->connection ) {
			return false;
		}
		//printf( "connection %d new\n", conn_data->connection->sockfd );

		// start optimistically
		response_set_status( &conn_data->connection->response, 200 );
		// To ensure thier position at the top of the headers
		headers_set_header( &conn_data->connection->response.headers, "Date", "" );
		headers_set_header( &conn_data->connection->response.headers, "Server", "" );

		int i = 0;
		for ( i = 0; i < list_size( &website_data->module_data ); i++ ) {
			module_website_data_t* module_data = list_get_at( &website_data->module_data, i );
			*(void **)(&modzimr_connection_new) = dlsym( module_data->module->handle, "modzimr_connection_new" );
		//	printf( "connection @ 0x%x\n", &conn_data->connection );
			if ( modzimr_connection_new ) (*modzimr_connection_new)( conn_data->connection, module_data->udata );
			if ( zimr_website_connection_sent( website->sockfd, msgid ) )
				return true;
		}

		if ( website_data->redirect_url ) {
			size_t url_len = strlen( website_data->redirect_url ) + strlen( conn_data->connection->request.url );
			size_t qs_len = params_qs_len( &conn_data->connection->request.params );
			char buf[ url_len + qs_len + 9 ];
			sprintf( buf, "%s%s", website_data->redirect_url, conn_data->connection->request.url );
			if ( qs_len ) {
				strcat( buf + url_len, "?" );
				params_gen_qs( &conn_data->connection->request.params, buf + url_len + 1 );
			}
			zimr_connection_send_redirect( conn_data->connection, buf );
		}

		else if ( website_data->connection_handler )
			website_data->connection_handler( conn_data->connection );

		else
			zimr_website_default_connection_handler( conn_data->connection );
	}

	return true;
}

void zimr_connection_set_onclose_event( connection_t* connection, void (*onclose_event)( connection_t*, void* ), void* udata ) {
	website_data_t* website_data = connection->website->udata;
	int msgid = connection->sockfd;

	website_data->connections[ msgid ]->onclose_event = onclose_event;
	website_data->connections[ msgid ]->onclose_udata = udata;
}

void zimr_connection_send_status( connection_t* connection ) {
	char status_line[ 100 ];
	sprintf(
		status_line,
		"HTTP/%s %s" HTTP_HDR_ENDL,
		connection->http_version,
		response_status( connection->response.http_status )
	);

	msg_write( connection->website->sockfd, connection->sockfd, status_line, strlen( status_line ) );

	FL_SET( connection->status, CONN_STATUS_SENT_STATUS );
}

void zimr_connection_send_headers( connection_t* connection ) {
	time_t now;
	char now_str[80];

	time( &now );
	strftime( now_str, 80, "%a, %d %b %Y %I:%M:%S %Z", gmtime( &now ) );

	headers_set_header( &connection->response.headers, "Date", now_str );
	headers_set_header( &connection->response.headers, "Server", "Zimr/" ZIMR_VERSION );

	char* cookies = cookies_to_string( &connection->cookies, (char*) malloc( 1024 ), 1024 );

	if ( strlen( cookies ) )
		headers_set_header( &connection->response.headers, "Set-Cookie", cookies );

	char* headers = headers_to_string( &connection->response.headers, (char*) malloc( 1024 ) );

	msg_write( connection->website->sockfd, connection->sockfd, (void*) headers, strlen( headers ) );
	free( cookies );
	free( headers );

	zimr_log_request( connection );

	FL_SET( connection->status, CONN_STATUS_SENT_HEADERS );
}

void zimr_connection_send( connection_t* connection, void* message, int size ) {
	char sizebuf[32];

	if ( !headers_get_header( &connection->response.headers, "Content-Type" ) ) {
		headers_set_header( &connection->response.headers, "Content-Type", mime_get_type( ".html" ) );
	}

	sprintf( sizebuf, "%d", size );
	headers_set_header( &connection->response.headers, "Content-Length", sizebuf );

	zimr_connection_send_status( connection );
	zimr_connection_send_headers( connection );

	msg_write( connection->website->sockfd, connection->sockfd, (void*) message, size );

	cleanup_connection( connection->website->sockfd, connection->sockfd );
}

void zimr_connection_write( connection_t* connection, void* message, int size ) {
	msg_write( connection->website->sockfd, connection->sockfd, (void*) message, size );
	msg_flush( connection->website->sockfd, connection->sockfd );
}

void zimr_connection_close( connection_t* connection ) {
	cleanup_connection( connection->website->sockfd, connection->sockfd );
}

void zimr_connection_send_file( connection_t* connection, char* filepath, bool use_pubdir ) {
	website_data_t* website_data = connection->website->udata;
	struct stat file_stat;
	char full_filepath[256] = "";
	char* ptr;
	int i;

	// if the filepath is relative to the specified public directory, initialize
	// the filepath with the public directory.
	if ( use_pubdir && filepath[0] != '/' ) {
		for ( i = 0; i < list_size( &website_data->pubdirs ); i++ ) {
			strcpy( full_filepath, list_get_at( &website_data->pubdirs, i ) );

			strcat( full_filepath, filepath );
			if ( stat( full_filepath, &file_stat ) ) {
				if ( i == list_size( &website_data->pubdirs ) - 1 ) {
					// Does not exist.
					zimr_connection_send_error( connection, 404, NULL, 0 );
					return;
				}
			} else break;
		}
	} else {
		strcpy( full_filepath, filepath );
	}

	if ( S_ISDIR( file_stat.st_mode ) ) {
		if ( connection->request.full_url[ strlen( connection->request.full_url ) - 1 ] != '/' ) {
			if ( ( ptr = strrchr( connection->request.full_url, '/' ) ) ) ptr++;
			else ptr = connection->request.full_url;

			char buf[ strlen( ptr ) + 1 ];
			sprintf( buf, "%s/", ptr );
			zimr_connection_send_redirect( connection, buf );
			return;
		}

		// search for the directory default page
		ptr = full_filepath + strlen( full_filepath );
		for ( i = 0; i < list_size( &website_data->default_pages ); i++ ) {
			strcpy( ptr, list_get_at( &website_data->default_pages, i ) );
			if ( !stat( full_filepath, &file_stat ) )
				break; // default file exists...break loop
		}

		if ( i == list_size( &website_data->default_pages ) )
			*ptr = 0;

	}

	// check if file is ignored
	if ( list_contains( &website_data->ignored_regexs, filepath ) ) {
		// Does not exist.
		zimr_connection_send_error( connection, 404, NULL, 0 );
		return;
	}

	// search for a page handler by file extension
	ptr = strrchr( full_filepath, '.' );

	if ( ptr != NULL && ptr[ 1 ] != '\0' ) {
		ptr++; // skip over "."

		for ( i = 0 ; i < list_size( &website_data->page_handlers ); i++ ) {
			page_handler_t* page_handler = list_get_at( &website_data->page_handlers, i );
			if ( strcmp( page_handler->page_type, ptr ) == 0 ) {
				page_handler->handler( connection, full_filepath, page_handler->udata );
				return;
			}
		}

	}

	// if you have made it this far, use the default page handler.
	zimr_connection_default_page_handler( connection, full_filepath );

}

void zimr_connection_send_error( connection_t* connection, short code, char* message, size_t message_size ) {
	char sizebuf[10];

	if ( !connection->sending_error && ((website_data_t*) connection->website->udata )->error_handler ) {
		connection->sending_error = true; // Prevents recursively calling zimr_connection_send_error from the custom error_handler
		int fd = connection->website->sockfd, msgid = connection->sockfd;
		((website_data_t*) connection->website->udata )->error_handler( connection, code, message, message_size );
		if ( zimr_website_connection_sent( fd, msgid ) ) return;
	}

	response_set_status( &connection->response, code );
	//printf( "connection %d send error\n", connection->sockfd );
	headers_set_header( &connection->response.headers, "Content-Type", mime_get_type( ".html" ) );

	char error_msg_top[] =
	"<!doctype html>"
	"<html>"
	"<body>"
	"<h1>";
	char error_msg_middle[] =
	"</h1>"
	"<pre>";
	char error_msg_bottom[] =
	"</pre>"
	"</body>"
	"</html>";

	sprintf( sizebuf, "%zu", strlen( response_status( code ) ) + message_size
	  + sizeof( error_msg_top ) + sizeof( error_msg_middle ) + sizeof( error_msg_bottom ) - 3 );
	headers_set_header( &connection->response.headers, "Content-Length", sizebuf );

	zimr_connection_send_status( connection );
	zimr_connection_send_headers( connection );

	msg_write( connection->website->sockfd, connection->sockfd,
	  (void*) error_msg_top, sizeof( error_msg_top ) - 1 );
	msg_write( connection->website->sockfd, connection->sockfd,
	  (void*) response_status( code ), strlen( response_status( code ) ) );
	msg_write( connection->website->sockfd, connection->sockfd,
	  (void*) error_msg_middle, sizeof( error_msg_middle ) - 1 );
	msg_write( connection->website->sockfd, connection->sockfd,
	  (void*) message, message_size );
	msg_write( connection->website->sockfd, connection->sockfd,
	  (void*) error_msg_bottom, sizeof( error_msg_bottom ) - 1 );

	cleanup_connection( connection->website->sockfd, connection->sockfd );
}

void zimr_connection_send_redirect( connection_t* connection, char* url ) {

	headers_set_header( &connection->response.headers, "Location", url );

	response_set_status( &connection->response, 301 );
	zimr_connection_send_status( connection );
	zimr_connection_send_headers( connection );

	cleanup_connection( connection->website->sockfd, connection->sockfd );
}

int directory_sort( const char* file1, const char* file2 ) {
	int i, max = strlen( file1 ), tmp = strlen( file2 );
	if ( tmp > max ) max = tmp;
	for ( i = 0; file1[i] == file2[i]; i++ )
		if ( i == max ) return 0;
	return file1[i] > file2[i] ? -1 : 1;
}

void zimr_connection_send_dir( connection_t* connection, char* filepath ) {
	website_data_t* website_data = connection->website->udata;
	int i;
	DIR* dir;
	struct dirent* dp;
	if ( ( dir = opendir( filepath ) ) == NULL) {
		zimr_connection_send_error( connection, 404, NULL, 0 );
		return;
	}

	list_t files;
	list_init( &files );

	list_attributes_comparator( &files, (element_comparator) directory_sort );

	char html_header_fmt[] = "<html><style type=\"text/css\">html, body { font-family: sans-serif; }</style><body>\n<h1>%s</h1>\n";
	char html_file_fmt[]   = "<a href=\"%s\">%s</a><br/>\n";
	char html_dir_fmt[]    = "<a href=\"%s/\">%s/</a><br/>\n";
	char html_header[ strlen( html_header_fmt ) + strlen( connection->request.url ) ];
	sprintf( html_header, html_header_fmt, connection->request.url );
	char html_footer[] = "<br/>"  "Zimr/" ZIMR_VERSION "\n</body></html>";

	int size = strlen( html_header ) + strlen( html_footer );
	while ( ( dp = readdir( dir ) ) != NULL ) {
		char fullpath[ strlen( filepath ) + strlen( dp->d_name ) + 1 ];
		sprintf( fullpath, "%s%s", filepath, dp->d_name );

		struct stat file_stat;
		if ( stat( fullpath, &file_stat ) ) {
			continue;
		}

		char externpath[ strlen( connection->request.url ) + strlen( dp->d_name ) + 1 ];
		strcpy( externpath, connection->request.url );
		strcat( externpath, dp->d_name );

		if ( list_contains( &website_data->ignored_regexs, externpath ) || strcmp( dp->d_name, "." ) == 0 )
			continue;

		if ( S_ISDIR( file_stat.st_mode ) ) {
			list_append( &files, malloc( strlen( dp->d_name ) * 2 + sizeof( html_dir_fmt ) + 1 ) );
			sprintf( list_get_at( &files, list_size( &files ) - 1 ), html_dir_fmt, dp->d_name, dp->d_name );
		} else {
			list_append( &files, malloc( strlen( dp->d_name ) * 2 + sizeof( html_file_fmt ) + 1 ) );
			sprintf( list_get_at( &files, list_size( &files ) - 1 ), html_file_fmt, dp->d_name, dp->d_name );
		}
		size += strlen( list_get_at( &files, list_size( &files ) - 1 ) );
	}
	closedir( dir );

	char sizebuf[ 10 ];
	response_set_status( &connection->response, 200 );
	headers_set_header( &connection->response.headers, "Content-Type", mime_get_type( ".html" ) );
	sprintf( sizebuf, "%d", size );
	headers_set_header( &connection->response.headers, "Content-Length", sizebuf );

	zimr_connection_send_status( connection );
	zimr_connection_send_headers( connection );

	msg_write( connection->website->sockfd, connection->sockfd, html_header, strlen( html_header ) );

	list_sort( &files, 1 );
	for ( i = 0; i < list_size( &files ); i++ ) {
		msg_write( connection->website->sockfd, connection->sockfd, list_get_at( &files, i ), strlen( list_get_at( &files, i ) ) );
		free( list_get_at( &files, i ) );
	}

	msg_write( connection->website->sockfd, connection->sockfd, html_footer, strlen( html_footer ) );

	//msg_close( connection->website->sockfd, connection->sockfd );
	cleanup_connection( connection->website->sockfd, connection->sockfd );

	list_destroy( &files );
}

static const char* etag_gen( char* path, time_t* mtime ) {
	md5_state_t state;
	md5_byte_t digest[16];
	static char hex_output[16*2 + 1 + 2];
	int di;

	md5_init( &state );
	md5_append( &state, (const md5_byte_t*) path, strlen( path ) );
	md5_append( &state, (const md5_byte_t*) mtime, sizeof( mtime ) );
	md5_finish( &state, digest );

	for ( di = 0; di < 16; ++di )
	    sprintf( hex_output + ( di * 2 + 1 ), "%02x", digest[di] );

	hex_output[0] = '"';
	hex_output[16*2 + 1] = '"';

	return hex_output;
}

void zimr_connection_default_page_handler( connection_t* connection, char* filepath ) {
	int fd = -1, range_start = 0, range_end = 0;
	struct stat file_stat;
	char sizebuf[ 32 ];

	if ( stat( filepath, &file_stat ) ) {
		zimr_connection_send_error( connection, 404, NULL, 0 );
		return;
	}

	if ( S_ISDIR( file_stat.st_mode ) ) {
		zimr_connection_send_dir( connection, filepath );
		return;
	}

	if ( ( fd = open( filepath, O_RDONLY ) ) < 0 ) {
		zimr_connection_send_error( connection, 404, NULL, 0 );
		return;
	}
	range_end = (int) file_stat.st_size;

	website_data_t* website_data = connection->website->udata;

	if ( !headers_get_header( &connection->response.headers, "Content-Type" ) )
		headers_set_header( &connection->response.headers, "Content-Type", mime_get_type( filepath ) );

	if ( !headers_get_header( &connection->response.headers, "Accept-Ranges" ) )
		headers_set_header( &connection->response.headers, "Accept-Ranges", "bytes" );

	header_t* last_modified = headers_get_header( &connection->response.headers, "Last-Modified" );
	if ( !last_modified ) {
		char mtime_str[80];
		strftime( mtime_str, 80, "%a, %d %b %Y %I:%M:%S %Z", gmtime( &file_stat.st_mtime ) );
		last_modified = headers_set_header( &connection->response.headers, "Last-Modified", mtime_str );
	}

	header_t* etag = headers_get_header( &connection->response.headers, "ETag" );
	if ( !etag ) {
		etag = headers_set_header( &connection->response.headers, "ETag", etag_gen( filepath, &file_stat.st_mtime ) );
	}

	header_t* cache_control = headers_get_header( &connection->request.headers, "Cache-Control" );
	if ( !cache_control || ( !strstr( cache_control->value, "max-age=0" ) && !strstr( cache_control->value, "no-cache" ) ) ) {
		header_t* if_modified_since = headers_get_header( &connection->request.headers, "If-Modified-Since" );
		header_t* if_none_match     = headers_get_header( &connection->request.headers, "If-None-Match" );
		if ( ( if_modified_since && strcmp( if_modified_since->value, last_modified->value ) == 0 )
		 &&  ( if_none_match     && strcmp( if_none_match->value, etag->value ) == 0 ) ) {
			response_set_status( &connection->response, 304 );
			zimr_connection_send( connection, "", 0 );
			return;
		}
	}

	header_t* range;
	if ( ( range = headers_get_header( &connection->request.headers, "Range" ) ) ) {
		response_set_status( &connection->response, 206 );
		int tmp_start, tmp_end;
		headers_header_range_parse( range, &tmp_start, &tmp_end );

		if ( tmp_end == -1 ) tmp_end = range_end;

		if ( tmp_start >=0 && tmp_end <= range_end && tmp_start <= tmp_end ) {
			char rangebuf[ 100 ];
			sprintf( rangebuf, "bytes %d-%d/%d", tmp_start, tmp_end, range_end );
			headers_set_header( &connection->response.headers, "Content-Range", rangebuf );
			range_start = tmp_start;
			range_end = tmp_end;
		}
	}

	if ( range_start )
		lseek( fd, range_start, SEEK_SET );

	sprintf( sizebuf, "%d", range_end - range_start );
	headers_set_header( &connection->response.headers, "Content-Length", sizebuf );

	zimr_connection_send_status( connection );
	zimr_connection_send_headers( connection );

	//printf( "[%d] opening filereadfd %d\n", connection->sockfd, fd );
	website_data->connections[ connection->sockfd ]->fileread_data.fd = fd;
	website_data->connections[ connection->sockfd ]->fileread_data.range_start = range_start;
	website_data->connections[ connection->sockfd ]->fileread_data.range_end = range_end;

	msg_set_write( connection->website->sockfd, connection->sockfd );
}

void zimr_file_handler( int fd, connection_t* connection ) {
	website_data_t* website_data = connection->website->udata;
	fileread_data_t* fileread_data = &website_data->connections[ connection->sockfd ]->fileread_data;
	int n;
	char buffer[ 2048 ];

	zfd_clr( fd, ZFD_R );
	msg_set_write( connection->website->sockfd, connection->sockfd );

	if ( lseek( fd, 0, SEEK_CUR ) + sizeof( buffer ) > fileread_data->range_end )
		n = sizeof( buffer ) - ( ( lseek( fd, 0, SEEK_CUR ) + sizeof( buffer ) ) - fileread_data->range_end );
	else n = sizeof( buffer );

	if ( ( n = read( fd, buffer, n ) ) <= 0 || msg_write( connection->website->sockfd, connection->sockfd, buffer, n ) == -1 )
		cleanup_connection( connection->website->sockfd, connection->sockfd );
}

void zimr_website_load_module( website_t* website, module_t* module, int argc, char* argv[] ) {
	website_data_t* website_data = website->udata;
	module_website_data_t* module_data = malloc( sizeof( module_website_data_t ) );
	zimr_event( ZIMR_EVENT_WEBSITE_MODULE_INIT, module->name, website->full_url );
	list_append( &website_data->module_data, module_data );
	module_data->module = module;
	module_data->argc = argc;

	int i;
	for ( i = 0; i < module_data->argc; i++ )
		module_data->argv[i] = strdup( argv[i] );

	*(void**) (&modzimr_website_init) = dlsym( module->handle, "modzimr_website_init" );
	*(bool**) (&modzimr_err_occured)  = dlsym( module->handle, "modzimr_err_occured" );

	if ( modzimr_website_init ) {
		module_data->udata = (*modzimr_website_init)( website, module_data->argc, module_data->argv );
		if ( modzimr_err_occured && (*modzimr_err_occured)() ) {
			zimr_event( ZIMR_EVENT_WEBSITE_MODULE_INIT_FAILED, module->name );
		}
	}
	else module_data->udata = NULL;
}

void zimr_website_set_redirect( website_t* website, char* redirect_url ) {
	website_data_t* website_data = (website_data_t*) website->udata;
	if ( website_data->redirect_url ) free( website_data->redirect_url );
	if ( !startswith( redirect_url, "http://" ) && !startswith( redirect_url, "https://" ) ) {
		website_data->redirect_url = malloc( 8 + strlen( redirect_url ) );
		strcpy( website_data->redirect_url, "http://" );
		strcat( website_data->redirect_url, redirect_url );
	} else
		website_data->redirect_url = strdup( redirect_url );
}

void zimr_website_set_proxy( website_t* website, char* ip, int port ) {
	website_data_t* website_data = (website_data_t*) website->udata;
	if ( ip ) strcpy( website_data->proxy.ip, ip );
	if ( port ) website_data->proxy.port = port;
}

void zimr_website_set_connection_handler( website_t* website, void (*connection_handler)( connection_t* connection ) ) {
	website_data_t* website_data = (website_data_t*) website->udata;
	website_data->connection_handler = connection_handler;
}

void zimr_website_set_error_handler( website_t* website, void (*error_handler)( connection_t*, int error_code, char* error_message, size_t len ) ) {
	website_data_t* website_data = (website_data_t*) website->udata;
	website_data->error_handler = error_handler;
}

void zimr_website_unset_connection_handler( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->udata;
	website_data->connection_handler = NULL;
}

void zimr_website_insert_pubdir( website_t* website, char* pubdir, int pos ) {
	website_data_t* website_data = (website_data_t*) website->udata;

	if ( pos < 0 )
		pos = list_size( &website_data->pubdirs ) + pos + 1;

	if ( pos > list_size( &website_data->pubdirs ) )
		pos = list_size( &website_data->pubdirs );
	else if ( pos < 0 )
		pos = 0;

	if ( list_get_at( &website_data->pubdirs, pos ) )
		free( list_extract_at( &website_data->pubdirs, pos ) );

	if ( pos > list_size( &website_data->pubdirs ) )
		pos = list_size( &website_data->pubdirs );

	if ( !pubdir ) return;

	list_insert_at( &website_data->pubdirs, malloc( strlen( pubdir ) + 2 ), pos );
	strcpy( list_get_at( &website_data->pubdirs, pos ), pubdir );
	pubdir = list_get_at( &website_data->pubdirs, pos );

	// we need the trailing forward slash
	if ( pubdir[ strlen( pubdir ) - 1 ] != '/' )
		strcat( pubdir, "/" );

}

/*char* zimr_website_get_pubdir( website_t* website ) {
//	website_data_t* website_data = (website_data_t*) website->udata;
//	return website_data->pubdir;
}*/

void zimr_website_register_page_handler( website_t* website, const char* page_type,
										 void (*handler)( connection_t*, const char*, void* ), void* udata ) {
	website_data_t* website_data = (website_data_t*) website->udata;
	page_handler_t* page_handler = (page_handler_t*) malloc( sizeof( page_handler_t ) );

	strcpy( page_handler->page_type, page_type );
	page_handler->handler = handler;
	page_handler->udata = udata;

	list_append( &website_data->page_handlers, page_handler );
}

void zimr_website_insert_default_page( website_t* website, const char* default_page, int pos ) {
	website_data_t* website_data = (website_data_t*) website->udata;

	if ( pos < 0 )
		pos = list_size( &website_data->default_pages ) + pos + 1;

	if ( pos > list_size( &website_data->default_pages ) )
		pos = list_size( &website_data->default_pages );
	else if ( pos < 0 )
		pos = 0;

	if ( list_get_at( &website_data->default_pages, pos ) )
		free( list_get_at( &website_data->default_pages, pos ) );

	list_insert_at( &website_data->default_pages, strdup( default_page ), pos );
}

void zimr_website_insert_ignored_regex( website_t* website, const char* ignored_regex ) {
	website_data_t* website_data = (website_data_t*) website->udata;
	regex_t* regex = (regex_t*) malloc( sizeof( regex_t ) );
	memset( regex, 0, sizeof( regex_t ) );

	int err_no;
	if ( ( err_no = regcomp( regex, ignored_regex, 0 ) ) !=0 ) /* Compile the regex */
		regfree( regex );
	else
		list_append( &website_data->ignored_regexs, regex );
}

bool zimr_open_request_log() {
	if ( ( reqlogfd = open( ZM_REQ_LOGFILE, O_WRONLY | O_APPEND ) ) == -1 ) {
		if ( errno != ENOENT
		 || ( reqlogfd = creat( ZM_REQ_LOGFILE, S_IRUSR | S_IWUSR ) ) == -1 ) {
			dlog( stderr, "error opening request logfile: %s", strerror( errno ) );
			return false;
		}
	}
	return true;
}

void zimr_log_request( connection_t* connection ) {
	time_t now;
	char now_str[ 80 ], buffer[ 1024 ];

	time( &now );
	strftime( now_str, 80, "%a %b %d %H:%M:%S %Z %Y", localtime( &now ) );

	header_t* header = headers_get_header( &connection->request.headers, "User-Agent" );
	header_t* referer = headers_get_header( &connection->request.headers, "Referer" );

	sprintf( buffer, "%s, %s, %s, %s, %s%s, %d, %s, %s\n",
		now_str,
		inet_ntoa( connection->ip ),
		connection->hostname,
		HTTP_TYPE( connection->request.type ),
		connection->website->full_url,
		connection->request.url,
		connection->response.http_status,
		header ? header->value : "",
		referer ? referer->value : ""
	);

	if ( write( reqlogfd, buffer, strlen( buffer ) ) == -1 ) {
		dlog( stderr, "error writing to request logfile: %s", strerror( errno ) );
	}
}

void zimr_close_request_log() {
	close( reqlogfd );
}
