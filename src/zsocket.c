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

void zsocket_init() {
	assert( !initialized );
	initialized = true;
	SSL_library_init();
	SSL_load_error_strings();
	memset( zsockets, 0, sizeof( zsockets ) );
}

int zsocket( in_addr_t addr, int portno, int type, bool ssl ) {
	assert( initialized );
	zsocket_t* p;

	if ( type == ZSOCK_LISTEN ) {
		p = zsocket_get_by_info( addr, portno );
		if ( p ) {
			p->listen.n_open++;
			return p->listen.sockfd;
		}
	}

	int sockfd = zsocket_new( addr, portno, type );
	if ( sockfd == -1 )
		return -1;

	p = (zsocket_t* ) malloc( sizeof( zsocket_t ) );
	p->type = type;

	if ( p->type == ZSOCK_LISTEN ) {
		p->listen.sockfd = sockfd;
		p->listen.addr = addr;
		p->listen.portno = portno;
		p->listen.n_open = 0;
		p->listen.ssl_ctx = NULL;
	} else {
		p->connect.sockfd = sockfd;
		p->connect.addr = addr;
		p->connect.portno = portno;
		p->connect.ssl = NULL;
	}

	if ( type == ZSOCK_LISTEN && ssl ) {
		SSL_METHOD* meth = SSLv3_method();
		p->listen.ssl_ctx = SSL_CTX_new( meth );
		/* Load the server certificate into the SSL_CTX structure */
		/* Load the private-key corresponding to the server certificate */
		if ( !p->listen.ssl_ctx
		  || SSL_CTX_use_certificate_file( p->listen.ssl_ctx, RSA_SERVER_CERT, SSL_FILETYPE_PEM ) <= 0
		  || SSL_CTX_use_PrivateKey_file( p->listen.ssl_ctx, RSA_SERVER_KEY, SSL_FILETYPE_PEM ) <= 0 ) {
			ERR_print_errors_fp( stderr );
			free( p );
			return -1;
		}

		/* Check if the server certificate and private-key matches */
		if ( !SSL_CTX_check_private_key( p->listen.ssl_ctx ) ) {
			fprintf( stderr, "Private key does not match the certificate public key\n" );
			free( p );
			return -1;
		}
	}

	if ( type == ZSOCK_LISTEN )
		p->listen.n_open++;

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

void zclose( int zs ) {
	assert( initialized );

	zsocket_t* p = zsockets[ zs ];

	if ( p->type == ZSOCK_LISTEN ) {
		p->listen.n_open--;

		if ( p->listen.n_open > 0 )
			return;
	}

	zsockets[ zs ] = NULL;

	if ( p->type == ZSOCK_LISTEN ) {
		if ( p->listen.ssl_ctx )
			SSL_CTX_free( p->listen.ssl_ctx );
		close( p->listen.sockfd );
	} else if ( p->type == ZSOCK_CONNECT ) {
		if ( p->connect.ssl )
			SSL_shutdown( p->connect.ssl );
		close( p->connect.sockfd );
	}

	if ( p->type == ZSOCK_CONNECT && p->connect.ssl )
		SSL_free( p->connect.ssl );

	free( p );
}

zsocket_t* zsocket_get_by_info( in_addr_t addr, int portno ) {
	assert( initialized );

	int i;
	for ( i = 0; i < sizeof( zsockets ) / sizeof( zsockets[ 0 ] ); i++ ) {
		zsocket_t* p = zsockets[ i ];
		if ( p ) {
			if ( p->type == ZSOCK_LISTEN && p->listen.portno == portno && p->listen.addr == addr )
				return p;
			else if ( p->type == ZSOCK_CONNECT && p->connect.portno == portno && p->connect.addr == addr )
				return p;
		}
	}

	return NULL;
}

zsocket_t* zsocket_get_by_sockfd( int sockfd ) {
	assert( initialized );

	return zsockets[ sockfd ];
}

int zaccept( int sockfd, struct sockaddr_in* cli_addr, unsigned int* cli_len ) {
	int newsockfd;
	zsocket_t* zs = zsocket_get_by_sockfd( sockfd );
	// Connection request on original socket.
	if ( ( newsockfd = accept( sockfd, (struct sockaddr*)cli_addr, cli_len ) ) < 0 )
		return newsockfd;

	zsocket_t* p = (zsocket_t* ) malloc( sizeof( zsocket_t ) );
	p->type = ZSOCK_CONNECT;
	p->connect.sockfd = newsockfd;
	p->connect.addr = cli_addr->sin_addr.s_addr;
	p->connect.portno = zs->connect.portno;

	if ( zs->listen.ssl_ctx ) {
		if ( !( p->connect.ssl = SSL_new( zs->listen.ssl_ctx ) ) ) {
			free( p );
			return -1;
		}
		SSL_set_fd( p->connect.ssl, newsockfd );
		if ( SSL_accept( p->connect.ssl ) == -1 ) {
			ERR_print_errors_fp( stderr );
			free( p );
			return -1;
		}

	}

	zsockets[ newsockfd ] = p;

	return newsockfd;
}

int zread( int fd, char* buffer, size_t buffer_size ) {
	zsocket_t* zs = zsocket_get_by_sockfd( fd );
	int n;
	if ( zs->connect.ssl ) {
		if ( ( n = SSL_read( zs->connect.ssl, buffer, buffer_size ) ) == -1 ) {
			ERR_print_errors_fp( stderr );
		}
	} else
		n = read( fd, buffer, buffer_size );

	return n;
}

int zwrite( int fd, const char* buffer, size_t buffer_size ) {
	zsocket_t* zs = zsocket_get_by_sockfd( fd );
	int n;
	if ( zs->connect.ssl ) {
		if ( ( n = SSL_write( zs->connect.ssl, buffer, buffer_size ) ) == -1 ) {
			ERR_print_errors_fp( stderr );
		}
	} else
		n = write( fd, buffer, buffer_size );

	return n;
}
