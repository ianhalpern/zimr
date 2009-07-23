
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

#include "psocket.h"

static psocket_t* root_psocket_node = NULL;

psocket_t* psocket_open( in_addr_t addr, int portno ) {
	psocket_t* p = psocket_get_by_info( addr, portno );

	if ( !p ) {
		int sockfd = psocket_init( addr, portno );
		if ( sockfd == -1 )
			return NULL;
		p = psocket_create( sockfd, addr, portno );
	}

	p->n_open++;
	return p;
}

int psocket_init( in_addr_t addr, int portno ) {
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

psocket_t* psocket_create( int sockfd, in_addr_t addr, int portno ) {
	psocket_t* p = (psocket_t* ) malloc( sizeof( psocket_t ) );

	p->sockfd = sockfd;
	p->addr = addr;
	p->portno = portno;
	p->n_open = 0;

	p->next = root_psocket_node;
	p->prev = NULL;

	if ( root_psocket_node != NULL )
		root_psocket_node->prev = p;

	root_psocket_node = p;

	return p;
}

void psocket_remove( psocket_t* p ) {

	p->n_open--;

	if ( p->n_open > 0 )
		return;

	if ( p == root_psocket_node )
		root_psocket_node = p->next;

	if ( p->prev != NULL )
		p->prev->next = p->next;
	if ( p->next != NULL )
		p->next->prev = p->prev;

	close( p->sockfd );
	free( p );
}

psocket_t* psocket_get_by_info( in_addr_t addr, int portno ) {
	psocket_t* p = root_psocket_node;

	while ( p != NULL ) {

		if ( p->portno == portno && p->addr == addr )
			return p;

		p = p->next;
	}

	return NULL;
}

psocket_t* psocket_get_by_sockfd( int sockfd ) {
	psocket_t* p = root_psocket_node;

	while ( p != NULL ) {

		if ( p->sockfd == sockfd )
			return p;

		p = p->next;
	}

	return NULL;
}


