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

/*  ssl api reference: http://h71000.www7.hp.com/doc/83final/ba554_90007/ch05s04.html
 *  ssl ca reference: https://help.ubuntu.com/community/OpenSSL
 */

#include "zsocket.h"

static bool initialized = false;
static SSL_CTX* client_ctx;

static void zsocket_fire_event( int fd, unsigned int type, ... );
static int zsocket_new( struct sockaddr_in* addr, int portno, int type );

static void _zaccept( int fd, void* udata );
static void _zwrite(  int fd, void* udata );
static void _zread(   int fd, void* udata );

static void _zaccepter( int fd, void* udata );
static void _zwriter(   int fd, void* udata );
static void _zreader(   int fd, void* udata );

void zsocket_init() {
	assert( !initialized );
	initialized = true;

	SSL_library_init();
	SSL_load_error_strings();

	client_ctx = SSL_CTX_new( SSLv23_client_method() );
	if ( client_ctx == NULL ) {
		ERR_print_errors_fp(stderr);
		abort();
	}

	memset( zsockets, 0, sizeof( zsockets ) );
}

static zsocket_t* _zsocket_type_init( int fd, int type, struct sockaddr_in* addr, int portno,
  void (*zsocket_event_hdlr)( int fd, zsocket_event_t event ), SSL_CTX* ssl_ctx ) {
	zsocket_t* p;
	p = (zsocket_t* ) malloc( sizeof( zsocket_t ) );
	p->type = type;

	p->general.sockfd = fd;
	p->general.addr = *addr;
	p->general.portno = portno;
	p->general.flags = 0;
	p->general.event_hdlr = zsocket_event_hdlr;
	p->general.udata = NULL;

	if ( p->type == ZSOCK_LISTEN ) {
		p->listen.n_open = 0;
		p->listen.ssl_ctx = NULL;
	} else {
		p->connect.ssl = NULL;
		p->connect.read.buffer = malloc( 2048 );
		p->connect.read.buffer_size = 2048;
		p->connect.read.buffer_used = 0;
		p->connect.write.buffer = NULL;
	}

	if ( ssl_ctx ) {
		if ( type == ZSOCK_LISTEN ) {
			p->listen.ssl_ctx = ssl_ctx;
			// Load the server certificate into the SSL_CTX structure
			// Load the private-key corresponding to the server certificate
			if ( !p->listen.ssl_ctx
			  || SSL_CTX_use_certificate_chain_file( p->listen.ssl_ctx, RSA_SERVER_CERT ) <= 0
			  || SSL_CTX_use_PrivateKey_file( p->listen.ssl_ctx, RSA_SERVER_KEY, SSL_FILETYPE_PEM ) <= 0 ) {
				ERR_print_errors_fp( stderr );
				free( p );
				return NULL;
			}

			// Check if the server certificate and private-key matches
			if ( !SSL_CTX_check_private_key( p->listen.ssl_ctx ) ) {
				fprintf( stderr, "Private key does not match the certificate public key\n" );
				free( p );
				return NULL;
			}

			SSL_CTX_set_mode( p->listen.ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE );
		} else {
			p->connect.ssl = SSL_new( ssl_ctx );
			//BIO *sbio = NULL;
			//sbio = BIO_new_socket( sockfd, BIO_NOCLOSE );
			//SSL_set_bio( p->connect.ssl, sbio, sbio );
			SSL_set_fd( p->connect.ssl, fd );
		}
	}

	if ( type == ZSOCK_LISTEN )
		p->listen.n_open++;

	zsockets[ fd ] = p;

	return p;
}

int zsocket( in_addr_t addr, int portno, int type, void (*zsocket_event_hdlr)( int fd, zsocket_event_t event ), bool ssl ) {
	assert( initialized );
	zsocket_t* p;

	if ( type == ZSOCK_LISTEN ) {
		p = zsocket_get_by_info( addr, portno );
		if ( p ) {
			p->listen.n_open++;
			return p->listen.sockfd;
		}
	}

	struct sockaddr_in serv_addr;
	memset( &serv_addr, 0, sizeof( serv_addr ) );

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = addr;
	serv_addr.sin_port = htons( portno );

	int sockfd = zsocket_new( &serv_addr, portno, type );
	if ( sockfd == -1 )
		return -1;

	SSL_CTX* ssl_ctx = NULL;
	if ( ssl ) {
		if ( type == ZSOCK_LISTEN )
			ssl_ctx = SSL_CTX_new( SSLv23_server_method() );
		else
			ssl_ctx = client_ctx;
	}

	p = _zsocket_type_init( sockfd, type, &serv_addr, portno, zsocket_event_hdlr, ssl_ctx );
	if ( p && type == ZSOCK_CONNECT ) {
		FL_SET( p->general.flags, ZCONNECTED );
		if ( ssl ) SSL_set_connect_state( p->connect.ssl );
	}
	return p ? sockfd : -1;
}

