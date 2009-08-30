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

#include "ztransport.h"

ztransport_t* ztransport_open( int pd, int io_type, int id ) {
	ztransport_t* transport = (ztransport_t*) malloc( sizeof( ztransport_t ) );

	transport->pd = pd;
	transport->io_type = io_type;
	transport->header = (ztransport_header_t*) transport->buffer;
	transport->header->msgid = id;
	transport->message = transport->buffer + ZT_HDR_SIZE;

	ztransport_reset( transport, ZT_MSG_FIRST );

	return transport;
}

void ztransport_reset( ztransport_t* transport, int flags ) {
	transport->header->size = 0;
	transport->header->flags = flags;
	memset( transport->message, 0, ZT_MSG_SIZE );
}

int ztransport_read( ztransport_t* transport ) {

	int n = read( transport->pd, transport->buffer, ZT_BUF_SIZE );

	if ( n != ZT_BUF_SIZE ) {

		/* if n (bytes read) is not zero, we have a problem other
		   than a closed connection from the other side, report
		   the error. */

		if ( n )
			fprintf( stderr, "[warning] ztransport_read: did not read corrent number of bytes\n" );

		/* A return of zero should alert the function caller should
		  to close the originating file descriptor. */
		return 0;
	}

	return n;
}

int ztransport_write( ztransport_t* transport, void* message, int size ) {
	int chunk = 0;

	while ( size ) {

		if ( transport->header->size == ZT_MSG_SIZE )
			if ( !ztransport_flush( transport ) ) return 0;

		if ( size / ( ZT_MSG_SIZE - transport->header->size ) )
			chunk = ZT_MSG_SIZE - transport->header->size;
		else
			chunk = size % ( ZT_MSG_SIZE - transport->header->size );

		memcpy( transport->message + transport->header->size, message, chunk );

		size -= chunk;
		message += chunk;
		transport->header->size += chunk;

	}

	return 1;
}

int ztransport_flush( ztransport_t* transport ) {
	int n;

	if ( ( n = write( transport->pd, transport->buffer, ZT_BUF_SIZE ) ) < 0 ) {
		if ( errno == EPIPE )
			return 0;
	}

	if ( n != ZT_BUF_SIZE ) {
		fprintf( stderr, "[error] ztransport_flush: did not write corrent number of bytes, only wrote %d\n", n );
		return 0;
	}

	ztransport_reset( transport, 0 );

	errno = 0; // just in case EPIPE errno was set but not from this call

	return n;
}

int ztransport_close( ztransport_t* transport ) {
	int ret = 1;

	if ( transport->io_type == ZT_WO ) {
		transport->header->flags |= ZT_MSG_LAST;
		ret = ztransport_flush( transport );
	}

	ztransport_free( transport );
	return ret;
}

void ztransport_free( ztransport_t* transport ) {
	free( transport );
}
