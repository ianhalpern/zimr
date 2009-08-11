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

#include "podora.h"

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

const char* podora_version( ) {
	return PODORA_VERSION;
}

const char* podora_build_date( ) {
	return BUILD_DATE;
}

void podora_init( ) {

	// Setup syslog logging - see SETLOGMASK(3)
#if defined(DEBUG)
	setlogmask( LOG_UPTO( LOG_DEBUG ) );
	openlog( DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER );
#else
	setlogmask( LOG_UPTO( LOG_INFO ) );
	openlog( DAEMON_NAME, LOG_CONS, LOG_USER );
#endif

	// Register file descriptor type handlers
	pfd_register_type( PFD_TYPE_INT_CONNECTED, PFD_TYPE_HDLR podora_connection_handler );
	pfd_register_type( PFD_TYPE_FILE, PFD_TYPE_HDLR podora_file_handler );

}

void podora_shutdown( ) {
	while ( website_get_root( ) ) {
		podora_website_destroy( website_get_root( ) );
	}
}

void podora_start( ) {
	int timeout = 0;
	website_t* website;
	website_data_t* website_data;

	do {
		timeout = 0; // reset timeout value

		website = website_get_root( );

		// Test and Start website proxies
		while ( website ) {
			website_data = (website_data_t*) website->udata;

			if ( !website_data->socket && website_data->status == WS_STATUS_ENABLING ) {

				/* connect to the Podora Daemon Proxy for routing external
				   requests or commands to this process. */
				if ( !podora_website_enable( website ) ) {
					printf( "could not connect to proxy, " );

					if ( PD_NUM_PROXY_DEATH_RETRIES > ++website_data->conn_tries ) {
						timeout = PD_PROXY_DEATH_RETRY_DELAY;
						puts( "retrying..." );
					}

					/* giving up ... */
					else {
						puts( "giving up." );
						podora_website_disable( website );
						//podora_website_destroy( website );
					}

				}

				/* successfully connected to Podora Daemon Proxy, reset number of retries */
				else
					website_data->conn_tries = 0;
			}

			website = website->next;
		}

	} while ( website_get_root( ) && pfd_select( timeout ) );

}

int podora_send_cmd( psocket_t* socket, int cmd, void* message, int size ) {

	int ret = 1;
	ptransport_t* transport;

	transport = ptransport_open( socket->sockfd, PT_WO, cmd );

	if ( ptransport_write( transport, message, size ) <= 0 ) {
		ret = 0;
		goto quit;
	}

	ptransport_close( transport );

	transport = ptransport_open( socket->sockfd, PT_RO, 0 );

	if ( ptransport_read( transport ) <= 0 ) {
		ret = 0;
		goto quit;
	}

	if ( strcmp( "OK", transport->message ) != 0 ) {
		printf( "command failed %s\n", (char*) transport->message );
		ret = 0;
	}

quit:
	ptransport_close( transport );
	return ret;
}

int podora_cnf_load( ) {
	pcnf_t* cnf = pcnf_load( );
	if ( !cnf ) return 0;

	pcnf_website_t* website_cnf = cnf->website_node;

	while ( website_cnf ) {
		if ( website_cnf->url ) {
			website_t* website = podora_website_create( website_cnf->url );

			if ( website_cnf->pubdir )
				podora_website_set_pubdir( website, website_cnf->pubdir );

			podora_website_enable( website );
		}
		website_cnf = website_cnf->next;
	}

	pcnf_free( cnf );
	return 1;
}

website_t* podora_website_create( char* url ) {
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
	website_data->conn_tries = 0;
	website_data->default_pages_count = 0;
	podora_website_insert_default_page( website, "default.html", 0 );
	return website;
}

void podora_website_destroy( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->udata;

	podora_website_disable( website );

	if ( website_data->pubdir )
		free( website_data->pubdir );

	free( website_data );
	website_remove( website );
}