static int zsocket_new( struct sockaddr_in* addr, int portno, int type ) {
	assert( initialized );
	int sockfd = socket( AF_INET, SOCK_STREAM, 0 );
	int on = 1; // used by setsockopt

	if ( sockfd < 0 ) {
		return -1;
	}

	if ( type == ZSOCK_LISTEN ) {
		setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof( on ) );

		if ( bind( sockfd, (struct sockaddr*) addr, sizeof( struct sockaddr_in ) ) < 0 ) {
			close( sockfd );
			return -1;
		}

		if ( listen( sockfd, ZSOCK_N_PENDING ) < 0 ) {
			return -1;
		}
	} else if ( type == ZSOCK_CONNECT ) {
		if ( connect( sockfd, (struct sockaddr*) addr, sizeof( struct sockaddr_in ) ) < 0 ) {
			return -1;
		}
	} else {
		close( sockfd );
		return -1;
	}

	fcntl( sockfd, F_SETFL, fcntl( sockfd, F_GETFL, 0 ) | O_NONBLOCK );
	return sockfd;
}

bool zsocket_is_ssl( int fd ) {
	zsocket_t* p = zsockets[ fd ];

	if ( p->type == ZSOCK_LISTEN )
		return (bool) p->listen.ssl_ctx;
	else if ( p->type == ZSOCK_CONNECT )
		return (bool) p->connect.ssl;
	else abort();
}

zsocket_t* zsocket_get_by_info( in_addr_t addr, int portno ) {
	assert( initialized );

	int i;
	for ( i = 0; i < sizeof( zsockets ) / sizeof( zsockets[ 0 ] ); i++ ) {
		zsocket_t* p = zsockets[ i ];
		if ( p ) {
			if ( p->type == ZSOCK_LISTEN && p->listen.portno == portno && p->listen.addr.sin_addr.s_addr == addr )
				return p;
			else if ( p->type == ZSOCK_CONNECT && p->connect.portno == portno && p->connect.addr.sin_addr.s_addr == addr )
				return p;
		}
	}

	return NULL;
}

zsocket_t* zsocket_get_by_sockfd( int sockfd ) {
	assert( initialized );

	return zsockets[ sockfd ];
}

void zsocket_set_event_hdlr( int fd,  void (*zsocket_event_hdlr)( int fd, zsocket_event_t event ) ) {
	zsocket_t* zs;
	assert( zs = zsocket_get_by_sockfd( fd ) );
	zs->general.event_hdlr = zsocket_event_hdlr;
}

void zclose( int fd ) {
	assert( initialized );
	zsocket_t* p = zsockets[ fd ];

	if ( p->type == ZSOCK_LISTEN ) {
		p->listen.n_open--;

		if ( p->listen.n_open > 0 )
			return;
	}

	zsockets[ fd ] = NULL;

	if ( p->type == ZSOCK_LISTEN ) {
		if ( p->listen.ssl_ctx )
			SSL_CTX_free( p->listen.ssl_ctx );
		close( p->listen.sockfd );
	} else if ( p->type == ZSOCK_CONNECT ) {
		if ( p->connect.ssl )
			SSL_shutdown( p->connect.ssl );
		close( p->connect.sockfd );
		free( p->connect.read.buffer );
		if ( p->connect.write.buffer )
			free( p->connect.write.buffer );
	}

	zfd_clr( fd, ZFD_R );
	zfd_clr( fd, ZFD_W );

	if ( p->type == ZSOCK_CONNECT && p->connect.ssl )
		SSL_free( p->connect.ssl );

	free( p );
}

