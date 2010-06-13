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

static int n_selectable = 0;

static fd_set active_read_fd_set;
static fd_set active_write_fd_set;

static bool initialized = false;
static SSL_CTX* client_ctx;

//static void zsocket_fire_event( int fd, unsigned int type, ... );
static int zs_new( struct sockaddr_in* addr, int portno, int type );
static void zs_update_fd_state( int fd, void* udata );
static void zs_connecter( int fd, void* udata );

void zs_init() {
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
	FD_ZERO( &active_read_fd_set );
	FD_ZERO( &active_write_fd_set );
}

static zsocket_t* zs_type_init( int fd, int type, struct sockaddr_in* addr, int portno,
  void (*zs_event_hdlr)( int fd, int event ), SSL_CTX* ssl_ctx ) {
	zsocket_t* p;
	p = (zsocket_t* ) malloc( sizeof( zsocket_t ) );
	p->type = type;

	p->general.sockfd = fd;
	p->general.addr = *addr;
	p->general.portno = portno;
	p->general.status = 0;
	p->general.event_hdlr = zs_event_hdlr;
	p->general.udata = NULL;

	if ( p->type == ZSOCK_LISTEN ) {
		p->listen.accepted_fd = -1;
		p->listen.accepted_errno = 0;
		p->listen.n_open = 0;
		p->listen.ssl_ctx = NULL;
	} else {
		p->connect.ssl = NULL;
		p->connect.read.buffer = malloc( 2048 );
		p->connect.read.size = 2048;
		p->connect.read.used = 0;
		p->connect.read.pos = 0;
		p->connect.read.rw_errno = 0;
		p->connect.write.buffer = NULL;
		p->connect.write.rw_errno = 0;
		p->connect.parent_fd = -1;
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

int zsocket( in_addr_t addr, int portno, int type, void (*zs_event_hdlr)( int fd, int event ), bool ssl ) {
	assert( initialized );
	zsocket_t* p;

	if ( type == ZSOCK_LISTEN ) {
		p = zs_get_by_info( addr, portno );
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

	int sockfd = zs_new( &serv_addr, portno, type );
	if ( sockfd == -1 )
		return -1;

	SSL_CTX* ssl_ctx = NULL;
	if ( ssl ) {
		if ( type == ZSOCK_LISTEN )
			ssl_ctx = SSL_CTX_new( SSLv23_server_method() );
		else
			ssl_ctx = client_ctx;
	}

	p = zs_type_init( sockfd, type, &serv_addr, portno, zs_event_hdlr, ssl_ctx );
	if ( p && type == ZSOCK_CONNECT ) {
		FL_SET( p->general.status, ZS_STAT_CONNECTING );
		if ( ssl ) SSL_set_connect_state( p->connect.ssl );
		zs_connecter( sockfd, NULL );
		zs_update_fd_state( sockfd, NULL );
	}
	return p ? sockfd : -1;
}

static int zs_new( struct sockaddr_in* addr, int portno, int type ) {
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

bool zs_is_ssl( int fd ) {
	zsocket_t* p = zsockets[ fd ];

	if ( p->type == ZSOCK_LISTEN )
		return (bool) p->listen.ssl_ctx;
	else if ( p->type == ZSOCK_CONNECT )
		return (bool) p->connect.ssl;
	else abort();
}

zsocket_t* zs_get_by_info( in_addr_t addr, int portno ) {
	assert( initialized );

	int i;
	for ( i = 0; i < sizeof( zsockets ) / sizeof( zsockets[0] ); i++ ) {
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

zsocket_t* zs_get_by_fd( int fd ) {
	assert( initialized );

	return zsockets[ fd ];
}

void zs_set_event_hdlr( int fd, void (*zs_event_hdlr)( int fd, int event ) ) {
	zsocket_t* zs;
	assert( zs = zs_get_by_fd( fd ) );
	zs->general.event_hdlr = zs_event_hdlr;
}
////////////////////////////////////////////////////////////////////////////

static void zs_accepter( int fd, void* udata ) {
	zsocket_t* zs = zs_get_by_fd( fd );
	zsocket_t* zs_parent = zs_get_by_fd( zs->connect.parent_fd );

	ssize_t n;
	if ( zs->connect.ssl ) {
		n = SSL_accept( zs->connect.ssl );
		if ( n < 0 ) {
			int err = SSL_get_error( zs->connect.ssl, n );
			switch ( err ) {
				case SSL_ERROR_WANT_READ:
					FL_SET( zs->connect.status, ZS_STAT_WANT_READ_FROM_ACCEPT );
					return;
				case SSL_ERROR_WANT_WRITE:
					FL_SET( zs->connect.status, ZS_STAT_WANT_WRITE_FROM_ACCEPT );
					return;
			}

			ERR_print_errors_fp( stderr );
			zs_close( fd );

			zs_parent->listen.accepted_fd = -1;
			zs_parent->listen.accepted_errno = EPROTO;
		}
	}

	if ( FD_ISSET( zs_parent->general.sockfd, &active_read_fd_set ) && !FL_ISSET( zs_parent->general.status, ZS_STAT_READABLE ) )
		n_selectable++;

	FL_SET( zs_parent->general.status, ZS_STAT_READABLE );
	FL_CLR( zs_parent->general.status, ZS_STAT_ACCEPTING );

	if ( zs_parent->listen.accepted_fd != -1 ) {
		if ( FD_ISSET( fd, &active_write_fd_set ) && !FL_ISSET( zs->general.status, ZS_STAT_WRITABLE ) )
			n_selectable++;
		FL_SET( zs->connect.status, ZS_STAT_CONNECTED | ZS_STAT_WRITABLE );
	}

	// Must call this inside zs_accepter because the
	// parent zsocket's state update may not be called by
	// the caller on return from calling zs_accepter,
	// it may only call it for the child
	zs_update_fd_state( zs_parent->general.sockfd, NULL );
}

static void zs_connecter( int fd, void* udata ) {
	zsocket_t* zs = zs_get_by_fd( fd );

	ssize_t n;
	if ( zs->connect.ssl ) {
		n = SSL_connect( zs->connect.ssl );
		if ( n < 0 ) {
			int err = SSL_get_error( zs->connect.ssl, n );
			switch ( err ) {
				case SSL_ERROR_WANT_READ:
					FL_SET( zs->connect.status, ZS_STAT_WANT_READ_FROM_CONNECT );
					return;
				case SSL_ERROR_WANT_WRITE:
					FL_SET( zs->connect.status, ZS_STAT_WANT_WRITE_FROM_CONNECT );
					return;
			}

			ERR_print_errors_fp( stderr );

			zs->connect.read.rw_errno = EPROTO;
			zs->connect.write.rw_errno = EPROTO;
		}
	}

	if ( FD_ISSET( fd, &active_write_fd_set ) && !FL_ISSET( zs->general.status, ZS_STAT_WRITABLE ) )
		n_selectable++;

	FL_SET( zs->connect.status, ZS_STAT_CONNECTED | ZS_STAT_WRITABLE );
	FL_CLR( zs->general.status, ZS_STAT_CONNECTING );
}

static void zs_writer( int fd, void* udata ) {
	zsocket_t* zs = zs_get_by_fd( fd );

	ssize_t n;
	if ( zs->connect.ssl ) {
		n = SSL_write( zs->connect.ssl, zs->connect.write.buffer + zs->connect.write.pos,
		  zs->connect.write.used - zs->connect.write.pos );
		//fprintf( stderr, "%d of %d zs_writer\n", n, zs->connect.write.used - zs->connect.write.pos );

		if ( n < 0 ) {
			int err = SSL_get_error( zs->connect.ssl, n );
			switch ( err ) {
				case SSL_ERROR_WANT_READ:
					FL_SET( zs->connect.status, ZS_STAT_WANT_READ_FROM_WRITE );
					return;
				case SSL_ERROR_WANT_WRITE:
					FL_SET( zs->connect.status, ZS_STAT_WANT_WRITE_FROM_WRITE );
					return;
			}
			ERR_print_errors_fp( stderr );

			zs->connect.write.rw_errno = EPROTO;
		}
	} else {
		n = write( fd, zs->connect.write.buffer + zs->connect.write.pos,
		  zs->connect.write.used - zs->connect.write.pos );
		//fprintf( stderr, "%d of %d zs_writer\n", n, zs->connect.write.used - zs->connect.write.pos );

		if ( n == -1 ) {
			if ( errno == EAGAIN ) {
				FL_SET( zs->connect.status, ZS_STAT_WANT_WRITE_FROM_WRITE );
				return;
			}

			zs->connect.write.rw_errno = errno;
		}
	}

	if ( n > 0 ) {
		zs->connect.write.pos += n;
		if ( zs->connect.write.used - zs->connect.write.pos ) {
			FL_SET( zs->connect.status, ZS_STAT_WANT_WRITE_FROM_WRITE );
			return;
		}
	}

	free( zs->connect.write.buffer );
	zs->connect.write.buffer = NULL;

	if ( FD_ISSET( fd, &active_write_fd_set ) && !FL_ISSET( zs->general.status, ZS_STAT_WRITABLE ) )
		n_selectable++;

	FL_SET( zs->general.status, ZS_STAT_WRITABLE );
	FL_CLR( zs->general.status, ZS_STAT_WRITING );
	if ( FL_ISSET( zs->general.status, ZS_STAT_CLOSED ) )
		zs_close( fd );
}

static void zs_reader( int fd, void* udata ) {
	zsocket_t* zs = zs_get_by_fd( fd );

	ssize_t n;
	if ( zs->connect.ssl ) {
		n = SSL_read( zs->connect.ssl, zs->connect.read.buffer + zs->connect.read.used,
		  zs->connect.read.size - zs->connect.read.used );
		//fprintf( stderr, "%d of %d zs_reader\n", n, zs->connect.read.size - zs->connect.read.used );

		if ( n < 0 ) {
			int err = SSL_get_error( zs->connect.ssl, n );
			switch ( err ) {
				case SSL_ERROR_WANT_READ:
					FL_SET( zs->connect.status, ZS_STAT_WANT_READ_FROM_READ );
					return;
				case SSL_ERROR_WANT_WRITE:
					FL_SET( zs->connect.status, ZS_STAT_WANT_READ_FROM_WRITE );
					return;
			}
			ERR_print_errors_fp( stderr );

			zs->connect.read.rw_errno = EPROTO;
		}

	} else {
		n = read( fd, zs->connect.read.buffer + zs->connect.read.used,
		  zs->connect.read.size - zs->connect.read.used );
		//fprintf( stderr, "%d of %d zs_reader\n", n, zs->connect.read.size - zs->connect.read.used );
		if ( n == -1 ) {
			if ( errno == EAGAIN ) {
				//puts( "EAGAIN: zreader" );
				FL_SET( zs->connect.status, ZS_STAT_WANT_READ_FROM_READ );
				return;
			}

			zs->connect.read.rw_errno = errno;
		}
	}

	if ( n > 0 )
		zs->connect.read.used += n;
	else if ( n == -1 )
		zs->connect.read.used = -1;
	else
		FL_SET( zs->general.status, ZS_STAT_EOF );

	if ( FD_ISSET( fd, &active_read_fd_set ) && !FL_ISSET( zs->general.status, ZS_STAT_READABLE ) )
		n_selectable++;

	FL_SET( zs->general.status, ZS_STAT_READABLE );
	FL_CLR( zs->general.status, ZS_STAT_READING );
}

static void zs_internal_accept( int fd, void* udata ) {
	int newsockfd;
	struct sockaddr_in cli_addr;
	socklen_t cli_len = sizeof( struct sockaddr_in );

	zsocket_t* zs = zs_get_by_fd( fd );

	FL_SET( zs->general.status, ZS_STAT_ACCEPTING );

	// Connection request on original socket.
	if ( ( newsockfd = accept( fd, (struct sockaddr*) &cli_addr, &cli_len ) ) == -1 ) {
		zs->listen.accepted_fd = -1;
		zs->listen.accepted_errno = errno;
		FL_CLR( zs->general.status, ZS_STAT_ACCEPTING );
		zs_update_fd_state( fd, NULL );
		return;
	}

	fcntl( newsockfd, F_SETFL, fcntl( newsockfd, F_GETFL, 0 ) | O_NONBLOCK );

	if ( !zs_type_init( newsockfd, ZSOCK_CONNECT, &cli_addr,
	  zs->general.portno, zs->general.event_hdlr, zs->listen.ssl_ctx ) ) {
		zs->listen.accepted_fd = -1;
		zs->listen.accepted_errno = EPROTO;
		FL_CLR( zs->general.status, ZS_STAT_ACCEPTING );
		zs_update_fd_state( fd, NULL );
		return;
	}

	zsocket_t* zs_child = zs_get_by_fd( newsockfd );
	zs->listen.accepted_fd = newsockfd;
	zs_child->connect.parent_fd = fd;

	zs_accepter( newsockfd, NULL );
	if ( zs_get_by_fd( newsockfd ) ) zs_update_fd_state( newsockfd, NULL );
	zs_update_fd_state( fd, NULL );
}

static void zs_internal_write( int fd, void* udata ) {
	zsocket_t* zs = zs_get_by_fd( fd );
	assert( zs->type == ZSOCK_CONNECT );

	if ( FL_ISSET( zs->connect.status, ZS_STAT_CONNECTED ) ) {

		if ( FL_ISSET( zs->connect.status, ZS_STAT_WANT_WRITE_FROM_WRITE ) ) {
			// clear write_from_write, if read_from_write is not set clear fd select flag
			FL_CLR( zs->connect.status, ZS_STAT_WANT_WRITE_FROM_WRITE );
			zs_writer( fd, udata );

			// zs_writer sometimes will call zs_close
			if ( !zs_get_by_fd( fd ) ) return;
		}

		if ( FL_ISSET( zs->connect.status, ZS_STAT_WANT_WRITE_FROM_READ ) ) {
			// clear write_from_read, if read_from_read is not set and read is not set clear fd select flag
			FL_CLR( zs->connect.status, ZS_STAT_WANT_WRITE_FROM_READ );
			zs_reader( fd, udata );
		}

	} else if ( FL_ISSET( zs->connect.status, ZS_STAT_CONNECTING ) ) {
		FL_CLR( zs->connect.status, ZS_STAT_WANT_WRITE_FROM_CONNECT );
		zs_connecter( fd, udata );
	} else {
		FL_CLR( zs->connect.status, ZS_STAT_WANT_WRITE_FROM_ACCEPT );
		zs_accepter( fd, udata );

		// zs_accpeter sometimes will call zs_close
		// if this happens we must return before
		// before zs_update_fd_state tries to use it
		if ( !zs_get_by_fd( fd ) ) return;
	}

	zs_update_fd_state( fd, udata );
}

static void zs_internal_read( int fd, void* udata ) {
	zsocket_t* zs = zs_get_by_fd( fd );
	assert( zs->type == ZSOCK_CONNECT );

	if ( FL_ISSET( zs->connect.status, ZS_STAT_CONNECTED ) ) {

		if ( !FL_ARESOMESET( zs->connect.status, ZS_STAT_WANT_READ_FROM_READ | ZS_STAT_WANT_READ_FROM_WRITE ) ) {
			FL_SET( zs->connect.status, ZS_STAT_READING );
			FL_SET( zs->connect.status, ZS_STAT_WANT_READ_FROM_READ );
		}

		if ( FL_ISSET( zs->connect.status, ZS_STAT_WANT_READ_FROM_READ ) ) {
			// clear write_from_write, if read_from_write is not set clear fd select flag
			FL_CLR( zs->connect.status, ZS_STAT_WANT_READ_FROM_READ );
			zs_reader( fd, udata );
		}

		if ( FL_ISSET( zs->connect.status, ZS_STAT_WANT_READ_FROM_WRITE ) ) {
			// clear write_from_read, if read_from_read is not set and read is not set clear fd select flag
			FL_CLR( zs->connect.status, ZS_STAT_WANT_READ_FROM_WRITE );
			zs_writer( fd, udata );

			// zs_writer sometimes will call zs_close
			if ( !zs_get_by_fd( fd ) ) return;
		}

	} else if ( FL_ISSET( zs->connect.status, ZS_STAT_CONNECTING ) ) {
		FL_CLR( zs->connect.status, ZS_STAT_WANT_READ_FROM_CONNECT );
		zs_connecter( fd, udata );
	} else {
		FL_CLR( zs->connect.status, ZS_STAT_WANT_READ_FROM_ACCEPT );
		zs_accepter( fd, udata );

		// zs_accpeter sometimes will call zclose
		// if this happens we must return before
		// before zs_update_fd_state tries to use it
		if ( !zs_get_by_fd( fd ) ) return;
	}

	zs_update_fd_state( fd, udata );
}

static void zs_update_fd_state( int fd, void* udata ) {
	zsocket_t* zs;
	assert( zs = zs_get_by_fd( fd ) );

	if ( zs->type == ZSOCK_CONNECT ) {

		if ( ( zs_isset_write( fd ) && !FL_ARESOMESET( zs->connect.status, ZS_STAT_WRITING | ZS_STAT_WRITABLE ) )
		|| FL_ARESOMESET( zs->connect.status,
		ZS_STAT_WANT_WRITE_FROM_READ |
		ZS_STAT_WANT_WRITE_FROM_WRITE |
		ZS_STAT_WANT_WRITE_FROM_CONNECT |
		ZS_STAT_WANT_WRITE_FROM_ACCEPT ) ) {
			zfd_set( fd, ZFD_W, &zs_internal_write, udata );
		} else
			zfd_clr( fd, ZFD_W );

		if ( ( zs_isset_read( fd ) && !FL_ARESOMESET( zs->connect.status, ZS_STAT_READING | ZS_STAT_READABLE ) )
		|| FL_ARESOMESET( zs->connect.status,
		ZS_STAT_WANT_READ_FROM_READ |
		ZS_STAT_WANT_READ_FROM_WRITE |
		ZS_STAT_WANT_READ_FROM_CONNECT |
		ZS_STAT_WANT_READ_FROM_ACCEPT ) ) {
			zfd_set( fd, ZFD_R, &zs_internal_read, udata );
		} else
			zfd_clr( fd, ZFD_R );

	} else {
		if ( zs_isset_read( fd ) && !FL_ARESOMESET( zs->listen.status, ZS_STAT_ACCEPTING | ZS_STAT_READABLE ) )
			zfd_set( fd, ZFD_R, &zs_internal_accept, udata );
		else
			zfd_clr( fd, ZFD_R );
	}
}

////////////////////////////////////////////////////////////////////////////
void zs_set_read( int fd ) {
	zsocket_t* zs = zs_get_by_fd( fd );
	assert( zs );

	if ( !FD_ISSET( fd, &active_read_fd_set ) && FL_ISSET( zs->general.status, ZS_STAT_READABLE ) )
		n_selectable++;

	FD_SET( fd, &active_read_fd_set );
	zs_update_fd_state( fd, NULL );
}

void zs_clr_read( int fd ) {
	zsocket_t* zs = zs_get_by_fd( fd );
	assert( zs );

	if ( FD_ISSET( fd, &active_read_fd_set ) && FL_ISSET( zs->general.status, ZS_STAT_READABLE ) )
		n_selectable--;

	FD_CLR( fd, &active_read_fd_set );
	if ( zs_get_by_fd( fd ) ) zs_update_fd_state( fd, NULL );
}

bool zs_isset_read( int fd ) {
	return FD_ISSET( fd, &active_read_fd_set );
}

void zs_set_write( int fd ) {
	zsocket_t* zs = zs_get_by_fd( fd );
	assert( zs );

	if ( !FD_ISSET( fd, &active_write_fd_set ) && FL_ISSET( zs->general.status, ZS_STAT_WRITABLE ) )
		n_selectable++;

	FD_SET( fd, &active_write_fd_set );
	zs_update_fd_state( fd, NULL );
}

void zs_clr_write( int fd ) {
	zsocket_t* zs = zs_get_by_fd( fd );
	assert( zs );

	if ( FD_ISSET( fd, &active_write_fd_set ) && FL_ISSET( zs->general.status, ZS_STAT_WRITABLE ) )
		n_selectable--;

	FD_CLR( fd, &active_write_fd_set );
	if ( zs_get_by_fd( fd ) ) zs_update_fd_state( fd, NULL );
}

bool zs_isset_write( int fd ) {
	return FD_ISSET( fd, &active_write_fd_set );
}

int zs_accept( int fd ) {
	zsocket_t* zs = zs_get_by_fd( fd );

	if ( !zs || FL_ISSET( zs->general.status, ZS_STAT_CLOSED ) ) {
		errno = EBADF;
		return -1;
	}

	if ( zs->type != ZSOCK_LISTEN ) {
		errno = EINVAL;
		return -1;
	}

	if ( !FL_ISSET( zs->listen.status, ZS_STAT_READABLE ) ) {
		errno = EAGAIN;
		return -1;
	}

	if ( FD_ISSET( fd, &active_read_fd_set ) && FL_ISSET( zs->general.status, ZS_STAT_READABLE ) )
		n_selectable--;

	FL_CLR( zs->listen.status, ZS_STAT_READABLE );
	zs_update_fd_state( fd, NULL );

	if ( zs->listen.accepted_errno ) {
		errno = zs->listen.accepted_errno;
		zs->listen.accepted_errno = 0;
	} else
		assert( zs->listen.accepted_fd >= 0 );

	fd = zs->listen.accepted_fd;
	zs->listen.accepted_fd = -1;
	return fd;
}

ssize_t zs_read( int fd,  void* buf, size_t nbyte ) {
	zsocket_t* zs = zs_get_by_fd( fd );

	if ( !zs || zs->type != ZSOCK_CONNECT || FL_ISSET( zs->general.status, ZS_STAT_CLOSED ) ) {
		errno = EBADF;
		return -1;
	}

	if ( !FL_ISSET( zs->connect.status, ZS_STAT_READABLE ) ) {
		errno = EAGAIN;
		return -1;
	}

	ssize_t used = (ssize_t)nbyte > zs->connect.read.used ? zs->connect.read.used : nbyte;

	if ( zs->connect.read.rw_errno ) {
		errno = zs->connect.read.rw_errno;
		goto quit;
	}

	if ( zs->connect.read.used == 0 ) {
		assert( FL_ISSET( zs->connect.status, ZS_STAT_EOF ) );
		goto quit;
	}


	memcpy( buf, zs->connect.read.buffer + zs->connect.read.pos, used );

	zs->connect.read.pos += used;
	zs->connect.read.used -= used;

quit:
	if ( !zs->connect.read.used && !FL_ISSET( zs->connect.status, ZS_STAT_EOF ) ) {

		if ( FD_ISSET( fd, &active_read_fd_set ) && FL_ISSET( zs->general.status, ZS_STAT_READABLE ) )
			n_selectable--;

		zs->connect.read.pos = 0;
		zs->connect.read.used = 0;
		FL_CLR( zs->connect.status, ZS_STAT_READABLE );
		zs_update_fd_state( fd, NULL );
	}

	return used;
}

ssize_t zs_write( int fd, const void* buf, size_t nbyte ) {
	zsocket_t* zs = zs_get_by_fd( fd );

	if ( !zs || zs->type != ZSOCK_CONNECT || FL_ISSET( zs->general.status, ZS_STAT_CLOSED ) ) {
		errno = EBADF;
		fprintf( stderr, "%d: %d, bad\n", fd, zs->type );
		return -1;
	}

	if ( !FL_ISSET( zs->connect.status, ZS_STAT_WRITABLE ) ) {
		errno = EAGAIN;
		return -1;
	}

	assert( !zs->connect.write.buffer );

	if ( FD_ISSET( fd, &active_write_fd_set ) && FL_ISSET( zs->general.status, ZS_STAT_WRITABLE ) )
		n_selectable--;

	FL_CLR( zs->connect.status, ZS_STAT_WRITABLE );

	if ( zs->connect.write.rw_errno ) {
		errno = zs->connect.write.rw_errno || zs->connect.read.rw_errno;
		zs->connect.write.rw_errno = 0;
		nbyte = -1;
	} else {

		zs->connect.write.buffer = malloc( nbyte );
		memcpy( zs->connect.write.buffer, buf, nbyte );
		zs->connect.write.size = nbyte;
		zs->connect.write.used = nbyte;
		zs->connect.write.pos = 0;

		FL_SET( zs->connect.status, ZS_STAT_WRITING | ZS_STAT_WANT_WRITE_FROM_WRITE );
	}

	zs_update_fd_state( fd, NULL );

	return nbyte;
}

int zs_close( int fd ) {
	zsocket_t* p = zsockets[ fd ];

	if ( !p ) {
		errno = EBADF;
		return -1;
	}

	if ( p->type == ZSOCK_LISTEN ) {
		p->listen.n_open--;

		if ( p->listen.n_open > 0 )
			return 0;
	} else {
		if ( FL_ISSET( p->general.status, ZS_STAT_WRITING ) ) {
			zs_clr_read( fd );
			zs_clr_write( fd );
			FL_SET( p->general.status, ZS_STAT_CLOSED );
			return 0;
		}
	}

	zfd_clr( fd, ZFD_R );
	zfd_clr( fd, ZFD_W );
	zs_clr_read( fd );
	zs_clr_write( fd );

	zsockets[ fd ] = NULL;

	if ( p->type == ZSOCK_LISTEN ) {
		close( p->listen.sockfd );
		if ( p->listen.ssl_ctx )
			SSL_CTX_free( p->listen.ssl_ctx );
	} else if ( p->type == ZSOCK_CONNECT ) {
		// Closing
		if ( p->connect.ssl )
			SSL_shutdown( p->connect.ssl );
		close( p->connect.sockfd );

		// Freeing
		free( p->connect.read.buffer );
		if ( p->connect.ssl )
			SSL_free( p->connect.ssl );
		if ( p->connect.write.buffer )
			free( p->connect.write.buffer );
	}

	free( p );

	return 0;
}

bool zs_need_select() {
	return n_selectable;
}

int zs_select() {
	fd_set read_fd_set  = active_read_fd_set;
	fd_set write_fd_set = active_write_fd_set;

//	int rw_still_avail = 0;
	int fd;
	for ( fd = 0; fd < FD_SETSIZE; fd++ ) {
		zsocket_t* zs;
		if ( !( zs = zs_get_by_fd( fd ) ) ) continue;

		if ( FD_ISSET( fd, &read_fd_set ) && FD_ISSET( fd, &active_read_fd_set ) && FL_ISSET( zs->general.status, ZS_STAT_READABLE ) ) {
			if ( zs->type == ZSOCK_CONNECT )
				zs->general.event_hdlr( fd, ZS_EVT_READ_READY );
			else
				zs->general.event_hdlr( fd, ZS_EVT_ACCEPT_READY );
		}

		if ( FD_ISSET( fd, &write_fd_set ) && FD_ISSET( fd, &active_write_fd_set ) && FL_ISSET( zs->general.status, ZS_STAT_WRITABLE ) ) {
			assert( zs->type == ZSOCK_CONNECT );
			zs->general.event_hdlr( fd, ZS_EVT_WRITE_READY );
		}

		/*if (
		( FD_ISSET( fd, &active_read_fd_set )  && FL_ISSET( zs->general.status, ZS_STAT_READABLE ) ) ||
		( FD_ISSET( fd, &active_write_fd_set ) && FL_ISSET( zs->general.status, ZS_STAT_WRITABLE ) )
		)
			rw_still_avail++;*/
	}

	return zs_need_select();
}
