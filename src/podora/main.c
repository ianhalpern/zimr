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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "pcom.h"
#include "pfildes.h"
#include "website.h"
#include "psocket.h"

typedef struct website_data {
	int fd;
	psocket_t* socket;
} website_data_t;

int res_fd;

void save_podora_info( );

void request_accept_handler( int sockfd, void* udata );
void request_handler( int sockfd, void* udata );
void response_handler( int fd, pcom_transport_t* transport );

void start_website( pcom_transport_t* transport );
void stop_website( pcom_transport_t* transport );

void remove_website( website_t* website );

void broken_pipe( ) { }
void quit( ) { }

int main( ) {

	printf( "Podora " PODORA_VERSION " (" BUILD_DATE ")\n\n" );

	signal( SIGPIPE, broken_pipe );
	signal( SIGINT, quit );

	pfd_register_type( PFD_TYPE_PCOM,          PFD_TYPE_HDLR response_handler );
	pfd_register_type( PFD_TYPE_SOCK_LISTEN,   PFD_TYPE_HDLR request_accept_handler );
	pfd_register_type( PFD_TYPE_SOCK_ACCEPTED, PFD_TYPE_HDLR request_handler );

	if ( ( res_fd = pcom_create( ) ) < 0 ) {
		return EXIT_FAILURE;
	}

	save_podora_info( );

	pfd_set( res_fd, PFD_TYPE_PCOM, pcom_open( res_fd, PCOM_RO, 0, 0 ) );

	pfd_start( );

	printf( "quit\n" );
	return 1;
}

void save_podora_info( ) {
	int pid = getpid( ), fd;

	if ( ( fd = creat( PD_INFO_FILE, 0644 ) ) < 0 ) {
		perror( "[fatal] save_podora_info: creat() failed" );
		exit( EXIT_FAILURE );
	}

	write( fd, &pid, sizeof( pid ) );
	write( fd, &res_fd, sizeof( pid ) );
	close( fd );
}

char* get_url_from_http_header( char* url, char* raw ) {
	char* ptr,* ptr2;

	ptr = strstr( raw, "Host: " ) + 6;
	ptr2 = strstr( ptr, HTTP_HDR_ENDL );
	strncpy( url, ptr, ptr2 - ptr );

	*( url + ( ptr2 - ptr ) ) = '\0';

	raw = strstr( raw, " " );
	raw++;

	ptr = strstr( raw, " " );

	strncat( url, raw, ptr - raw );

	return url;
}

int get_port_from_url( char* url ) {
	char* ptr1,* ptr2;
	char port_str[ 6 ];

	memset( port_str, 0, sizeof( port_str ) );

	ptr1 = strstr( url, ":" );
	ptr2 = strstr( url, "/" );

	if ( !ptr2 )
		ptr2 = url + strlen( url );

	if ( !ptr1 || ptr2 < ptr1 )
		return HTTP_DEFAULT_PORT;

	ptr1++;

	strncpy( port_str, ptr1, ptr2 - ptr1 );
	return atoi( port_str );
}

void start_website( pcom_transport_t* transport ) {
	website_t* website;
	website_data_t* website_data;

	int pid = *(int*) transport->message;
	int fd = *(int*) ( transport->message + sizeof( int ) );
	char* url = transport->message + sizeof( int ) * 2;

	if ( website_get_by_url( url ) ) {
		fprintf( stderr, "[warning] start_website: tried to add website that already exists" );
		return;
	}

	if ( !( website = website_add( transport->header->key, url ) ) ) {
		fprintf( stderr, "[error] start_website: website_add() failed starting website \"%s\"", url );
		return;
	}

	printf( "[website] starting \"http://%s\"\n", website->url );

	website_data = (website_data_t*) malloc( sizeof( website_data_t ) );

	website->data = website_data;

	if ( !( website_data->fd = pcom_connect( pid, fd ) ) ) {
		fprintf( stderr, "[error] start_website: pcom_connect() failed for website \"%s\"\n", website->url );
		//stop_website( website->id );
		return;
	}

	website_data->socket = psocket_open( INADDR_ANY, get_port_from_url( website->url ) );

	if ( !website_data->socket ) {
		fprintf( stderr, "[error] start_website: psocket_open() failed\n" );
		remove_website( website );
		return;
	}

	pfd_set( website_data->socket->sockfd, PFD_TYPE_SOCK_LISTEN, NULL );

}

