/*   Poroda - Next Generation Web Server
 *
 *+  Copyright (c) 2009 Ian Halpern
 *@  http://Poroda.org
 *
 *   This file is part of Poroda.
 *
 *   Poroda is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Poroda is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Poroda.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#include "ptransport.h"

ptransport_t* ptransport_open( int pd, int io_type, int id ) {
	ptransport_t* transport = (ptransport_t*) malloc( sizeof( ptransport_t ) );

	transport->pd = pd;
	transport->io_type = io_type;
	transport->header = (ptransport_header_t*) transport->buffer;
	transport->header->msgid = id;
	transport->message = transport->buffer + PT_HDR_SIZE;

	ptransport_reset( transport, PT_MSG_FIRST );

	return transport;
}

void ptransport_reset( ptransport_t* transport, int flags ) {
	transport->header->size = 0;
	transport->header->flags = flags;
	memset( transport->message, 0, PT_MSG_SIZE );
}

int ptransport_read( ptransport_t* transport ) {

	int n = read( transport->pd, transport->buffer, PT_BUF_SIZE );

	if ( n != PT_BUF_SIZE ) {

		/* if n (bytes read) is not zero, we have a problem other
		   than a closed connection from the other side, report
		   the error. */

		if ( n )
			fprintf( stderr, "[warning] ptransport_read: did not read corrent number of bytes\n" );

		/* A return of zero should alert the function caller should
		  to close the originating file descriptor. */
		return 0;
	}

	return n;
}

int ptransport_write( ptransport_t* transport, void* message, int size ) {
	int chunk = 0;

	while ( size ) {

		if ( transport->header->size == PT_MSG_SIZE )
			if ( !ptransport_flush( transport ) ) return 0;

		if ( size / ( PT_MSG_SIZE - transport->header->size ) )
			chunk = PT_MSG_SIZE - transport->header->size;
		else
			chunk = size % ( PT_MSG_SIZE - transport->header->size );

		memcpy( transport->message + transport->header->size, message, chunk );

		size -= chunk;
		message += chunk;
		transport->header->size += chunk;

	}

	return 1;
}

int ptransport_flush( ptransport_t* transport ) {
	int n;

	if ( ( n = write( transport->pd, transport->buffer, PT_BUF_SIZE ) ) < 0 ) {
		if ( errno == EPIPE )
			return 0;
	}

	if ( n != PT_BUF_SIZE ) {
		fprintf( stderr, "[error] ptransport_flush: did not write corrent number of bytes, only wrote %d\n", n );
		return 0;
	}

	ptransport_reset( transport, 0 );

	errno = 0; // just in case EPIPE errno was set but not from this call

	return n;
}

int ptransport_close( ptransport_t* transport ) {
	int ret = 1;

	if ( transport->io_type == PT_WO ) {
		transport->header->flags |= PT_MSG_LAST;
		ret = ptransport_flush( transport );
	}

	ptransport_free( transport );
	return ret;
}

void ptransport_free( ptransport_t* transport ) {
	free( transport );
}
