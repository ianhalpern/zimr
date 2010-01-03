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
static void (*modzimr_init)();
static void (*modzimr_destroy)();
static int  (*modzimr_website_init)( website_t*, int, char** );
/////////////////////////////////////////////////

void command_response_handler( msg_switch_t* msg_switch, msg_packet_resp_t* resp );

const char* zimr_version() {
	return ZIMR_VERSION;
}

const char* zimr_build_date() {
	return BUILD_DATE;
}

bool zimr_init() {
	if ( initialized ) return true;
	// Setup syslog logging - see SETLOGMASK(3)
#if defined(DEBUG)
	setlogmask( LOG_UPTO( LOG_DEBUG ) );
	openlog( DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER );
#else
	setlogmask( LOG_UPTO( LOG_INFO ) );
	openlog( DAEMON_NAME, LOG_CONS, LOG_USER );
#endif

	// call any needed library init functions
	assert( userdir_init( getuid() ) );
	website_init();
	zsocket_init();
	msg_switch_init( INREAD, INWRIT );

	list_init( &loaded_modules );

	if ( !zimr_open_request_log() )
		return false;

	// Register file descriptor type handlers
	//zfd_register_type( INT_CONNECTED, ZFD_R, PFD_TYPE_HDLR zimr_connection_handler );
	zfd_register_type( FILEREAD, ZFD_R, ZFD_TYPE_HDLR zimr_file_handler );

	syslog( LOG_INFO, "initialized." );
	initialized = true;
	return true;
}

void zimr_shutdown() {
	if ( !initialized ) return;

	initialized = false;

	int i;
	for ( i = 0; i < list_size( &websites ); i++ ) {
		zimr_website_destroy( list_get_at( &websites, i ) );
	}

	for ( i = 0; i < list_size( &loaded_modules ); i++ ) {
		zimr_unload_module( list_get_at( &loaded_modules, i ) );
	}

	list_destroy( &websites );
	list_destroy( &loaded_modules );
	syslog( LOG_INFO, "shutdown." );
	zimr_close_request_log();
}

int zimr_cnf_load( char* cnf_path ) {
	zcnf_app_t* cnf = zcnf_app_load( cnf_path );
	if ( !cnf ) return 0;

	zcnf_website_t* website_cnf = cnf->website_node;

	while ( website_cnf ) {
		if ( website_cnf->url ) {

			website_t* website = zimr_website_create( website_cnf->url );

			zimr_website_set_proxy( website, cnf->proxy.ip, cnf->proxy.port );

			if ( website_cnf->pubdir )
				zimr_website_set_pubdir( website, website_cnf->pubdir );

			if ( website_cnf->redirect_url )
				zimr_website_set_redirect( website, website_cnf->redirect_url );

			int i;
			for ( i = 0; i < list_size( &website_cnf->modules ); i++ ) {
				zcnf_module_t* module_cnf = list_get_at( &website_cnf->modules, i );
				module_t* module = zimr_load_module( module_cnf->name );
				if ( !module ) return 0;
				zimr_website_load_module( website, module, module_cnf->argc, module_cnf->argv );
			}

			zimr_website_enable( website );
		}
		website_cnf = website_cnf->next;
	}

	zcnf_app_free( cnf );
	return 1;
}

void zimr_start() {

	int timeout = 0, i = 0;
	website_t* website;
	website_data_t* website_data;

	// TODO: also check to see if there are any enabled websites.
	if ( !list_size( &websites ) )
		syslog( LOG_WARNING, "zimr_start() failed: No websites created" );

	do {
		timeout = 0; // reset timeout value

		// Test and Start website proxies
		for ( i = 0; i < list_size( &websites ); i++ ) {
			website = list_get_at( &websites, i );
			website_data = (website_data_t*) website->udata;

			if ( website->sockfd == -1 && website_data->status == WS_STATUS_ENABLING ) {
				/* connect to the Zimr Daemon Proxy for routing external
				   requests or commands to this process. */
				if ( !zimr_website_enable( website ) ) {
					if ( website_data->conn_tries == 0 )
						syslog( LOG_WARNING, "%s could not connect to proxy...will retry.", website->url );

					if ( ( !ZM_NUM_PROXY_DEATH_RETRIES && website_data->conn_tries != -1 )
					  || ZM_NUM_PROXY_DEATH_RETRIES > ++website_data->conn_tries )
						timeout = ZM_PROXY_DEATH_RETRY_DELAY;

					// giving up ...
					else {
						syslog( LOG_ERR, "\"%s\" is destroying: %s", website->url, pdstrerror( zerrno ) );
						//zimr_website_disable( website );
						zimr_website_destroy( website );
					}
				}
			}

		}

	} while ( list_size( &websites ) && zfd_select( timeout ) );

}