void stop_website( pcom_transport_t* transport ) {
	website_t* website;

	if ( !( website = website_get_by_key( transport->header->key ) ) ) {
		fprintf( stderr, "[warning] stop_website: tried to stop a nonexisting website\n" );
		return;
	}

	remove_website( website );
}

void remove_website( website_t* website ) {
	printf( "[website] stopping \"http://%s\"\n", website->url );
	website_data_t* website_data = (website_data_t*) website->data;

	close( website_data->fd );
	if ( website_data->socket->n_open == 1 ) {
		pfd_clr( website_data->socket->sockfd );
	}
	psocket_remove( website_data->socket );

	free( website_data );
	website_remove( website );
}

// Handlers ////////////////////////////////////////////

void request_accept_handler( int sockfd, void* udata ) {
	struct sockaddr_in cli_addr;
	unsigned int cli_len = sizeof ( cli_addr );
	int newsockfd;

	/* Connection request on original socket.  */
	if ( ( newsockfd = accept( sockfd, (struct sockaddr *) &cli_addr, &cli_len ) ) < 0 ) {
		perror( "[error] request_accept_handler: accept() failed" );
		return;
	}

	pfd_set( newsockfd, 3, NULL );
}

void request_handler( int sockfd, void* udata ) {
	char buffer[ PCOM_MSG_SIZE ];
	int n;
	website_t* website;
	website_data_t* website_data;
	pcom_transport_t* transport;
	int request_type;
	char url[ 256 ];
	char* ptr;
	char postlenbuf[ 16 ];
	int postlen = 0;

	memset( &buffer, 0, sizeof( buffer ) );

	if ( ( n = read( sockfd, buffer, sizeof( buffer ) ) ) < 0 ) {
		perror( "[error] request_handler: read() failed" );
		return;
	}

	if ( startswith( buffer, HTTP_GET ) )
		request_type = HTTP_GET_TYPE;
	else if ( startswith( buffer, HTTP_POST ) )
		request_type = HTTP_POST_TYPE;
	else return;

	if ( !( website = website_get_by_url( get_url_from_http_header( url, buffer ) ) ) ) {
		fprintf( stderr, "[warning] request_handler: no website to service request\n" );
		pfd_clr( sockfd );
		close( sockfd );
		return;
	}

	website_data = website->data;
	transport = pcom_open( website_data->fd, PCOM_WO, sockfd, website->key );

	if ( request_type == HTTP_POST_TYPE && ( ptr = strstr( buffer, "Content-Length: " ) ) ) {
		memcpy( postlenbuf, ptr + 16, (long) strstr( ptr + 16, HTTP_HDR_ENDL ) - (long) ( ptr + 16 ) );
		postlen = atoi( postlenbuf );
	}

	if ( ( ptr = strstr( buffer, HTTP_HDR_ENDL HTTP_HDR_ENDL ) ) ) {
		ptr += strlen( HTTP_HDR_ENDL HTTP_HDR_ENDL );
		pcom_write( transport, (void*) buffer, ( ptr - buffer ) );
	}

	if ( request_type == HTTP_POST_TYPE && postlen ) {
		pcom_write( transport, (void*) ptr, postlen );
	}

	if ( !pcom_flush( transport ) ) {
		pfd_clr( transport->header->id );
		close( transport->header->id );
		if ( errno == EPIPE ) {
			fprintf( stderr, "[warning] request_handler: pipe broken...removing website\n" );
			remove_website( website );
		}
	}

	pcom_free( transport );
}

void command_handler( pcom_transport_t* transport ) {

	switch ( transport->header->id ) {

		case WS_START_CMD:
			start_website( transport );
			break;

		case WS_STOP_CMD:
			stop_website( transport );
			break;

	}

}

void response_handler( int res_fd, pcom_transport_t* transport ) {

	if ( !pcom_read( transport ) )
		return;

	if ( PCOM_MSG_IS_FIRST( transport ) ) {
		if ( transport->header->id < 0 ) {
			command_handler( transport );
			return;
		}
	}

	if ( write( transport->header->id, transport->message, transport->header->size ) != transport->header->size ) {
		perror( "[error] response_handler: write failed" );
		return;
	}

	if ( PCOM_MSG_IS_LAST( transport ) ) {
		pfd_clr( transport->header->id );
		close( transport->header->id );
	}
}

////////////////////////////////////////////////////////////////
