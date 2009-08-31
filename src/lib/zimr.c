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

static int reqlogfd = -1;

typedef struct {
	int size;
	char* data;
} request_data_t;

static request_data_t requests[ 100 ];

typedef struct {
	char page_type[ 10 ];
	void (*page_handler)( connection_t*, const char*, void* );
	void* udata;
} page_handler_t;

static int page_handler_count = 0;
static page_handler_t page_handlers[ 100 ];

const char* zimr_version( ) {
	return ZIMR_VERSION;
}

const char* zimr_build_date( ) {
	return BUILD_DATE;
}

bool zimr_init( ) {

	// Setup syslog logging - see SETLOGMASK(3)
#if defined(DEBUG)
	setlogmask( LOG_UPTO( LOG_DEBUG ) );
	openlog( DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER );
#else
	setlogmask( LOG_UPTO( LOG_INFO ) );
	openlog( DAEMON_NAME, LOG_CONS, LOG_USER );
#endif

	if ( !zimr_open_request_log( ) )
		return false;

	// Register file descriptor type handlers
	zfd_register_type( PFD_TYPE_INT_CONNECTED, PFD_TYPE_HDLR zimr_connection_handler );
	zfd_register_type( PFD_TYPE_FILE, PFD_TYPE_HDLR zimr_file_handler );

	website_init( );

	syslog( LOG_INFO, "initialized." );
	return true;
}

void zimr_shutdown( ) {
	int i;
	for ( i = 0; i < list_size( &websites ); i++ ) {
		zimr_website_destroy( list_get_at( &websites, i ) );
	}

	list_clear( &websites );
	syslog( LOG_INFO, "shutdown." );
	zimr_close_request_log( );
}

void zimr_start( ) {
	int timeout = 0, i = 0;
	website_t* website;
	website_data_t* website_data;

	// TODO: also check to see if there are any enabled websites.
	if ( !list_size( &websites ) )
		syslog( LOG_WARNING, "zimr_start() failed: No websites created" );

	do {
		timeout = 0; // reset timeout value

		// Test and Start website proxies
		while ( i < list_size( &websites ) ) {
			website = list_get_at( &websites, i );
			website_data = (website_data_t*) website->udata;

			if ( !website_data->socket && website_data->status == WS_STATUS_ENABLING ) {

				/* connect to the Zimr Daemon Proxy for routing external
				   requests or commands to this process. */
				if ( !zimr_website_enable( website ) ) {
					if ( zerrno == ZMERR_FAILED )
						website_data->conn_tries = ZM_NUM_PROXY_DEATH_RETRIES - 1; // we do not want to retry

					if ( website_data->conn_tries == 0 )
						syslog( LOG_WARNING, "%s could not connect to proxy...will retry.", website->url );

					if ( ( !ZM_NUM_PROXY_DEATH_RETRIES && website_data->conn_tries != -1 )
					  || ZM_NUM_PROXY_DEATH_RETRIES > ++website_data->conn_tries )
						timeout = ZM_PROXY_DEATH_RETRY_DELAY;

					/* giving up ... */
					else {
						syslog( LOG_ERR, "\"%s\" is destroying: %s", website->url, pdstrerror( zerrno ) );
						//zimr_website_disable( website );
						zimr_website_destroy( website );
					}

				}

				/* successfully connected to Zimr Daemon Proxy, reset number of retries */
				/*else {
					syslog( LOG_INFO, "%s is enabled!", website->url );
					website_data->conn_tries = 0;
				}*/
			}

			i++;
		}

	} while ( list_size( &websites ) && zfd_select( timeout ) );

}

bool zimr_send_cmd( zsocket_t* socket, int cmd, void* message, int size ) {

	int ret = true;
	ztransport_t* transport;

	transport = ztransport_open( socket->sockfd, ZT_WO, cmd );

	if ( ztransport_write( transport, message, size ) <= 0 ) {
		zerr( ZMERR_SOCK_CLOSED );
		ret = false;
		goto quit;
	}

	ztransport_close( transport );

	transport = ztransport_open( socket->sockfd, ZT_RO, 0 );

	if ( ztransport_read( transport ) <= 0 ) {
		zerr( ZMERR_SOCK_CLOSED );
		ret = false;
		goto quit;
	}

	if ( strcmp( "OK", transport->message ) != 0 ) {
		zerr( ZMERR_FAILED );
		ret = false;
	}

quit:
	ztransport_close( transport );
	return ret;
}