module_t* zimr_load_module( const char* module_name ) {
	if ( strlen( module_name ) >= ZM_MODULE_NAME_MAX_LEN ) {
		fprintf( stderr, "module name excedes max length: %s\n", module_name );
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

		char module_filename[ strlen( module_name ) + 6 ];
		strcpy( module_filename, "mod" );
		strcat( module_filename, module_name );
		strcat( module_filename, ".so" );

		void* handle = dlopen( module_filename, RTLD_NOW | RTLD_GLOBAL );
		if ( handle == NULL ) {
			// report error ...
			fprintf( stderr, "%s\n", dlerror() );
			return NULL;
		}

		module = malloc( sizeof( module_t ) );
		strcpy( module->name, module_name );
		module->handle = handle;

		list_append( &loaded_modules, module );

		*(void**) (&modzimr_init) = dlsym( module->handle, "modzimr_init" );
		if ( modzimr_init ) (*modzimr_init)();
	}

	return module;
}

module_t* (*zimr_get_module)( const char* module_name ) = zimr_load_module;

void zimr_unload_module( module_t* module ) {

	*(void**) (&modzimr_destroy) = dlsym( module->handle, "modzimr_destroy" );
	if ( modzimr_destroy ) (*modzimr_destroy)();

	//TODO: locate module in loaded_modules and remove
	dlclose( module->handle );
	free( module );
}

website_t* zimr_website_create( char* url ) {
	website_t* website;
	website_data_t* website_data;

	if ( !( website = website_add( -1, url ) ) )
		return NULL;

	website_data = (website_data_t*) malloc( sizeof( website_data_t ) );
	website->udata = (void*) website_data;

	website_data->msg_switch = NULL;
	website_data->pubdir = strdup( "./" );
	website_data->status = WS_STATUS_DISABLED;
	website_data->connection_handler = NULL;
	website_data->conn_tries = 0; //ZM_NUM_PROXY_DEATH_RETRIES - 1;
	website_data->redirect_url = NULL;
	strcpy( website_data->proxy.ip, ZM_PROXY_DEFAULT_ADDR );
	website_data->proxy.port = ZM_PROXY_DEFAULT_PORT;
	memset( website_data->connections, 0, sizeof( website_data->connections ) );
	list_init( &website_data->default_pages );
	list_init( &website_data->ignored_files );
	list_init( &website_data->page_handlers );

	list_attributes_comparator( &website_data->ignored_files, (element_comparator) strcmp );

	zimr_website_insert_default_page( website, "default.html", 0 );
	zimr_website_insert_ignored_file( website, "zimr.cnf" );
	zimr_website_insert_ignored_file( website, "zimr.log" );

	return website;
}

void zimr_website_destroy( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->udata;

	zimr_website_disable( website );

	if ( website_data->pubdir )
		free( website_data->pubdir );

	// TODO: free memory before destroying
	list_destroy( &website_data->default_pages );
	list_destroy( &website_data->ignored_files );
	list_destroy( &website_data->page_handlers );

	free( website_data );
	website_remove( website );
}