static void _zsocket_update_fd_state( int fd, void* udata ) {
	zsocket_t* zs;
	if ( !( zs = zsocket_get_by_sockfd( fd ) ) ) return;
	assert( zs->type == ZSOCK_CONNECT );

	if ( !FL_ISSET( zs->connect.flags, ZPAUSE ) && ( FL_ISSET( zs->connect.flags, ZWRITE_FROM_READ ) || FL_ISSET( zs->connect.flags, ZWRITE_FROM_WRITE )
	|| FL_ISSET( zs->connect.flags, ZWRITE_FROM_ACCEPT ) ) )
		zfd_set( fd, ZFD_W, &_zwrite, udata );
	else
		zfd_clr( fd, ZFD_W );

	if ( FL_ISSET( zs->connect.flags, ZREAD_READ ) && !FL_ISSET( zs->connect.flags, ZREAD_FROM_WRITE ) )
		FL_SET( zs->connect.flags, ZREAD_FROM_READ );

	if ( !FL_ISSET( zs->connect.flags, ZPAUSE ) && ( FL_ISSET( zs->connect.flags, ZREAD_FROM_READ ) || FL_ISSET( zs->connect.flags, ZREAD_FROM_WRITE )
	|| FL_ISSET( zs->connect.flags, ZREAD_FROM_ACCEPT ) ) )
		zfd_set( fd, ZFD_R, &_zread, udata );
	else
		zfd_clr( fd, ZFD_R );
}

static void _zread( int fd, void* udata ) {
	zsocket_t* zs;
	if ( !( zs = zsocket_get_by_sockfd( fd ) ) ) return;
	assert( zs->type == ZSOCK_CONNECT );

	if ( FL_ISSET( zs->connect.flags, ZCONNECTED ) ) {

		if ( FL_ISSET( zs->connect.flags, ZREAD_FROM_READ ) ) {
			// clear write_from_write, if read_from_write is not set clear fd select flag
			FL_CLR( zs->connect.flags, ZREAD_FROM_READ );
			_zreader( fd, udata );
		}

		if ( FL_ISSET( zs->connect.flags, ZREAD_FROM_WRITE ) ) {
			// clear write_from_read, if read_from_read is not set and read is not set clear fd select flag
			FL_CLR( zs->connect.flags, ZREAD_FROM_WRITE );
			_zwriter( fd, udata );
		}

	} else {
		FL_CLR( zs->connect.flags, ZREAD_FROM_ACCEPT );
		_zaccepter( fd, udata );
	}

	_zsocket_update_fd_state( fd, udata );
}

static void _zwrite( int fd, void* udata ) {
	zsocket_t* zs;
	if ( !( zs = zsocket_get_by_sockfd( fd ) ) ) return;
	assert( zs->type == ZSOCK_CONNECT );

	if ( FL_ISSET( zs->connect.flags, ZCONNECTED ) ) {

		if ( FL_ISSET( zs->connect.flags, ZWRITE_FROM_WRITE ) ) {
			// clear write_from_write, if read_from_write is not set clear fd select flag
			FL_CLR( zs->connect.flags, ZWRITE_FROM_WRITE );
			_zwriter( fd, udata );
		}

		if ( FL_ISSET( zs->connect.flags, ZWRITE_FROM_READ ) ) {
			// clear write_from_read, if read_from_read is not set and read is not set clear fd select flag
			FL_CLR( zs->connect.flags, ZWRITE_FROM_READ );
			_zreader( fd, udata );
		}

	} else {
		FL_CLR( zs->connect.flags, ZWRITE_FROM_ACCEPT );
		_zaccepter( fd, udata );
	}

	_zsocket_update_fd_state( fd, udata );
}

void zaccept( int fd, bool toggle ) {
	zsocket_t* zs;
	assert( zs = zsocket_get_by_sockfd( fd ) );
	assert( zs->type == ZSOCK_LISTEN );

	if ( toggle )
		zfd_set( fd, ZFD_R, &_zaccept, NULL );
	else
		zfd_clr( fd, ZFD_R );
}

