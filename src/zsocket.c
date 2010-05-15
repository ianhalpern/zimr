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
 *  ssl api reference: http://h71000.www7.hp.com/doc/83final/ba554_90007/ch05s04.html
 *  ssl ca reference: https://help.ubuntu.com/community/OpenSSL
 */

#include "zsocket.h"

static bool initialized = false;
static int zsocket_new( in_addr_t addr, int portno, int type );

void zaccepter( int fd, zsock_a_data_t* data );
void zwriter( int fd, zsock_rw_data_t* data );
void zreader( int fd, zsock_rw_data_t* data );

void zsocket_init() {
	assert( !initialized );
	initialized = true;
	SSL_library_init();
	SSL_load_error_strings();
	memset( zsockets, 0, sizeof( zsockets ) );

	zfd_register_type( ZACCEPT_R, ZFD_R, ZFD_TYPE_HDLR zaccepter );
	zfd_register_type( ZACCEPT_W, ZFD_W, ZFD_TYPE_HDLR zaccepter );
	zfd_register_type( ZWRITE, ZFD_W, ZFD_TYPE_HDLR zwriter );
	zfd_register_type( ZREAD,  ZFD_R, ZFD_TYPE_HDLR zreader );
	//zfd_register_type( INLISN, ZFD_R, ZFD_TYPE_HDLR inlisn );
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
		SSL_METHOD* meth = SSLv23_method();
		p->listen.ssl_ctx = SSL_CTX_new( meth );
		// Load the server certificate into the SSL_CTX structure
		// Load the private-key corresponding to the server certificate
		if ( !p->listen.ssl_ctx
		  || SSL_CTX_use_certificate_chain_file( p->listen.ssl_ctx, RSA_SERVER_CERT ) <= 0
		  || SSL_CTX_use_PrivateKey_file( p->listen.ssl_ctx, RSA_SERVER_KEY, SSL_FILETYPE_PEM ) <= 0 ) {
			ERR_print_errors_fp( stderr );
			free( p );
			return -1;
		}
/*
		// Check if the server certificate and private-key matches
		if ( !SSL_CTX_check_private_key( p->listen.ssl_ctx ) ) {
			fprintf( stderr, "Private key does not match the certificate public key\n" );
			free( p );
			return -1;
		}*/
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
		return -1;
	}
	fcntl( sockfd, F_SETFL, fcntl( sockfd, F_GETFL, 0 ) | O_NONBLOCK );

	memset( &serv_addr, 0, sizeof( serv_addr ) );

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = addr;
	serv_addr.sin_port = htons( portno );

	if ( type == ZSOCK_LISTEN ) {
		setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof( on ) );

		if ( bind( sockfd, (struct sockaddr*) &serv_addr, sizeof( serv_addr ) ) < 0 ) {
			close( sockfd );
			return -1;
		}

		if ( listen( sockfd, ZSOCK_N_PENDING ) < 0 ) {
			return -1;
		}
	} else if ( type == ZSOCK_CONNECT ) {
		if ( connect( sockfd, (struct sockaddr*) &serv_addr, sizeof( serv_addr ) ) < 0 ) {
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

void zaccept( int sockfd, struct sockaddr_in* cli_addr, socklen_t* cli_len,
  void (*oncomplete)( int, struct sockaddr_in*, socklen_t*, void* ), void* udata ) {
	int newsockfd;
	printf( "accepting conn for %d\n", sockfd );
	zsocket_t* zs = zsocket_get_by_sockfd( sockfd );
	// Connection request on original socket.
	if ( ( newsockfd = accept( sockfd, (struct sockaddr*)cli_addr, cli_len ) ) == -1 ) {
		perror( "error: accepting: 1" );
		oncomplete( newsockfd, cli_addr, cli_len, udata );
		return;
	}

	fcntl( newsockfd, F_SETFL, fcntl( newsockfd, F_GETFL, 0 ) | O_NONBLOCK );

	zsocket_t* p = (zsocket_t* ) malloc( sizeof( zsocket_t ) );
	p->type = ZSOCK_CONNECT;
	p->connect.sockfd = newsockfd;
	p->connect.addr = cli_addr->sin_addr.s_addr;
	p->connect.portno = zs->connect.portno;
	p->connect.ssl = NULL;
	zsockets[ newsockfd ] = p;

	if ( zs->listen.ssl_ctx ) {
		if ( !( p->connect.ssl = SSL_new( zs->listen.ssl_ctx ) ) ) {
			p->connect.ssl = NULL;
			zclose( newsockfd );
			oncomplete( -1, cli_addr, cli_len, udata );
			puts( "error: accepting: 2" );
			return;
		}
		SSL_set_fd( p->connect.ssl, newsockfd );
	}

	zsock_a_data_t* a = malloc( sizeof( zsock_a_data_t ) );
	a->cli_addr = cli_addr;
	a->cli_len = cli_len;
	a->oncomplete = oncomplete;
	a->udata = udata;

	zaccepter( newsockfd, a );
}

void zaccepter( int fd, zsock_a_data_t* a ) {
	zsocket_t* zs = zsocket_get_by_sockfd( fd );
	if ( zfd_type_isset( fd, ZACCEPT_R ) ) 
		zfd_clr( fd, ZACCEPT_R );
	if ( zfd_type_isset( fd, ZACCEPT_W ) )
		zfd_clr( fd, ZACCEPT_W );
	int n;
	if ( zs->connect.ssl ) {
		if ( ( n = SSL_accept( zs->connect.ssl ) ) == -1 ) {
			int err = SSL_get_error( zs->connect.ssl, n );
			printf( "%d err\n", err );
			switch ( err ) {
				case SSL_ERROR_WANT_READ:
					zfd_set( fd, ZACCEPT_R, a );
					puts( "zaccepter: SSL WANT READ" );
					return;
				case SSL_ERROR_WANT_WRITE:
					zfd_set( fd, ZACCEPT_W, a );
					puts( "zaccepter: SSL WANT WRITE" );
					return;
			}

			ERR_print_errors_fp( stderr );
			zclose( fd );
			a->oncomplete( -1, a->cli_addr, a->cli_len, a->udata );
			puts( "error: accepting: 3" );
			return;
		}
	}

	a->oncomplete( fd, a->cli_addr, a->cli_len, a->udata );
	free( a );
}

void zread( int fd, char* buffer, size_t buffer_size, void (*oncomplete)( int, int, void*, size_t, void* ), void* udata ) {
	zsock_rw_data_t* rw = malloc( sizeof( zsock_rw_data_t ) );
	rw->buffer = buffer;
	rw->buffer_size = buffer_size;
	rw->buffer_used = 0;
	rw->oncomplete = oncomplete;
	rw->udata = udata;
	rw->fd_info_set = false;

	if ( zfd_io_isset( fd, ZFD_R ) ) {
		rw->fd_info_set = true;
		rw->fd_info = zfd_info( fd, ZFD_R );
		zfd_clr( fd, rw->fd_info.type );
	}

	zreader( fd, rw );
}

void zreader( int fd, zsock_rw_data_t* rw ) {
	zsocket_t* zs = zsocket_get_by_sockfd( fd );
	int n;
	if ( zs->connect.ssl ) {
		n = SSL_read( zs->connect.ssl, rw->buffer + rw->buffer_used, rw->buffer_size - rw->buffer_used );
		if ( n == -1 ) {
			int err = SSL_get_error( zs->connect.ssl, n );
			switch ( err ) {
				case SSL_ERROR_WANT_READ:
					break;
				case SSL_ERROR_WANT_WRITE:
					puts( "SSL WANT" );
					break;
				default:
					ERR_print_errors_fp( stderr );
			}
		}
	} else {
		n = read( fd, rw->buffer + rw->buffer_used, rw->buffer_size - rw->buffer_used );
		if ( n == -1 && errno == EAGAIN ) {
			//puts( "EAGAIN: zreader" );
			zfd_set( fd, ZREAD, rw );
			return;
		}
		rw->buffer_used += n;
	}

	if ( zfd_type_isset( fd, ZREAD ) )
		zfd_clr( fd, ZREAD );
	if ( rw->fd_info_set )
		zfd_set( fd,rw->fd_info.type, rw->fd_info.udata );
	rw->oncomplete( fd, n, rw->buffer, rw->buffer_size, rw->udata );
	free( rw );
}

void zwrite( int fd, char* buffer, size_t buffer_size, void (*oncomplete)( int, int, void*, size_t, void* ), void* udata ) {
	zsock_rw_data_t* rw = malloc( sizeof( zsock_rw_data_t ) );
	rw->buffer = buffer;
	rw->buffer_size = buffer_size;
	rw->buffer_used = 0;
	rw->oncomplete = oncomplete;
	rw->udata = udata;
	rw->fd_info_set = false;

	if ( zfd_io_isset( fd, ZFD_W ) ) {
		rw->fd_info_set = true;
		rw->fd_info = zfd_info( fd, ZFD_W );
		zfd_clr( fd, rw->fd_info.type );
	}

	zwriter( fd, rw );
}

void zwriter( int fd, zsock_rw_data_t* rw ) {
	zsocket_t* zs = zsocket_get_by_sockfd( fd );
	int n;

	if ( zs->connect.ssl ) {
		n = SSL_write( zs->connect.ssl, rw->buffer + rw->buffer_used, rw->buffer_size - rw->buffer_used );
		if ( n == -1 ) {
			int err = SSL_get_error( zs->connect.ssl, n );
			switch ( err ) {
				//case SSL_ERROR_NONE:
				case SSL_ERROR_WANT_READ:
				case SSL_ERROR_WANT_WRITE:
					puts( "zwrite SSL WANT" );
					break;
				default:
					ERR_print_errors_fp( stderr );
			}
		}
	} else {
		n = write( fd, rw->buffer + rw->buffer_used, rw->buffer_size - rw->buffer_used );
		if ( n == -1 && errno == EAGAIN ) {
			puts( "EAGAIN: zwriter" );
			zfd_set( fd, ZWRITE, rw );
			return;
		}
		rw->buffer_used += n;
		if ( n > 0 && rw->buffer_used < rw->buffer_size ) {
			puts( "not full write: zwriter" );
			zfd_set( fd, ZWRITE, rw );
			return;
		}
	}

	if ( zfd_type_isset( fd, ZWRITE ) )
		zfd_clr( fd, ZWRITE );
	if ( rw->fd_info_set )
		zfd_set( fd, rw->fd_info.type, rw->fd_info.udata );
	rw->oncomplete( fd, n, rw->buffer, rw->buffer_size, rw->udata );
	free( rw );
}
