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

int fds[ 1024 ][ 1024 ];

int socket_open( in_addr_t addr, int portno );
void request_accept_handler( int sockfd, void* udata );
void request_handler( int sockfd, void* udata );
void response_handler( int fd, pcom_transport_t* transport );

int main( ) {
	int pd, sockfd;

	pfd_register_type( 1, PFD_TYPE_HDLR response_handler );
	pfd_register_type( 2, PFD_TYPE_HDLR request_accept_handler );
	pfd_register_type( 3, PFD_TYPE_HDLR request_handler );

	if ( ( pd = pcom_create( ) ) < 0 ) {
		return EXIT_FAILURE;
	}

	pfd_set( pd, 1, pcom_open( pd, PCOM_RO, 0, 0 ) );

	sockfd = socket_open( INADDR_ANY, 8081 );

	pfd_set( sockfd, 2, NULL );

	pfd_start( );

	return 1;
}

int socket_open( in_addr_t addr, int portno ) {
	int sockfd = socket( AF_INET, SOCK_STREAM, 0 );
	struct sockaddr_in serv_addr;
	int on = 1; // used by setsockopt

	if ( sockfd < 0 ) {
		perror( "[error] psocket_open: socket() failed" );
		return 0;
	}

	memset( &serv_addr, 0, sizeof( serv_addr ) );

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = addr;
	serv_addr.sin_port = htons( portno );

	setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof( on ) );

	if ( bind( sockfd, (struct sockaddr*) &serv_addr, sizeof( serv_addr ) ) < 0 ) {
		perror( "[error] psocket_open: bind() failed" );
		close( sockfd );
		return 0;
	}

	if ( listen( sockfd, SOCK_N_PENDING ) < 0 ) {
		perror( "[error] psocket_open: listen() failed" );
		return 0;
	}

	return sockfd;
}

void request_accept_handler( int sockfd, void* udata ) {
	struct sockaddr_in cli_addr;
	unsigned int cli_len = sizeof ( cli_addr );
	int newsockfd;

	/* Connection request on original socket.  */
	if ( ( newsockfd = accept( sockfd, (struct sockaddr *) &cli_addr, &cli_len ) ) < 0 ) {
		perror("[error] request_accept_handler: accept() failed");
		return;
	}

	printf( "accepted\n" );

	pfd_set( newsockfd, 3, NULL );
}

void request_handler( int sockfd, void* udata ) {
	char buffer[ PCOM_MSG_SIZE ];
	int n;

	memset( &buffer, 0, sizeof( buffer ) );

	if ( ( n = read( sockfd, buffer, sizeof( buffer ) ) ) > 0 ) {
		printf( "%s\n", buffer );
		pfd_clr( sockfd );
		close( sockfd );
	} else {
		pfd_clr( sockfd );
		close( sockfd );
	}

}

void command_handler( pcom_transport_t* transport ) {

	printf( "%d: %d:%d\n", transport->header->key, *(int*) transport->message, *(int*) ( transport->message + sizeof( int ) ) );
	int req_pd = pcom_connect( *(int*) transport->message, *(int*) ( transport->message + sizeof( int ) ) );

	pcom_transport_t* transport2 = pcom_open( req_pd, PCOM_WO, 1, transport->header->key );
	pcom_close( transport2 );

	transport2 = pcom_open( req_pd, PCOM_WO, 2, transport->header->key );
	pcom_close( transport2 );

	transport2 = pcom_open( req_pd, PCOM_WO, 3, transport->header->key );
	pcom_close( transport2 );

	close( req_pd );
}

void response_handler( int res_fd, pcom_transport_t* transport ) {
	char filename[ 100 ];
	int fd;

	pcom_read( transport );

	if ( PCOM_MSG_IS_FIRST( transport ) ) {
		if ( transport->header->id < 0 ) {
			command_handler( transport );
			return;
		}

		sprintf( filename, "out/%d", transport->header->key );
		mkdir( filename, 0777 );
		sprintf( filename, "out/%d/%d.out", transport->header->key, transport->header->id );
		if ( ( fd = creat( filename, 0644 ) ) < 0 ) {
			perror( "[error] main: creat() could not create file" );
			return;
		}
		fds[ transport->header->key ][ transport->header->id ] = fd;
		printf( "%d: create %d\n", transport->header->key, transport->header->id );
	}

	if ( write( fds[ transport->header->key ][ transport->header->id ], transport->message, transport->header->size ) != transport->header->size )
		perror( "[error] main: write failed" );

	if ( PCOM_MSG_IS_LAST( transport ) ) {
		pfd_clr( fds[ transport->header->key ][ transport->header->id ] );
		close( fds[ transport->header->key ][ transport->header->id ] );
		printf( "%d: close %d\n", transport->header->key, transport->header->id );
	}
}
