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

#include "pcom.h"

static int npds = 0;

int pcom_create( ) {
	psocket_t* socket = psocket_open( inet_addr( PCOM_ADDR ), PCOM_SOCK );
	if ( !socket ) return -1;
	return socket->sockfd;
}

int pcom_connect( int pid, int fd ) {
	psocket_t* socket = psocket_connect( inet_addr( PCOM_ADDR ), PCOM_SOCK );
	if ( !socket ) return -1;
	return socket->sockfd;
}

void pcom_destroy( int fd ) {
	psocket_t* socket = psocket_get_by_sockfd( fd );
	if ( socket )
		psocket_remove( socket );
}

pcom_transport_t* pcom_open( int pd, int io_type, int id, int key ) {
	pcom_transport_t* transport = (pcom_transport_t*) malloc( sizeof( pcom_transport_t ) );

	transport->pd = pd;
	transport->io_type = io_type;
	transport->header = (pcom_header_t*) transport->buffer;
	transport->header->id = id;
	transport->header->key = key;
	transport->message = transport->buffer + PCOM_HDR_SIZE;

	pcom_reset_header( transport, PCOM_MSG_FIRST );

	return transport;
}

void pcom_reset_header( pcom_transport_t* transport, int flags ) {
	transport->header->size = 0;
	transport->header->flags = flags;
}

int pcom_read( pcom_transport_t* transport ) {

	int n = read( transport->pd, transport->buffer, PCOM_BUF_SIZE );

	if ( n != PCOM_BUF_SIZE ) {
		fprintf( stderr, "[warning] pcom_read: did not read corrent number of bytes\n" );
		return 0;
	}

	return n;
}

int pcom_write( pcom_transport_t* transport, void* message, int size ) {
	int chunk = 0;

	while ( size ) {

		if ( transport->header->size == PCOM_MSG_SIZE )
			if ( !pcom_flush( transport ) ) return 0;

		if ( size / ( PCOM_MSG_SIZE - transport->header->size ) )
			chunk = PCOM_MSG_SIZE - transport->header->size;
		else
			chunk = size % ( PCOM_MSG_SIZE - transport->header->size );

		memcpy( transport->message + transport->header->size, message, chunk );

		size -= chunk;
		message += chunk;
		transport->header->size += chunk;

	}

	return 1;
}

int pcom_flush( pcom_transport_t* transport ) {
	int n;

	if ( ( n = write( transport->pd, transport->buffer, PCOM_BUF_SIZE ) ) < 0 ) {
		if ( errno == EPIPE )
			return 0;
	}

	if ( n != PCOM_BUF_SIZE ) {
		fprintf( stderr, "[error] pcom_flush: did not write corrent number of bytes, only wrote %d\n", n );
		return 0;
	}

	pcom_reset_header( transport, 0 );

	errno = 0; // just in case EPIPE errno was set but not from this call

	return n;
}

int pcom_close( pcom_transport_t* transport ) {
	int ret = 1;

	if ( transport->io_type == PCOM_WO ) {
		transport->header->flags |= PCOM_MSG_LAST;
		ret = pcom_flush( transport );
	}

	pcom_free( transport );
	return ret;
}

void pcom_free( pcom_transport_t* transport ) {
	free( transport );
}
