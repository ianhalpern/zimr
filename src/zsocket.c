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

static bool initialized = false;
static int zsocket_new( in_addr_t addr, int portno, int type );

void zsocket_init( ) {
	assert( !initialized );
	initialized = true;
	memset( zsockets, 0, sizeof( zsockets ) );
}

int zsocket( in_addr_t addr, int portno, int type ) {
	assert( initialized );
	zsocket_t* p;

	if ( type == ZSOCK_LISTEN ) {
		p = zsocket_get_by_info( addr, portno );
		if ( p ) {
			p->n_open++;
			return p->sockfd;
		}
	}

	int sockfd = zsocket_new( addr, portno, type );
	if ( sockfd == -1 )
		return -1;

	p = (zsocket_t* ) malloc( sizeof( zsocket_t ) );

	p->sockfd = sockfd;
	p->addr = addr;
	p->portno = portno;
	p->n_open = 0;
	p->ssl = NULL;

	if ( type == ZSOCK_LISTEN )
		p->n_open++;

	zsockets[ sockfd ] = p;

	return sockfd;
}

static int zsocket_new( in_addr_t addr, int portno, int type ) {
	assert( initialized );
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

void zsocket_close( int zs ) {
	assert( initialized );

	zsocket_t* p = zsockets[ zs ];

	p->n_open--;

	if ( p->n_open > 0 )
		return;

	zsockets[ zs ] = NULL;

	close( p->sockfd );
	free( p );
}

zsocket_t* zsocket_get_by_info( in_addr_t addr, int portno ) {
	assert( initialized );

	int i;
	for ( i = 0; i < sizeof( zsockets ) / sizeof( zsockets[ 0 ] ); i++ ) {
		zsocket_t* p = zsockets[ i ];
		if ( p ) {
			if ( p->portno == portno && p->addr == addr )
				return p;
		}
	}

	return NULL;
}

zsocket_t* zsocket_get_by_sockfd( int sockfd ) {
	assert( initialized );

	int i;
	for ( i = 0; i < sizeof( zsockets ) / sizeof( zsockets[ 0 ] ); i++ ) {
		zsocket_t* p = zsockets[ i ];
		if ( p ) {
			if ( p->sockfd == sockfd )
				return p;
		}
	}

	return NULL;
}