void command_response_handler( msg_switch_t* msg_switch, msg_packet_resp_t* resp ) {
	website_t* website = website_get_by_sockfd( msg_switch->sockfd );
	website_data_t* website_data = website->udata;
	switch ( resp->msgid ) {
		case ZM_CMD_WS_START:
			switch ( resp->status ) {
				case MSG_PACK_RESP_OK:
					// Everything is good, start listening for requests.
					website_data->status = WS_STATUS_ENABLED;

					syslog( LOG_INFO, "%s is enabled!", website->url );
					website_data->conn_tries = 0;

					break;
				case MSG_PACK_RESP_FAIL:
					// WS_START_CMD failed...close socket
					zsocket_close( website->sockfd );
					website->sockfd = -1;
					website_data->conn_tries = ZM_NUM_PROXY_DEATH_RETRIES - 1; // we do not want to retry
					break;
			}
			break;
	}
}

void cleanup_fileread( website_data_t* website_data, int sockfd ) {
	if ( website_data->connections[ sockfd ]->filereadfd != -1 ) {
		zfd_clr( website_data->connections[ sockfd ]->filereadfd, FILEREAD );
		close( website_data->connections[ sockfd ]->filereadfd );
		website_data->connections[ sockfd ]->filereadfd = -1;
		//printf( "[%d] closing filereadfd %d\n", sockfd, website_data->connections[ sockfd ]->filereadfd );
	}
}

void cleanup_connection( website_data_t* website_data, int sockfd ) {
	cleanup_fileread( website_data, sockfd );
	if ( website_data->connections[ sockfd ]->connection )
		connection_free( website_data->connections[ sockfd ]->connection );
	free( website_data->connections[ sockfd ]->data );
	free( website_data->connections[ sockfd ] );
	website_data->connections[ sockfd ] = NULL;
}

void msg_event_handler( msg_switch_t* msg_switch, msg_event_t event ) {
	website_t* website = website_get_by_sockfd( msg_switch->sockfd );
	website_data_t* website_data = website->udata;

	switch ( event.type ) {
		case MSG_EVT_SENT:
		case MSG_RECV_EVT_KILL:
			if ( event.data.msgid >= 0 ) {
				cleanup_connection( website_data, event.data.msgid );
			}
			break;
		case MSG_RECV_EVT_PACK:
			if ( zimr_connection_handler( website, event.data.packet ) )
				msg_switch_send_pack_resp( msg_switch, event.data.packet, MSG_PACK_RESP_OK );
			else
				msg_switch_send_pack_resp( msg_switch, event.data.packet, MSG_PACK_RESP_FAIL );
			break;
		case MSG_RECV_EVT_RESP:
			if ( event.data.resp->msgid < 0 ) {
				command_response_handler( msg_switch, event.data.resp );
			}

			else if ( event.data.resp->status == MSG_PACK_RESP_FAIL ) {
				cleanup_connection( website_data, event.data.resp->msgid );
			}
			break;
		case MSG_EVT_BUF_FULL:
			if ( event.data.msgid >= 0 && website_data->connections[ event.data.msgid ]->filereadfd != -1 ) {
				zfd_clr( website_data->connections[ event.data.msgid ]->filereadfd, FILEREAD );
			}
			break;
		case MSG_EVT_BUF_EMPTY:
			if ( website_data->connections[ event.data.msgid ]->filereadfd != -1 ) {
				zfd_reset( website_data->connections[ event.data.msgid ]->filereadfd, FILEREAD );
			}
			break;
		case MSG_SWITCH_EVT_IO_FAILED:
			zimr_website_disable( website );
			zimr_website_enable( website );
			break;
		case MSG_EVT_NEW:
		case MSG_EVT_DESTROY:
		case MSG_EVT_COMPLETE:
		case MSG_RECV_EVT_LAST:
		case MSG_RECV_EVT_FIRST:
		case MSG_SWITCH_EVT_NEW:
		case MSG_SWITCH_EVT_DESTROY:
			break;
	}
}

bool zimr_website_enable( website_t* website ) {
	website_data_t* website_data = website->udata;

	if ( website_data->status == WS_STATUS_ENABLED ) {
		zerr( ZMERR_EXISTS );
		return false;
	}

	website_data->status = WS_STATUS_ENABLING;
	website->sockfd = zsocket( inet_addr( website_data->proxy.ip ), website_data->proxy.port, ZSOCK_CONNECT );

	if ( website->sockfd == -1 ) {
		zerr( ZMERR_ZSOCK_CONN );
		return false;
	}

	website_data->msg_switch = msg_switch_new( website->sockfd, msg_event_handler );

	// send website enable command
	msg_new( website_data->msg_switch, ZM_CMD_WS_START );
	msg_push_data( website_data->msg_switch, ZM_CMD_WS_START, website->url, strlen( website->url ) );
	msg_end( website_data->msg_switch, ZM_CMD_WS_START );

	return true;
}