int zimr_cnf_load( char* cnf_path ) {
	zcnf_app_t* cnf = zcnf_app_load( cnf_path );
	if ( !cnf ) return 0;

	zcnf_website_t* website_cnf = cnf->website_node;

	while ( website_cnf ) {
		if ( website_cnf->url ) {
			website_t* website = zimr_website_create( website_cnf->url );

			if ( website_cnf->pubdir )
				zimr_website_set_pubdir( website, website_cnf->pubdir );

			zimr_website_enable( website );
		}
		website_cnf = website_cnf->next;
	}

	zcnf_app_free( cnf );
	return 1;
}

website_t* zimr_website_create( char* url ) {
	website_t* website;
	website_data_t* website_data;

	if ( !( website = website_add( -1, url ) ) )
		return NULL;

	website_data = (website_data_t*) malloc( sizeof( website_data_t ) );
	website->udata = (void*) website_data;

	website_data->socket = NULL;
	website_data->pubdir = strdup( "./" );
	website_data->status = WS_STATUS_DISABLED;
	website_data->connection_handler = NULL;
	website_data->conn_tries = 0; //ZM_NUM_PROXY_DEATH_RETRIES - 1;
	website_data->default_pages_count = 0;
	zimr_website_insert_default_page( website, "default.html", 0 );
	return website;
}

void zimr_website_destroy( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->udata;

	zimr_website_disable( website );

	if ( website_data->pubdir )
		free( website_data->pubdir );

	free( website_data );
	website_remove( website );
}

bool zimr_website_enable( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->udata;

	if ( website_data->status == WS_STATUS_ENABLED ) {
		zerr( ZMERR_EXISTS );
		return false;
	}

	website_data->status = WS_STATUS_ENABLING;
	website_data->socket = zsocket_connect( inet_addr( ZM_PROXY_ADDR ), ZM_PROXY_PORT );

	if ( !website_data->socket ) {
		zerr( ZMERR_ZSOCK_CONN );
		return false;
	}

	// send website enable command
	if ( !zimr_send_cmd( website_data->socket, ZM_CMD_WS_START, website->url, strlen( website->url ) ) ) {
		// WS_START_CMD failed...close socket
		zsocket_close( website_data->socket );
		website_data->socket = NULL;
		return false;
	}

	// Everything is good, start listening for requests.
	/* set website sockfd */
	website->sockfd = website_data->socket->sockfd;
	website_data->status = WS_STATUS_ENABLED;
	zfd_set( website_data->socket->sockfd, PFD_TYPE_INT_CONNECTED, website );

	syslog( LOG_INFO, "%s is enabled!", website->url );
	website_data->conn_tries = 0;
	return true;
}

