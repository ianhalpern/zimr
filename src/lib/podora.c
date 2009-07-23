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
	void* (*page_handler)( response_t*, const char*, void* );
	void* udata;
} page_handler_t;

static int res_fd;
static int page_handler_count = 0;
static page_handler_t page_handlers[ 100 ];

const char* podora_version( ) {
	return PODORA_VERSION;
}

const char* podora_build_date( ) {
	return BUILD_DATE;
}

int podora_init( ) {
	int pid, res_filenum;

	podora_read_server_info( &pid, &res_filenum );

	if ( ( res_fd = pcom_connect( pid, res_filenum ) ) < 0 ) {
		return 0;
	}

	pfd_register_type( PFD_TYPE_PCOM, PFD_TYPE_HDLR &podora_request_handler );
	pfd_register_type( PFD_TYPE_FILE, PFD_TYPE_HDLR &podora_file_handler );

	return 1;
}

void podora_shutdown( ) {
	while ( website_get_root( ) ) {
		podora_website_destroy( website_get_root( ) );
	}
}

int podora_read_server_info( int* pid, int* res_fn ) {
	int fd;

	if ( ( fd = open( PD_INFO_FILE, O_RDONLY ) ) < 0 ) {
		perror( "[fatal] read_podora_info: open() failed" );
		return 0;
	}

	read( fd, pid, sizeof( *pid ) );
	read( fd, res_fn, sizeof( *res_fn ) );

	close( fd );

	return 1;
}

void podora_start( ) {
	pfd_start( );
}

void podora_send_cmd( int cmd, int key, void* message, int size ) {
	pcom_transport_t* transport = pcom_open( res_fd, PCOM_WO, cmd, key );
	pcom_write( transport, message, size );
	pcom_close( transport );
}

website_t* podora_website_create( char* url ) {
	int req_fd;

	if ( ( req_fd = pcom_create( ) ) < 0 ) {
		return NULL;
	}

	website_t* website;
	website_data_t* website_data;

	if ( !( website = website_add( randkey( ), url ) ) )
		return NULL;

	website_data = (website_data_t*) malloc( sizeof( website_data_t ) );
	website->data = (void*) website_data;

	website_data->req_fd = req_fd;
	website_data->pubdir = strdup( "./" );
	website_data->status = WS_STATUS_STOPPED;

	pfd_set( website_data->req_fd, PFD_TYPE_PCOM, website );

	return website;
}

void podora_website_destroy( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->data;

	podora_website_stop( website );

	if ( website_data->pubdir )
		free( website_data->pubdir );

	pfd_clr( website_data->req_fd );
	close( website_data->req_fd );

	free( website_data );
	website_remove( website );
}

int podora_website_start( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->data;

	if ( website_data->status == WS_STATUS_STARTED )
		return 0;

	int pid = getpid( );
	char message[ sizeof( int ) * 2 + strlen( website->url ) + 1 ];

	memcpy( message, &pid, sizeof( int ) );
	memcpy( message + sizeof( int ), &website_data->req_fd, sizeof( int ) );
	memcpy( message + sizeof( int ) * 2, website->url, strlen( website->url ) + 1 );

	podora_send_cmd( WS_START_CMD, website->key, message, sizeof( message ) );

	website_data->status = WS_STATUS_STARTED;

	return 1;
}

int podora_website_stop( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->data;

	if ( website_data->status == WS_STATUS_STOPPED )
		return 0;

	podora_send_cmd( WS_STOP_CMD, website->key, NULL, 0 );

	return 1;
}

void podora_website_set_request_handler ( website_t* website, void* (*request_handler)( void* ) ) {
	website_data_t* website_data = (website_data_t*) website->data;
	website_data->request_handler = request_handler;
}

void podora_website_unset_request_handler ( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->data;
	website_data->request_handler = NULL;
}

void podora_website_set_pubdir ( website_t* website, const char* pubdir ) {
	website_data_t* website_data = (website_data_t*) website->data;
	if ( website_data->pubdir ) free( website_data->pubdir );
	website_data->pubdir = strdup( pubdir );
}

void podora_request_handler( int req_fd, website_t* website ) {

	pcom_transport_t* transport = pcom_open( req_fd, PCOM_RO, 0, 0 );
	request_t request;

	pcom_read( transport );

	request = request_create( website, transport->header->id, transport->message, transport->header->size );

	// To ensure thier position at the top of the headers
	headers_set_header( &request.response.headers, "Date", "" );
	headers_set_header( &request.response.headers, "Server", "" );

	podora_response_serve_file( &request.response, request.url, 1 );

	pcom_close( transport );
}