void zimr_website_disable( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->udata;

	/* we only need to send command if the socket is connected. */
	if ( website->sockfd != -1 ) {

		int i;
		for ( i = 0; i < FD_SETSIZE; i++ ) {
			if ( website_data->connections[ i ] )
				cleanup_connection( website_data, i );
		}

		if ( website_data->status == WS_STATUS_ENABLED ) {
			msg_new( website_data->msg_switch, ZM_CMD_WS_STOP );
			msg_push_data( website_data->msg_switch, ZM_CMD_WS_STOP, NULL, 0 );
			msg_end( website_data->msg_switch, ZM_CMD_WS_STOP );
		}

		msg_switch_destroy( website_data->msg_switch );
		zsocket_close( website->sockfd );
		website->sockfd = -1;
	}

	website_data->status = WS_STATUS_DISABLED;
}

static void zimr_website_default_connection_handler( connection_t* connection ) {
	zimr_connection_send_file( connection, connection->request.url, true );
}

bool zimr_connection_handler( website_t* website, msg_packet_t* packet ) {
	website_data_t* website_data = website->udata;
	conn_data_t* conn_data;

	int msgid = packet->msgid;
	if ( PACK_IS_FIRST( packet ) ) {
		website_data->connections[ msgid ] = (conn_data_t*) malloc( sizeof( conn_data_t ) );
		website_data->connections[ msgid ]->data = strdup( "" );
		website_data->connections[ msgid ]->size = 0;
		website_data->connections[ msgid ]->filereadfd = -1;
	}

	conn_data = website_data->connections[ msgid ];

	char* tmp;
	tmp = conn_data->data;
	conn_data->data = (char*) malloc( packet->size + conn_data->size );
	memset( conn_data->data, 0, packet->size + conn_data->size );
	memcpy( conn_data->data, tmp, conn_data->size );
	memcpy( conn_data->data + conn_data->size, packet->data, packet->size );
	conn_data->size = packet->size + conn_data->size;
	free( tmp );

	if ( PACK_IS_LAST( packet ) ) {
		conn_data->connection = connection_create( website, msgid, conn_data->data, conn_data->size );

		if ( !conn_data->connection ) {
			cleanup_connection( website_data, packet->msgid );
			return false;
		}

		// start optimistically
		response_set_status( &conn_data->connection->response, 200 );
		// To ensure thier position at the top of the headers
		headers_set_header( &conn_data->connection->response.headers, "Date", "" );
		headers_set_header( &conn_data->connection->response.headers, "Server", "" );

		if ( website_data->redirect_url ) {
			char buf[ strlen( website_data->redirect_url ) + strlen( conn_data->connection->request.url ) + 8 ];
			sprintf( buf, "http://%s%s", website_data->redirect_url, conn_data->connection->request.url );
			zimr_connection_send_redirect( conn_data->connection, buf );
		}

		else if ( website_data->connection_handler )
			website_data->connection_handler( conn_data->connection );

		else
			zimr_website_default_connection_handler( conn_data->connection );
	}

	return true;
}

void zimr_connection_send_status( connection_t* connection ) {
	website_data_t* website_data = connection->website->udata;
	char status_line[ 100 ];
	sprintf(
		status_line,
		"HTTP/%s %s" HTTP_HDR_ENDL,
		connection->http_version,
		response_status( connection->response.http_status )
	);

	msg_push_data( website_data->msg_switch, connection->sockfd, status_line, strlen( status_line ) );
}