static void _zaccept( int fd, void* udata ) {
	int newsockfd;
	struct sockaddr_in cli_addr;
	socklen_t cli_len = sizeof( struct sockaddr_in );

	zsocket_t* zs = zsocket_get_by_sockfd( fd );
	// Connection request on original socket.
	if ( ( newsockfd = accept( fd, (struct sockaddr*) &cli_addr, &cli_len ) ) == -1 ) {
		perror( "error: accepting: 1" );
		zsocket_fire_event( fd, ZSE_ACCEPT_ERR, newsockfd );
		return;
	}

	fcntl( newsockfd, F_SETFL, fcntl( newsockfd, F_GETFL, 0 ) | O_NONBLOCK );

	if ( !_zsocket_type_init( newsockfd, ZSOCK_CONNECT, &cli_addr,
	  zs->connect.portno, zs->connect.event_hdlr, zs->listen.ssl_ctx ) ) {
		zsocket_fire_event( fd, ZSE_ACCEPT_ERR, -1 );
		fprintf( stderr, "error: accepting: 2\n" );
		return;
	}

	int* origfd_ptr = memdup( &fd, sizeof( fd ) );
	_zaccepter( newsockfd, origfd_ptr );
	_zsocket_update_fd_state( newsockfd, origfd_ptr );
}

static void _zaccepter( int fd, void* udata ) {
	zsocket_t* zs = zsocket_get_by_sockfd( fd );
	int origfd = *(int*) udata;
	int n;
	if ( zs->connect.ssl ) {
		n = SSL_accept( zs->connect.ssl );
		if ( n < 0 ) {
			int err = SSL_get_error( zs->connect.ssl, n );
			switch ( err ) {
				case SSL_ERROR_WANT_READ:
					FL_SET( zs->connect.flags, ZREAD_FROM_ACCEPT );
					return;
				case SSL_ERROR_WANT_WRITE:
					FL_SET( zs->connect.flags, ZWRITE_FROM_ACCEPT );
					return;
			}

			ERR_print_errors_fp( stderr );
			zclose( fd );
			fd = -1;
		}
	}


	if ( fd == -1 )
		zsocket_fire_event( origfd, ZSE_ACCEPT_ERR, fd );
	else {
		FL_SET( zs->connect.flags, ZCONNECTED );
		zsocket_fire_event( origfd, ZSE_ACCEPTED_CONNECTION, fd, zs->connect.addr );
	}

	free( udata );
}

void zpause( int fd, bool toggle ) {
	zsocket_t* zs;
	assert( zs = zsocket_get_by_sockfd( fd ) );
	assert( zs->type == ZSOCK_CONNECT );

	if ( toggle )
		FL_SET( zs->connect.flags, ZPAUSE );
	else
		FL_CLR( zs->connect.flags, ZPAUSE );

	_zsocket_update_fd_state( fd, NULL );
}

void zread( int fd, bool toggle ) {
	zsocket_t* zs;
	assert( zs = zsocket_get_by_sockfd( fd ) );
	assert( zs->type == ZSOCK_CONNECT );

	if ( toggle )
		FL_SET( zs->connect.flags, ZREAD_READ );
	else
		FL_CLR( zs->connect.flags, ZREAD_READ );

	_zsocket_update_fd_state( fd, NULL );
}

static void _zreader( int fd, void* udata ) {
	zsocket_t* zs = zsocket_get_by_sockfd( fd );

	ssize_t n;
	if ( zs->connect.ssl ) {
		n = SSL_read( zs->connect.ssl, zs->connect.read.buffer + zs->connect.read.buffer_used,
		  zs->connect.read.buffer_size - zs->connect.read.buffer_used );

		if ( n < 0 ) {
			int err = SSL_get_error( zs->connect.ssl, n );
			switch ( err ) {
				case SSL_ERROR_WANT_READ:
					FL_SET( zs->connect.flags, ZREAD_FROM_READ );
					return;
				case SSL_ERROR_WANT_WRITE:
					FL_SET( zs->connect.flags, ZREAD_FROM_WRITE );
					return;
			}
			ERR_print_errors_fp( stderr );
		}
	} else {
		n = read( fd, zs->connect.read.buffer + zs->connect.read.buffer_used,
		  zs->connect.read.buffer_size - zs->connect.read.buffer_used );

		if ( n == -1 && errno == EAGAIN ) {
			//puts( "EAGAIN: zreader" );
			FL_SET( zs->connect.flags, ZREAD_FROM_READ );
			return;
		}
	}

	if ( n > 0 )
		zs->connect.read.buffer_used += n;

	//if ( rw->fd_info_set )
	//	zfd_set( fd,rw->fd_info.type, rw->fd_info.udata );

	zsocket_fire_event( fd, ZSE_READ_DATA,  n <= 0 ? n : zs->connect.read.buffer_used, zs->connect.read.buffer, zs->connect.read.buffer_size );
	zs->connect.read.buffer_used = 0;
}

