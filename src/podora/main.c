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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "pcom.h"
#include "pfildes.h"
#include "website.h"

typedef struct website_data {
	int fd;
} website_data_t;

int res_fd;

void save_podora_info( );
int  socket_open( in_addr_t addr, int portno );

void request_accept_handler( int sockfd, void* udata );
void request_handler( int sockfd, void* udata );
void response_handler( int fd, pcom_transport_t* transport );

void start_website( pcom_transport_t* transport );
void stop_website( pcom_transport_t* transport );

int main( ) {
	int sockfd;

	printf( "Podora " PODORA_VERSION " (" BUILD_DATE ")\n\n" );

	pfd_register_type( 1, PFD_TYPE_HDLR response_handler );
	pfd_register_type( 2, PFD_TYPE_HDLR request_accept_handler );
	pfd_register_type( 3, PFD_TYPE_HDLR request_handler );

	if ( ( res_fd = pcom_create( ) ) < 0 ) {
		return EXIT_FAILURE;
	}

	save_podora_info( );

	pfd_set( res_fd, 1, pcom_open( res_fd, PCOM_RO, 0, 0 ) );

	sockfd = socket_open( INADDR_ANY, 8081 );

	pfd_set( sockfd, 2, NULL );

	pfd_start( );

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

int socket_open( in_addr_t addr, int portno ) {
	int sockfd = socket( AF_INET, SOCK_STREAM, 0 );
	struct sockaddr_in serv_addr;
	int on = 1; // used by setsockopt

	if ( sockfd < 0 ) {
		perror( "[error] psocket_open: socket() failed" );
		return -1;
	}

	memset( &serv_addr, 0, sizeof( serv_addr ) );

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = addr;
	serv_addr.sin_port = htons( portno );

	setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof( on ) );

	if ( bind( sockfd, (struct sockaddr*) &serv_addr, sizeof( serv_addr ) ) < 0 ) {
		perror( "[error] psocket_open: bind() failed" );
		close( sockfd );
		return -1;
	}

	if ( listen( sockfd, SOCK_N_PENDING ) < 0 ) {
		perror( "[error] psocket_open: listen() failed" );
		return -1;
	}

	printf( "[socket] started socket \"http://%s:%d\"\n", "0.0.0.0", portno );

	return sockfd;
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

	memset( &buffer, 0, sizeof( buffer ) );

	if ( ( n = read( sockfd, buffer, sizeof( buffer ) ) ) < 0 ) {
		perror( "[error] request_handler: read() failed" );
		return;
	}

	printf( "%s\n", buffer );

	if ( !( website = website_get_root( ) ) ) {
		fprintf( stderr, "[warning] request_handler: no website to service request\n" );
		pfd_clr( sockfd );
		close( sockfd );
		return;
	}

	website_data = website->data;

	transport = pcom_open( website_data->fd, PCOM_WO, sockfd, website->id );
	pcom_flush( transport );
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

	pcom_read( transport );

	if ( PCOM_MSG_IS_FIRST( transport ) ) {
		if ( transport->header->id < 0 ) {
			command_handler( transport );
			return;
		}
	}

	if ( write( transport->header->id, transport->message, transport->header->size ) != transport->header->size )
		perror( "[error] response_handler: write failed" );

	if ( PCOM_MSG_IS_LAST( transport ) ) {
		pfd_clr( transport->header->id );
		close( transport->header->id );
	}
}

////////////////////////////////////////////////////////////////

void start_website( pcom_transport_t* transport ) {
	website_t* website;
	website_data_t* website_data;

	int pid = *(int*) transport->message;
	int fd = *(int*) ( transport->message + sizeof( int ) );
	char* url = transport->message + sizeof( int ) * 2;

	if ( website_get_by_id( transport->header->key ) ) {
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

}

void stop_website( pcom_transport_t* transport ) {
	website_t* website;
	website_data_t* website_data;

	if ( !( website = website_get_by_id( transport->header->key ) ) ) {
		fprintf( stderr, "[warning] stop_website: tried to stop a nonexisting website\n" );
		return;
	}

	printf( "[website] stopping \"http://%s\"\n", website->url );

	website_data = (website_data_t*) website->data;

	close( website_data->fd );

	free( website_data );
	website_remove( website );
}