void zimr_connection_send_headers( connection_t* connection ) {
	website_data_t* website_data = connection->website->udata;
	time_t now;
	char now_str[ 80 ];

	time( &now );
	strftime( now_str, 80, "%a %b %d %I:%M:%S %Z %Y", localtime( &now ) );

	headers_set_header( &connection->response.headers, "Date", now_str );
	headers_set_header( &connection->response.headers, "Server", "Zimr/" ZIMR_VERSION );

	char* cookies = cookies_to_string( &connection->cookies, (char*) malloc( 1024 ), 1024 );

	if ( strlen( cookies ) )
		headers_set_header( &connection->response.headers, "Set-Cookie", cookies );

	char* headers = headers_to_string( &connection->response.headers, (char*) malloc( 1024 ) );

	msg_push_data( website_data->msg_switch, connection->sockfd, (void*) headers, strlen( headers ) );
	free( cookies );
	free( headers );

	zimr_log_request( connection );
}

void zimr_connection_send( connection_t* connection, void* message, int size ) {
	website_data_t* website_data = connection->website->udata;
	char sizebuf[ 32 ];

	msg_new( website_data->msg_switch, connection->sockfd );

	if ( !headers_get_header( &connection->response.headers, "Content-Type" ) ) {
		headers_set_header( &connection->response.headers, "Content-Type", mime_get_type( ".html" ) );
	}

	sprintf( sizebuf, "%d", size );
	headers_set_header( &connection->response.headers, "Content-Length", sizebuf );

	zimr_connection_send_status( connection );
	zimr_connection_send_headers( connection );

	msg_push_data( website_data->msg_switch, connection->sockfd, (void*) message, size );
	msg_end( website_data->msg_switch, connection->sockfd );
}