int podora_website_enable( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->udata;

	if ( website_data->status == WS_STATUS_ENABLED ) return 1;

	website_data->status = WS_STATUS_ENABLING;
	website_data->socket = psocket_connect( inet_addr( PD_PROXY_ADDR ), PD_PROXY_PORT );

	if ( !website_data->socket ) return 0;

	// send website enable command
	if ( !podora_send_cmd( website_data->socket, PD_CMD_WS_START, website->url, strlen( website->url ) ) ) {
		// WS_START_CMD failed...disable website
		psocket_close( website_data->socket );
		website_data->socket = NULL;
		return 0;
	}

	// Everything is good, start listening for requests.
	/* set website sockfd */
	website->sockfd = website_data->socket->sockfd;
	website_data->status = WS_STATUS_ENABLED;
	pfd_set( website_data->socket->sockfd, PFD_TYPE_INT_CONNECTED, website );

	return 1;
}

void podora_website_disable( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->udata;

	/* we only need to send command if the socket is connected. */
	if ( website_data->socket ) {

		if ( website_data->status == WS_STATUS_ENABLED )
			podora_send_cmd( website_data->socket, PD_CMD_WS_STOP, NULL, 0 );

		pfd_clr( website_data->socket->sockfd );
		psocket_close( website_data->socket );
		website_data->socket = NULL;

	}

	website_data->status = WS_STATUS_DISABLED;
}

void podora_website_set_connection_handler( website_t* website, void (*connection_handler)( connection_t* connection ) ) {
	website_data_t* website_data = (website_data_t*) website->udata;
	website_data->connection_handler = connection_handler;
}

void podora_website_unset_connection_handler( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->udata;
	website_data->connection_handler = NULL;
}

void podora_website_set_pubdir( website_t* website, const char* pubdir ) {
	website_data_t* website_data = (website_data_t*) website->udata;

	if ( website_data->pubdir ) free( website_data->pubdir );
	website_data->pubdir = (char*) malloc( strlen( pubdir + 2 ) );
	strcpy( website_data->pubdir, pubdir );

	// we need the trailing forward slash
	if ( website_data->pubdir[ strlen( website_data->pubdir ) ] != '/' )
		strcat( website_data->pubdir, "/" );
}

char* podora_website_get_pubdir( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->udata;
	return website_data->pubdir;
}

