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

#if defined(DEBUG)
// Setup syslog logging - see SETLOGMASK(3)
	setlogmask( LOG_UPTO( LOG_DEBUG ) );
	openlog( DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER );
#else
	setlogmask( LOG_UPTO( LOG_INFO ) );
	openlog( DAEMON_NAME, LOG_CONS, LOG_USER );
#endif

	// Register file descriptor type handlers
	pfd_register_type( PFD_TYPE_INT_CONNECTED, PFD_TYPE_HDLR podora_connection_handler );
	//pfd_register_type( PFD_TYPE_FILE, PFD_TYPE_HDLR podora_file_handler );

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
	/* use the sockfd as the website id */
	website->id = website_data->socket->sockfd;
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
/*
void podora_website_set_connection_handler( website_t* website, void (*connection_handler)( connection_t* connection ) ) {
	website_data_t* website_data = (website_data_t*) website->data;
	website_data->connection_handler = connection_handler;
}

void podora_website_unset_connection_handler( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->data;
	website_data->connection_handler = NULL;
}

void podora_website_set_pubdir( website_t* website, const char* pubdir ) {
	website_data_t* website_data = (website_data_t*) website->data;
	if ( website_data->pubdir ) free( website_data->pubdir );
	website_data->pubdir = strdup( pubdir );
}

char* podora_website_get_pubdir( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->data;
	return website_data->pubdir;
}

void podora_website_default_connection_handler( connection_t* connection ) {
	podora_connection_send_file( connection, connection->request.url, 1 );
}
*/
void podora_connection_handler( int sockfd, website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->udata;

	puts( "Request:" );
	ptransport_t* transport = ptransport_open( sockfd, PT_RO, 0 );

	if ( !ptransport_read( transport ) ) {
		// bad socket.
		puts( "bad" );
		website_data->status = WS_STATUS_ENABLING;
		pfd_clr( website_data->socket->sockfd );
		psocket_close( website_data->socket );
		website_data->socket = NULL;
		pfd_unblock( );
		return;
	}

	puts( inet_ntoa( *(struct in_addr*) transport->message ) );
	puts( transport->message + 4 );
	puts( transport->message + 4 + strlen( transport->message + 4 ) + 1 );

	int msgid = transport->header->msgid;

	ptransport_close( transport );

	transport = ptransport_open( website_data->socket->sockfd, PT_WO, msgid );
	ptransport_write( transport, "hello world", 11 );
	ptransport_close( transport );
/*
	connection_t* connection;
	website_data_t* website_data = (website_data_t*) website->data;
	connection = connection_create( website, transport->header->id, transport->message, transport->header->size );

	response_set_status( &connection->response, 200 );
	// To ensure thier position at the top of the headers
	headers_set_header( &connection->response.headers, "Date", "" );
	headers_set_header( &connection->response.headers, "Server", "" );

	if ( website_data->connection_handler )
		website_data->connection_handler( connection );
	else
		podora_website_default_connection_handler( connection );

	pcom_close( transport );*/
}
/*
void podora_file_handler( int fd, pcom_transport_t* transport ) {
	int n;
	char buffer[ 2048 ];

	if ( ( n = read( fd, buffer, sizeof( buffer ) ) ) ) {
		if ( !pcom_write( transport, buffer, n ) ) {
			syslog( LOG_ERR, "file_handler: pcom_write() failed" );
			return;
		}
	} else {
		pcom_close( transport );
		pfd_clr( fd );
		close( fd );
	}
}

void podora_connection_send_status( pcom_transport_t* transport, connection_t* connection ) {
	char status_line[ 100 ];
	sprintf( status_line, "HTTP/%s %s" HTTP_HDR_ENDL, connection->http_version, response_status( connection->response.http_status ) );

	pcom_write( transport, status_line, strlen( status_line ) );
}

void podora_connection_send_headers( pcom_transport_t* transport, connection_t* connection ) {
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

	pcom_write( transport, (void*) headers, strlen( headers ) );
	free( cookies );
	free( headers );
}

void podora_connection_send( connection_t* connection, void* message, int size ) {
	char sizebuf[ 10 ];
	pcom_transport_t* transport = pcom_open( res_fd, PCOM_WO, connection->sockfd, connection->website->key );

	if ( !headers_get_header( &connection->response.headers, "Content-Type" ) ) {
		headers_set_header( &connection->response.headers, "Content-Type", mime_get_type( ".html" ) );
	}

	sprintf( sizebuf, "%d", size );
	headers_set_header( &connection->response.headers, "Content-Length", sizebuf );

	podora_connection_send_status( transport, connection );
	podora_connection_send_headers( transport, connection );

	pcom_write( transport, (void*) message, size );
	pcom_close( transport );
	connection_free( connection );
}

void podora_connection_send_file( connection_t* connection, char* filepath, unsigned char use_pubdir ) {
	struct stat file_stat;
	char full_filepath[ 256 ];
	char* extension;
	int i;

	full_filepath[ 0 ] = '\0';

	if ( use_pubdir )
		strcpy( full_filepath, ((website_data_t*) connection->website->data)->pubdir );

	strcat( full_filepath, filepath );

	if ( stat( full_filepath, &file_stat ) ) {
		podora_connection_send_error( connection );
		return;
	}

	if ( S_ISDIR( file_stat.st_mode ) ) {

		if ( full_filepath + strlen( filepath ) != (char*) "/" )
			strcat( full_filepath, "/" );

		strcat( full_filepath, "default.html" );
	}

	extension = strrchr( full_filepath, '.' );

	if ( extension != NULL && *( extension + 1 ) != '\0' ) {
		extension++;

		for ( i = 0; i < page_handler_count; i++ ) {
			if ( strcmp( page_handlers[ i ].page_type, extension ) == 0 ) {
				page_handlers[ i ].page_handler( connection, full_filepath, page_handlers[ i ].udata );
				break;
			}
		}

	} else {
		i = page_handler_count;
	}

	if ( i == page_handler_count )
		podora_connection_default_page_handler( connection, full_filepath );

}

void podora_connection_send_error( connection_t* connection ) {
	char sizebuf[ 10 ];
	pcom_transport_t* transport = pcom_open( res_fd, PCOM_WO, connection->sockfd, connection->website->key );

	response_set_status( &connection->response, 404 );
	headers_set_header( &connection->response.headers, "Content-Type", mime_get_type( ".html" ) );
	sprintf( sizebuf, "%d", 48 );
	headers_set_header( &connection->response.headers, "Content-Length", sizebuf );

	podora_connection_send_status( transport, connection );
	podora_connection_send_headers( transport, connection );

	pcom_write( transport, (void*) "<html><body><h1>404 Not Found</h1></body></html>", 48 );
	pcom_close( transport );
	connection_free( connection );
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
	char sizebuf[ 10 ];
	pcom_transport_t* transport;

	if ( stat( filepath, &file_stat ) ) {
		podora_connection_send_error( connection );
		return;
	}

	if ( ( fd = open( filepath, O_RDONLY ) ) < 0 ) {
		podora_connection_send_error( connection );
		return;
	}

	transport = pcom_open( res_fd, PCOM_WO, connection->sockfd, connection->website->key );

	if ( !headers_get_header( &connection->response.headers, "Content-Type" ) )
		headers_set_header( &connection->response.headers, "Content-Type", mime_get_type( filepath ) );

	sprintf( sizebuf, "%d", (int) file_stat.st_size );
	headers_set_header( &connection->response.headers, "Content-Length", sizebuf );

	podora_connection_send_status( transport, connection );
	podora_connection_send_headers( transport, connection );
	connection_free( connection );

	pfd_set( fd, PFD_TYPE_FILE, transport );
}
*/