void zimr_connection_send_file( connection_t* connection, char* filepath, bool use_pubdir ) {
	website_data_t* website_data = connection->website->udata;
	struct stat file_stat;
	char full_filepath[ 256 ] = "";
	char* ptr;
	int i;

	// if the filepath is relative to the specified public directory, initialize
	// the filepath with the public directory.
	if ( use_pubdir && filepath[ 0 ] != '/' )
		strcpy( full_filepath, website_data->pubdir );

	strcat( full_filepath, filepath );

	if ( stat( full_filepath, &file_stat ) ) {
		// Does not exist.
		zimr_connection_send_error( connection, 404 );
		return;
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
	ptr = strrchr( full_filepath, '/' );
	if ( ptr ) {
		ptr++; // skip over '/'
		if ( list_contains( &website_data->ignored_files, ptr ) ) {
			// Does not exist.
			zimr_connection_send_error( connection, 404 );
			return;
		}
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

void zimr_connection_send_error( connection_t* connection, short code ) {
	website_data_t* website_data = connection->website->udata;
	char sizebuf[ 10 ];
	msg_new( website_data->msg_switch, connection->sockfd );

	response_set_status( &connection->response, code );
	headers_set_header( &connection->response.headers, "Content-Type", mime_get_type( ".html" ) );

	char error_msg_buf[ 128 ];
	strcpy( error_msg_buf, "<html><body><h1>" );
	strcat( error_msg_buf, response_status( code ) );
	strcat( error_msg_buf, "</h1></body></html>" );

	sprintf( sizebuf, "%zu", strlen( error_msg_buf ) );
	headers_set_header( &connection->response.headers, "Content-Length", sizebuf );

	zimr_connection_send_status( connection );
	zimr_connection_send_headers( connection );

	msg_push_data( website_data->msg_switch, connection->sockfd,
	  (void*) error_msg_buf, strlen( error_msg_buf ) );
	msg_end( website_data->msg_switch, connection->sockfd );
}

void zimr_connection_send_redirect( connection_t* connection, char* url ) {
	website_data_t* website_data = connection->website->udata;
	msg_new( website_data->msg_switch, connection->sockfd );

	headers_set_header( &connection->response.headers, "Location", url );

	response_set_status( &connection->response, 302 );
	zimr_connection_send_status( connection );
	zimr_connection_send_headers( connection );

	msg_end( website_data->msg_switch, connection->sockfd );
}

void zimr_connection_send_dir( connection_t* connection, char* filepath ) {
	website_data_t* website_data = connection->website->udata;
	int i;
	DIR* dir;
	struct dirent* dp;
	if ( ( dir = opendir( filepath ) ) == NULL) {
		zimr_connection_send_error( connection, 404 );
		return;
	}

	int num = 0;
	char* files[ 512 ];

	char html_header_fmt[] = "<html><style type=\"text/css\">html, body { font-family: sans-serif; }</style><body>\n<h1>%s</h1>\n";
	char html_file_fmt[]   = "<a href=\"%s\">%s</a><br/>\n";
	char html_dir_fmt[]    = "<a href=\"%s/\">%s/</a><br/>\n";
	char html_header[ strlen( html_header_fmt ) + strlen( filepath ) - strlen( zimr_website_get_pubdir( connection->website ) ) ];
	sprintf( html_header, html_header_fmt, filepath + strlen( zimr_website_get_pubdir( connection->website ) ) );
	char html_footer[] = "<br/>"  "Zimr/" ZIMR_VERSION "\n</body></html>";

	int size = strlen( html_header ) + strlen( html_footer );
	while ( ( dp = readdir( dir ) ) != NULL && num < sizeof( files ) / sizeof( char* ) ) {
		char fullpath[ strlen( filepath ) + strlen( dp->d_name ) + 1 ];
		sprintf( fullpath, "%s%s", filepath, dp->d_name );

		struct stat file_stat;
		if ( stat( fullpath, &file_stat ) ) {
			continue;
		}

		if ( S_ISDIR( file_stat.st_mode ) ) {
			files[ num ] = (char*) malloc( strlen( dp->d_name ) * 2 + sizeof( html_dir_fmt ) + 1 );
			sprintf( files[ num ], html_dir_fmt, dp->d_name, dp->d_name );
		} else {
			if ( list_contains( &website_data->ignored_files, dp->d_name ) )
				continue;

			files[ num ] = (char*) malloc( strlen( dp->d_name ) * 2 + sizeof( html_file_fmt ) + 1 );
			sprintf( files[ num ], html_file_fmt, dp->d_name, dp->d_name );
		}
		size += strlen( files[ num ] );
		num++;

		for ( i = num - 1; i > 0; i-- ) {
			if ( files[ i ][ 10 ] < files[ i - 1 ][ 10 ] ) {
				char* tmp = files[ i ];
				files[ i ] = files[ i - 1 ];
				files[ i - 1 ] = tmp;
			} else break;
		}
	}
	closedir( dir );

	char sizebuf[ 10 ];
	msg_new( website_data->msg_switch, connection->sockfd );
	response_set_status( &connection->response, 200 );
	headers_set_header( &connection->response.headers, "Content-Type", mime_get_type( ".html" ) );
	sprintf( sizebuf, "%d", size );
	headers_set_header( &connection->response.headers, "Content-Length", sizebuf );

	zimr_connection_send_status( connection );
	zimr_connection_send_headers( connection );

	msg_push_data( website_data->msg_switch, connection->sockfd, html_header, strlen( html_header ) );

	for ( i = 0; i < num; i++ ) {
		msg_push_data( website_data->msg_switch, connection->sockfd, files[ i ], strlen( files[ i ] ) );
		free( files[ i ] );
	}

	msg_push_data( website_data->msg_switch, connection->sockfd, html_footer, strlen( html_footer ) );

	msg_end( website_data->msg_switch, connection->sockfd );
}

void zimr_connection_default_page_handler( connection_t* connection, char* filepath ) {
	int fd = -1;
	struct stat file_stat;
	char sizebuf[ 32 ];

	if ( stat( filepath, &file_stat ) ) {
		zimr_connection_send_error( connection, 404 );
		return;
	}

	if ( S_ISDIR( file_stat.st_mode ) ) {
		zimr_connection_send_dir( connection, filepath );
		return;
	}

	if ( ( fd = open( filepath, O_RDONLY ) ) < 0 ) {
		zimr_connection_send_error( connection, 404 );
		return;
	}

	website_data_t* website_data = connection->website->udata;
	msg_new( website_data->msg_switch, connection->sockfd );

	if ( !headers_get_header( &connection->response.headers, "Content-Type" ) )
		headers_set_header( &connection->response.headers, "Content-Type", mime_get_type( filepath ) );

	sprintf( sizebuf, "%d", (int) file_stat.st_size );
	headers_set_header( &connection->response.headers, "Content-Length", sizebuf );

	zimr_connection_send_status( connection );
	zimr_connection_send_headers( connection );

	//printf( "[%d] opening filereadfd %d\n", connection->sockfd, fd );
	website_data->connections[ connection->sockfd ]->filereadfd = fd;
	zfd_set( fd, FILEREAD, connection );
}

void zimr_file_handler( int fd, connection_t* connection ) {
	website_data_t* website_data = connection->website->udata;
	int n;
	char buffer[ 2048 ];

	if ( ( n = read( fd, buffer, sizeof( buffer ) ) ) > 0 ) {
		msg_push_data( website_data->msg_switch, connection->sockfd, buffer, n );
	}

	else {
		if ( n == -1 )
			msg_kill( website_data->msg_switch, connection->sockfd );
		else
			msg_end( website_data->msg_switch, connection->sockfd );

		cleanup_fileread( website_data, connection->sockfd );
	}
}

void zimr_website_load_module( website_t* website, module_t* module, int argc, char* argv[] ) {
	*(void **)(&modzimr_website_init) = dlsym( module->handle, "modzimr_website_init" );
	if ( modzimr_website_init ) (*modzimr_website_init)( website, argc, argv );
}

void zimr_website_set_redirect( website_t* website, char* redirect_url ) {
	website_data_t* website_data = (website_data_t*) website->udata;
	if ( website_data->redirect_url ) free( website_data->redirect_url );
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

void zimr_website_unset_connection_handler( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->udata;
	website_data->connection_handler = NULL;
}

void zimr_website_set_pubdir( website_t* website, const char* pubdir ) {
	website_data_t* website_data = (website_data_t*) website->udata;

	if ( website_data->pubdir ) free( website_data->pubdir );
	website_data->pubdir = (char*) malloc( strlen( pubdir + 2 ) );
	strcpy( website_data->pubdir, pubdir );

	// we need the trailing forward slash
	if ( website_data->pubdir[ strlen( website_data->pubdir ) ] != '/' )
		strcat( website_data->pubdir, "/" );
}

char* zimr_website_get_pubdir( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->udata;
	return website_data->pubdir;
}

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

	list_insert_at( &website_data->default_pages, strdup( default_page ), pos );
}

void zimr_website_insert_ignored_file( website_t* website, const char* ignored_file ) {
	website_data_t* website_data = (website_data_t*) website->udata;
	list_append( &website_data->ignored_files, ignored_file );
}

bool zimr_open_request_log() {
	if ( ( reqlogfd = open( ZM_REQ_LOGFILE, O_WRONLY | O_APPEND ) ) == -1 ) {
		if ( errno != ENOENT
		 || ( reqlogfd = creat( ZM_REQ_LOGFILE, S_IRUSR | S_IWUSR ) ) == -1 ) {
			syslog( LOG_ERR, "error opening request logfile: %s", strerror( errno ) );
			return false;
		}
	}
	return true;
}

void zimr_log_request( connection_t* connection ) {
	time_t now;
	char now_str[ 80 ], buffer[ 1024 ];

	time( &now );
	strftime( now_str, 80, "%a %b %d %I:%M:%S %Z %Y", localtime( &now ) );

	header_t* header = headers_get_header( &connection->request.headers, "User-Agent" );
	header_t* referer = headers_get_header( &connection->request.headers, "Referer" );

	sprintf( buffer, "%s, %s, %s, %s, http://%s/%s, %d, %s, %s\n",
		now_str,
		inet_ntoa( connection->ip ),
		connection->hostname,
		HTTP_TYPE( connection->request.type ),
		connection->website->url,
		connection->request.url,
		connection->response.http_status,
		header ? header->value : "",
		referer ? referer->value : ""
	);

	if ( write( reqlogfd, buffer, strlen( buffer ) ) == -1 ) {
		perror( "error" );
	}
}

void zimr_close_request_log() {
	close( reqlogfd );
}