void zimr_website_disable( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->udata;

	/* we only need to send command if the socket is connected. */
	if ( website_data->socket ) {

		if ( website_data->status == WS_STATUS_ENABLED )
			zimr_send_cmd( website_data->socket, ZM_CMD_WS_STOP, NULL, 0 );

		zfd_clr( website_data->socket->sockfd );
		zsocket_close( website_data->socket );
		website_data->socket = NULL;

	}

	website_data->status = WS_STATUS_DISABLED;
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

void zimr_website_insert_default_page( website_t* website, const char* default_page, int pos ) {
	website_data_t* website_data = (website_data_t*) website->udata;

	if ( pos < 0 )
		pos = website_data->default_pages_count + pos + 1;

	if ( pos > website_data->default_pages_count )
		pos = website_data->default_pages_count;
	else if ( pos < 0 )
		pos = 0;

	int i;
	for ( i = website_data->default_pages_count; i > pos; i-- ) {
		strcpy( website_data->default_pages[ i ], website_data->default_pages[ i - 1 ] );
	}

	strcpy( website_data->default_pages[ pos ], default_page );
	website_data->default_pages_count++;
}

static void zimr_website_default_connection_handler( connection_t* connection ) {
	zimr_connection_send_file( connection, connection->request.url, true );
	connection_free( connection );
}

void zimr_connection_handler( int sockfd, website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->udata;

	ztransport_t* transport = ztransport_open( sockfd, ZT_RO, 0 );

	if ( !ztransport_read( transport ) ) {
		// bad socket.
		zerr( ZMERR_SOCK_CLOSED );
		website_data->status = WS_STATUS_ENABLING;
		zfd_clr( website_data->socket->sockfd );
		zsocket_close( website_data->socket );
		website_data->socket = NULL;
		zfd_unblock( );
		return;
	}

	int msgid = transport->header->msgid;
	if ( ZT_MSG_IS_FIRST( transport ) ) {
		requests[ msgid ].data = strdup( "" );
		requests[ msgid ].size = 0;
	}

	char* tmp;
	tmp = requests[ msgid ].data;
	requests[ msgid ].data = (char*) malloc( transport->header->size + requests[ msgid ].size );
	memset( requests[ msgid ].data, 0, transport->header->size + requests[ msgid ].size );
	memcpy( requests[ msgid ].data, tmp, requests[ msgid ].size );
	memcpy( requests[ msgid ].data + requests[ msgid ].size, transport->message, transport->header->size );
	requests[ msgid ].size =  transport->header->size + requests[ msgid ].size;
	free( tmp );

	if ( ZT_MSG_IS_LAST( transport ) ) {
		connection_t* connection;
		connection = connection_create( website, msgid, requests[ msgid ].data, requests[ msgid ].size );

		// start optimistically
		response_set_status( &connection->response, 200 );
		// To ensure thier position at the top of the headers
		headers_set_header( &connection->response.headers, "Date", "" );
		headers_set_header( &connection->response.headers, "Server", "" );

		if ( website_data->connection_handler )
			website_data->connection_handler( connection );
		else
			zimr_website_default_connection_handler( connection );
	}

	ztransport_close( transport );
}

void zimr_file_handler( int fd, ztransport_t* transport ) {
	int n;
	char buffer[ 2048 ];

	if ( ( n = read( fd, buffer, sizeof( buffer ) ) ) > 0 ) {
		if ( !ztransport_write( transport, buffer, n ) ) {
			ztransport_close( transport );
			zfd_clr( fd );
			close( fd );

			syslog( LOG_ERR, "file_handler: pcom_write() failed" );
		}
	} else {
		ztransport_close( transport );
		zfd_clr( fd );
		close( fd );
	}
}

void zimr_connection_send_status( ztransport_t* transport, connection_t* connection ) {
	char status_line[ 100 ];
	sprintf( status_line, "HTTP/%s %s" HTTP_HDR_ENDL, connection->http_version, response_status( connection->response.http_status ) );

	ztransport_write( transport, status_line, strlen( status_line ) );
}

void zimr_connection_send_headers( ztransport_t* transport, connection_t* connection ) {
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

	ztransport_write( transport, (void*) headers, strlen( headers ) );
	free( cookies );
	free( headers );

	zimr_log_request( connection );
}

void zimr_connection_send( connection_t* connection, void* message, int size ) {
	char sizebuf[ 32 ];
	ztransport_t* transport = ztransport_open( connection->website->sockfd, ZT_WO, connection->sockfd );

	if ( !headers_get_header( &connection->response.headers, "Content-Type" ) ) {
		headers_set_header( &connection->response.headers, "Content-Type", mime_get_type( ".html" ) );
	}

	sprintf( sizebuf, "%d", size );
	headers_set_header( &connection->response.headers, "Content-Length", sizebuf );

	zimr_connection_send_status( transport, connection );
	zimr_connection_send_headers( transport, connection );

	ztransport_write( transport, (void*) message, size );
	ztransport_close( transport );
}

void zimr_connection_send_file( connection_t* connection, char* filepath, bool use_pubdir ) {
	struct stat file_stat;
	char full_filepath[ 256 ] = "";
	char* ptr;
	int i;
	website_data_t* website_data = connection->website->udata;

	// if the filepath is relative to the specified public directory, initialize
	// the filepath with the public directory.
	if ( use_pubdir )
		strcpy( full_filepath, website_data->pubdir );

	strcat( full_filepath, filepath );

	if ( stat( full_filepath, &file_stat ) ) {
		// Does not exist.
		zimr_connection_send_error( connection, 404 );
		return;
	}

	if ( S_ISDIR( file_stat.st_mode ) ) {

		if ( full_filepath[ strlen( full_filepath ) - 1 ] != '/' ) {
			char buf[ strlen( connection->website->url ) + strlen( full_filepath ) + 8 ];
			sprintf( buf, "http://%s%s/", connection->website->url, full_filepath + 1 );
			zimr_connection_send_redirect( connection, buf );
			return;
		}

		// search for the directory default page
		ptr = full_filepath + strlen( full_filepath );
		for ( i = 0; i < website_data->default_pages_count; i++ ) {
			strcpy( ptr, website_data->default_pages[ i ] );
			if ( !stat( full_filepath, &file_stat ) )
				break; // default file exists...break loop
		}

		if ( i == website_data->default_pages_count ) {
			*ptr = 0;
		}

	}

	// search for a page handler by file extension
	ptr = strrchr( full_filepath, '.' );

	i = page_handler_count;
	if ( ptr != NULL && ptr[ 1 ] != '\0' ) {
		ptr++; // skip over "."

		for ( i = 0; i < page_handler_count; i++ ) {
			if ( strcmp( page_handlers[ i ].page_type, ptr ) == 0 ) {
				page_handlers[ i ].page_handler( connection, full_filepath, page_handlers[ i ].udata );
				break;
			}
		}

	}

	// check if a page handler was found, if not use the default.
	if ( i == page_handler_count )
		zimr_connection_default_page_handler( connection, full_filepath );

}

void zimr_connection_send_error( connection_t* connection, short code ) {
	char sizebuf[ 10 ];
	ztransport_t* transport = ztransport_open( connection->website->sockfd, ZT_WO, connection->sockfd );

	response_set_status( &connection->response, code );
	headers_set_header( &connection->response.headers, "Content-Type", mime_get_type( ".html" ) );
	sprintf( sizebuf, "%d", 48 );
	headers_set_header( &connection->response.headers, "Content-Length", sizebuf );

	zimr_connection_send_status( transport, connection );
	zimr_connection_send_headers( transport, connection );

	ztransport_write( transport, (void*) "<html><body><h1>404 Not Found</h1></body></html>", 48 );
	ztransport_close( transport );
}

void zimr_connection_send_redirect( connection_t* connection, char* url ) {
	ztransport_t* transport = ztransport_open( connection->website->sockfd, ZT_WO, connection->sockfd );

	headers_set_header( &connection->response.headers, "Location", url );

	response_set_status( &connection->response, 302 );
	zimr_connection_send_status( transport, connection );
	zimr_connection_send_headers( transport, connection );

	ztransport_close( transport );
}

void zimr_connection_send_dir( connection_t* connection, char* filepath ) {
	int i;
	DIR* dir;
	struct dirent* dp;
	if ( ( dir = opendir( filepath ) ) == NULL) {
		zimr_connection_send_error( connection, 404 );
		return;
	}

	int num = 0;
	char* files[ 512 ];

	char html_header_fmt[ ] = "<html><style type=\"text/css\">html, body { font-family: sans-serif; }</style><body>\n<h1>/%s</h1>\n";
	char html_file_fmt[ ]   = "<a href=\"%s\">%s</a><br/>\n";
	char html_dir_fmt[ ]    = "<a href=\"%s/\">%s/</a><br/>\n";
	char html_header[ strlen( html_header_fmt ) + strlen( filepath ) - strlen( zimr_website_get_pubdir( connection->website ) ) ];
	sprintf( html_header, html_header_fmt, filepath + strlen( zimr_website_get_pubdir( connection->website ) ) );
	char html_footer[ ] = "<br/>"  "Zimr/" ZIMR_VERSION "\n</body></html>";

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
	ztransport_t* transport = ztransport_open( connection->website->sockfd, ZT_WO, connection->sockfd );
	response_set_status( &connection->response, 200 );
	headers_set_header( &connection->response.headers, "Content-Type", mime_get_type( ".html" ) );
	sprintf( sizebuf, "%d", size );
	headers_set_header( &connection->response.headers, "Content-Length", sizebuf );

	zimr_connection_send_status( transport, connection );
	zimr_connection_send_headers( transport, connection );

	ztransport_write( transport, html_header, strlen( html_header ) );

	for ( i = 0; i < num; i++ ) {
		ztransport_write( transport, files[ i ], strlen( files[ i ] ) );
		free( files[ i ] );
	}

	ztransport_write( transport, html_footer, strlen( html_footer ) );

	ztransport_close( transport );
}

void zimr_register_page_handler( const char* page_type, void (*page_handler)( connection_t*, const char*, void* ), void* udata ) {
	strcpy( page_handlers[ page_handler_count ].page_type, page_type );
	page_handlers[ page_handler_count ].page_handler = page_handler;
	page_handlers[ page_handler_count ].udata = udata;
	page_handler_count++;
}

void zimr_connection_default_page_handler( connection_t* connection, char* filepath ) {
	int fd;
	struct stat file_stat;
	char sizebuf[ 32 ];
	ztransport_t* transport;

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

	transport = ztransport_open( connection->website->sockfd, ZT_WO, connection->sockfd );

	if ( !headers_get_header( &connection->response.headers, "Content-Type" ) )
		headers_set_header( &connection->response.headers, "Content-Type", mime_get_type( filepath ) );

	sprintf( sizebuf, "%d", (int) file_stat.st_size );
	headers_set_header( &connection->response.headers, "Content-Length", sizebuf );

	zimr_connection_send_status( transport, connection );
	zimr_connection_send_headers( transport, connection );

	zfd_set( fd, PFD_TYPE_FILE, transport );
}

bool zimr_open_request_log( ) {
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

	sprintf( buffer, "%s, %s, %s, %s, http://%s/%s, %d, %s\n",
		now_str,
		inet_ntoa( connection->ip ),
		connection->hostname,
		HTTP_TYPE( connection->request.type ),
		connection->website->url,
		connection->request.url,
		connection->response.http_status,
		header ? header->value : ""
	);

	if ( write( reqlogfd, buffer, strlen( buffer ) ) == -1 ) {
		perror( "error" );
	}
}

void zimr_close_request_log( ) {
	close( reqlogfd );
}
