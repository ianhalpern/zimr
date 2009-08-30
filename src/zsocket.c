
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

#include "zsocket.h"

static zsocket_t* root_zsocket_node = NULL;

zsocket_t* zsocket_open( in_addr_t addr, int portno ) {
	zsocket_t* p = zsocket_get_by_info( addr, portno );

	if ( !p ) {
		int sockfd = zsocket_init( addr, portno, ZSOCK_LISTEN );
		if ( sockfd == -1 )
			return NULL;
		p = zsocket_create( sockfd, addr, portno );
	}

	p->n_open++;
	return p;
}

zsocket_t* zsocket_connect( in_addr_t addr, int portno ) {

	int sockfd = zsocket_init( addr, portno, ZSOCK_CONNECT );
	if ( sockfd == -1 )
		return NULL;

	return zsocket_create( sockfd, addr, portno );
}

int zsocket_init( in_addr_t addr, int portno, int type ) {
	int sockfd = socket( AF_INET, SOCK_STREAM, 0 );
	struct sockaddr_in serv_addr;
	int on = 1; // used by setsockopt

	if ( sockfd < 0 ) {
		zerr( ZMERR_ZSOCK_CREAT );
		return -1;
	}

	memset( &serv_addr, 0, sizeof( serv_addr ) );

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = addr;
	serv_addr.sin_port = htons( portno );

	if ( type == ZSOCK_LISTEN ) {
		setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof( on ) );

		if ( bind( sockfd, (struct sockaddr*) &serv_addr, sizeof( serv_addr ) ) < 0 ) {
			zerr( ZMERR_ZSOCK_BIND );
			close( sockfd );
			return -1;
		}

		if ( listen( sockfd, ZSOCK_N_PENDING ) < 0 ) {
			zerr( ZMERR_ZSOCK_LISTN );
			return -1;
		}
	} else if ( type == ZSOCK_CONNECT ) {
		if ( connect( sockfd, (struct sockaddr*) &serv_addr, sizeof( serv_addr ) ) < 0 ) {
			zerr( ZMERR_ZSOCK_CONN );
			return -1;
		}
	} else {
		close( sockfd );
		return -1;
	}

	return sockfd;
}

zsocket_t* zsocket_create( int sockfd, in_addr_t addr, int portno ) {
	zsocket_t* p = (zsocket_t* ) malloc( sizeof( zsocket_t ) );

	p->sockfd = sockfd;
	p->addr = addr;
	p->portno = portno;
	p->n_open = 0;
	p->ssl = NULL;

	p->next = root_zsocket_node;
	p->prev = NULL;

	if ( root_zsocket_node != NULL )
		root_zsocket_node->prev = p;

	root_zsocket_node = p;

	return p;
}

void zsocket_close( zsocket_t* p ) {

	p->n_open--;

	if ( p->n_open > 0 )
		return;

	if ( p == root_zsocket_node )
		root_zsocket_node = p->next;

	if ( p->prev != NULL )
		p->prev->next = p->next;
	if ( p->next != NULL )
		p->next->prev = p->prev;

	close( p->sockfd );
	free( p );
}

zsocket_t* zsocket_get_by_info( in_addr_t addr, int portno ) {
	zsocket_t* p = root_zsocket_node;

	while ( p != NULL ) {

		if ( p->portno == portno && p->addr == addr )
			return p;

		p = p->next;
	}

	return NULL;
}

zsocket_t* zsocket_get_by_sockfd( int sockfd ) {
	zsocket_t* p = root_zsocket_node;

	while ( p != NULL ) {

		if ( p->sockfd == sockfd )
			return p;

		p = p->next;
	}

	return NULL;
}


zsocket_t* zsocket_get_root( ) {
	return root_zsocket_node;
}