void podora_file_handler( int fd, pcom_transport_t* transport ) {
	int n;
	char buffer[ 2048 ];

	if ( ( n = read( fd, buffer, sizeof( buffer ) ) ) ) {
		if ( !pcom_write( transport, buffer, n ) ) {
			fprintf( stderr, "[error] file_handler: pcom_write() failed\n" );
			return;
		}
	} else {
		pcom_close( transport );
		pfd_clr( fd );
		close( fd );
	}
}

void podora_response_send_status( pcom_transport_t* transport, response_t* response ) {
	char status_line[ 100 ];
	sprintf( status_line, "HTTP/%s %s" HTTP_HDR_ENDL, response->http_version, response_status( response->http_status ) );

	pcom_write( transport, status_line, strlen( status_line ) );
}

void podora_response_send_headers( pcom_transport_t* transport, response_t* response ) {
	time_t now;
	char now_str[ 80 ];

	time( &now );
	strftime( now_str, 80, "%a %b %d %I:%M:%S %Z %Y", localtime( &now ) );

	headers_set_header( &response->headers, "Date", now_str );
	headers_set_header( &response->headers, "Server", "Podora/" PODORA_VERSION );

	const char* headers = headers_to_string( &response->headers );

	pcom_write( transport, (void*) headers, strlen( headers ) );
	free( (char*) headers );
}

void podora_response_serve( response_t* response, void* message, int size ) {
	char sizebuf[ 10 ];
	pcom_transport_t* transport = pcom_open( res_fd, PCOM_WO, response->sockfd, response->website->key );

	response_set_status( response, 200 );
	sprintf( sizebuf, "%d", size );
	headers_set_header( &response->headers, "Content-Length", sizebuf );

	podora_response_send_status( transport, response );
	podora_response_send_headers( transport, response );

	pcom_write( transport, (void*) message, size );
	pcom_close( transport );
}

void podora_response_serve_file( response_t* response, char* filepath, unsigned char use_pubdir ) {
	struct stat file_stat;
	char sizebuf[ 10 ];
	char full_filepath[ 256 ];
	char* extension;
	int i;

	if ( use_pubdir )
		strcpy( full_filepath, ((website_data_t*) response->website->data)->pubdir );

	strcat( full_filepath, filepath );

	if ( stat( full_filepath, &file_stat ) ) {

		pcom_transport_t* transport = pcom_open( res_fd, PCOM_WO, response->sockfd, response->website->key );

		response_set_status( response, 404 );
		headers_set_header( &response->headers, "Content-Type", mime_get_type( ".html" ) );
		sprintf( sizebuf, "%d", 48 );
		headers_set_header( &response->headers, "Content-Length", sizebuf );

		podora_response_send_status( transport, response );
		podora_response_send_headers( transport, response );

		pcom_write( transport, (void*) "<html><body><h1>404 Not Found</h1></body></html>", 48 );
		pcom_close( transport );

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
				page_handlers[ i ].page_handler( response, full_filepath, page_handlers[ i ].udata );
				break;
			}
		}

	} else {
		i = page_handler_count;
	}

	if ( i == page_handler_count )
		podora_response_default_page_handler( response, full_filepath );

}

void podora_response_register_page_handler( const char* page_type, void* (*page_handler)( response_t*, const char*, void* ), void* udata ) {
	strcpy( page_handlers[ page_handler_count ].page_type, page_type );
	page_handlers[ page_handler_count ].page_handler = page_handler;
	page_handlers[ page_handler_count ].udata = udata;
	page_handler_count++;
}

void podora_response_default_page_handler( response_t* response, char* filepath ) {
	int fd;
	struct stat file_stat;
	char sizebuf[ 10 ];
	pcom_transport_t* transport;

	if ( ( fd = open( filepath, O_RDONLY ) ) < 0 ) {
		return;
	}

	fstat( fd, &file_stat );

	transport = pcom_open( res_fd, PCOM_WO, response->sockfd, response->website->key );

	response_set_status( response, 200 );
	headers_set_header( &response->headers, "Content-Type", mime_get_type( filepath ) );

	sprintf( sizebuf, "%d", (int) file_stat.st_size );
	headers_set_header( &response->headers, "Content-Length", sizebuf );

	podora_response_send_status( transport, response );
	podora_response_send_headers( transport, response );

	pfd_set( fd, PFD_TYPE_FILE, transport );
}