void podora_website_insert_default_page( website_t* website, const char* default_page, int pos ) {
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

static void podora_website_default_connection_handler( connection_t* connection ) {
	podora_connection_send_file( connection, connection->request.url, true );
	connection_free( connection );
}

void podora_connection_handler( int sockfd, website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->udata;

	ptransport_t* transport = ptransport_open( sockfd, PT_RO, 0 );

	if ( !ptransport_read( transport ) ) {
		// bad socket.
		puts( "bad proxy socket...killing" );
		website_data->status = WS_STATUS_ENABLING;
		pfd_clr( website_data->socket->sockfd );
		psocket_close( website_data->socket );
		website_data->socket = NULL;
		pfd_unblock( );
		return;
	}

	int msgid = transport->header->msgid;
	if ( PT_MSG_IS_FIRST( transport ) ) {
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

	if ( PT_MSG_IS_LAST( transport ) ) {
		connection_t* connection;
		connection = connection_create( website, msgid, requests[ msgid ].data, requests[ msgid ].size );

		printf( "%s %s %s\n",
			inet_ntoa( connection->ip ),
			connection->hostname,
			connection->request.url
		);

		// start optimistically
		response_set_status( &connection->response, 200 );
		// To ensure thier position at the top of the headers
		headers_set_header( &connection->response.headers, "Date", "" );
		headers_set_header( &connection->response.headers, "Server", "" );

		if ( website_data->connection_handler )
			website_data->connection_handler( connection );
		else
			podora_website_default_connection_handler( connection );
	}

	ptransport_close( transport );
}

void podora_file_handler( int fd, ptransport_t* transport ) {
	int n;
	char buffer[ 2048 ];

	if ( ( n = read( fd, buffer, sizeof( buffer ) ) ) ) {
		if ( !ptransport_write( transport, buffer, n ) ) {
			ptransport_close( transport );
			pfd_clr( fd );
			close( fd );

			syslog( LOG_ERR, "file_handler: pcom_write() failed" );
		}
	} else {
		ptransport_close( transport );
		pfd_clr( fd );
		close( fd );
	}
}

void podora_connection_send_status( ptransport_t* transport, connection_t* connection ) {
	char status_line[ 100 ];
	sprintf( status_line, "HTTP/%s %s" HTTP_HDR_ENDL, connection->http_version, response_status( connection->response.http_status ) );

	ptransport_write( transport, status_line, strlen( status_line ) );
}

void podora_connection_send_headers( ptransport_t* transport, connection_t* connection ) {
	time_t now;
	char now_str[ 80 ];

	time( &now );
	strftime( now_str, 80, "%a %b %d %I:%M:%S %Z %Y", localtime( &now ) );

	headers_set_header( &connection->response.headers, "Date", now_str );
	headers_set_header( &connection->response.headers, "Server", "Podora/" PODORA_VERSION );

	char* cookies = cookies_to_string( &connection->cookies, (char*) malloc( 1024 ), 1024 );

	if ( strlen( cookies ) )
		headers_set_header( &connection->response.headers, "Set-Cookie", cookies );

	char* headers = headers_to_string( &connection->response.headers, (char*) malloc( 1024 ) );

	ptransport_write( transport, (void*) headers, strlen( headers ) );
	free( cookies );
	free( headers );
}

void podora_connection_send( connection_t* connection, void* message, int size ) {
	char sizebuf[ 32 ];
	ptransport_t* transport = ptransport_open( connection->website->sockfd, PT_WO, connection->sockfd );

	if ( !headers_get_header( &connection->response.headers, "Content-Type" ) ) {
		headers_set_header( &connection->response.headers, "Content-Type", mime_get_type( ".html" ) );
	}

	sprintf( sizebuf, "%d", size );
	headers_set_header( &connection->response.headers, "Content-Length", sizebuf );

	podora_connection_send_status( transport, connection );
	podora_connection_send_headers( transport, connection );

	ptransport_write( transport, (void*) message, size );
	ptransport_close( transport );
}

void podora_connection_send_file( connection_t* connection, char* filepath, bool use_pubdir ) {
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
		podora_connection_send_error( connection );
		return;
	}

	if ( S_ISDIR( file_stat.st_mode ) ) {

		if ( full_filepath[ strlen( filepath ) ] != '/' )
			strcat( full_filepath, "/" );

		// search for the directory default page
		ptr = full_filepath + strlen( full_filepath );
		for ( i = 0; i < website_data->default_pages_count; i++ ) {
			strcpy( ptr, website_data->default_pages[ i ] );
			if ( !stat( full_filepath, &file_stat ) )
				break; // default file exists...break loop
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
		podora_connection_default_page_handler( connection, full_filepath );

}

void podora_connection_send_error( connection_t* connection ) {
	char sizebuf[ 10 ];
	ptransport_t* transport = ptransport_open( connection->website->sockfd, PT_WO, connection->sockfd );

	response_set_status( &connection->response, 404 );
	headers_set_header( &connection->response.headers, "Content-Type", mime_get_type( ".html" ) );
	sprintf( sizebuf, "%d", 48 );
	headers_set_header( &connection->response.headers, "Content-Length", sizebuf );

	podora_connection_send_status( transport, connection );
	podora_connection_send_headers( transport, connection );

	ptransport_write( transport, (void*) "<html><body><h1>404 Not Found</h1></body></html>", 48 );
	ptransport_close( transport );
}

void podora_register_page_handler( const char* page_type, void (*page_handler)( connection_t*, const char*, void* ), void* udata ) {
	strcpy( page_handlers[ page_handler_count ].page_type, page_type );
	page_handlers[ page_handler_count ].page_handler = page_handler;
	page_handlers[ page_handler_count ].udata = udata;
	page_handler_count++;
}

void podora_connection_default_page_handler( connection_t* connection, char* filepath ) {
	int fd;
	struct stat file_stat;
	char sizebuf[ 32 ];
	ptransport_t* transport;

	if ( stat( filepath, &file_stat ) ) {
		podora_connection_send_error( connection );
		return;
	}

	if ( ( fd = open( filepath, O_RDONLY ) ) < 0 ) {
		podora_connection_send_error( connection );
		return;
	}

	transport = ptransport_open( connection->website->sockfd, PT_WO, connection->sockfd );

	if ( !headers_get_header( &connection->response.headers, "Content-Type" ) )
		headers_set_header( &connection->response.headers, "Content-Type", mime_get_type( filepath ) );

	sprintf( sizebuf, "%d", (int) file_stat.st_size );
	headers_set_header( &connection->response.headers, "Content-Length", sizebuf );

	podora_connection_send_status( transport, connection );
	podora_connection_send_headers( transport, connection );

	pfd_set( fd, PFD_TYPE_FILE, transport );
}