void zwrite( int fd, const void* buffer, ssize_t buffer_size ) {
	zsocket_t* zs;
	assert( zs = zsocket_get_by_sockfd( fd ) );
	assert( zs->type == ZSOCK_CONNECT );
	assert( !zs->connect.write.buffer );

	zs->connect.write.buffer = malloc( buffer_size );
	memcpy( zs->connect.write.buffer, buffer, buffer_size );
	zs->connect.write.buffer_size = buffer_size;
	zs->connect.write.buffer_used = 0;
	FL_SET( zs->connect.flags, ZWRITE_FROM_WRITE );

	_zwrite( fd, NULL );
}

static void _zwriter( int fd, void* udata ) {
	zsocket_t* zs = zsocket_get_by_sockfd( fd );

	ssize_t n;
	if ( zs->connect.ssl ) {
		n = SSL_write( zs->connect.ssl, zs->connect.write.buffer + zs->connect.write.buffer_used,
		  zs->connect.write.buffer_size - zs->connect.write.buffer_used );

		if ( n < 0 ) {
			int err = SSL_get_error( zs->connect.ssl, n );
			switch ( err ) {
				case SSL_ERROR_WANT_READ:
					FL_SET( zs->connect.flags, ZREAD_FROM_WRITE );
					return;
				case SSL_ERROR_WANT_WRITE:
					FL_SET( zs->connect.flags, ZWRITE_FROM_WRITE );
					return;
			}
			ERR_print_errors_fp( stderr );
		}
	} else {
		n = write( fd, zs->connect.write.buffer + zs->connect.write.buffer_used,
		  zs->connect.write.buffer_size - zs->connect.write.buffer_used );

		if ( n == -1 && errno == EAGAIN ) {
			FL_SET( zs->connect.flags, ZWRITE_FROM_WRITE );
			return;
		}
	}

	if ( n > 0 ) {
		zs->connect.write.buffer_used += n;
		if ( zs->connect.write.buffer_used < zs->connect.write.buffer_size ) {
			FL_SET( zs->connect.flags, ZWRITE_FROM_WRITE );
			return;
		}
	}

	char* buf_ptr = zs->connect.write.buffer;
	zs->connect.write.buffer = NULL;

	zsocket_fire_event( fd, ZSE_WROTE_DATA, n <= 0 ? n : zs->connect.write.buffer_used, buf_ptr, zs->connect.write.buffer_size );
	free( buf_ptr );

}

static void zsocket_fire_event( int fd, unsigned int type, ... ) {
	zsocket_t* zs;
	assert( zs = zsocket_get_by_sockfd( fd ) );
	if ( !zs->general.event_hdlr ) return;

	va_list ap;

	zsocket_event_t event;
	memset( &event, 0, sizeof( event ) );
	event.type = type;

	va_start( ap, type );
	switch ( event.type ) {
		case ZSE_ACCEPT_ERR:
			event.data.conn.fd = va_arg( ap, int );
			break;
		case ZSE_ACCEPTED_CONNECTION:
			event.data.conn.fd = va_arg( ap, int );
			event.data.conn.addr = va_arg( ap, struct sockaddr_in );
			break;
		case ZSE_READ_DATA:
			event.data.read.buffer_used = va_arg( ap, ssize_t );
			event.data.read.buffer = va_arg( ap, char* );
			event.data.read.buffer_size = va_arg( ap, size_t );
			break;
		case ZSE_WROTE_DATA:
			event.data.write.buffer_used = va_arg( ap, ssize_t );
			event.data.write.buffer = va_arg( ap, char* );
			event.data.write.buffer_size = va_arg( ap, size_t );
			break;
		default:
			fprintf( stderr, "zsocket_fire_event: passed undefined event %d\n", event.type );
			goto quit;
	}

	zs->general.event_hdlr( fd, event );

quit:
	va_end( ap );
}
