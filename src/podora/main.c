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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "ptransport.h"
#include "psocket.h"
#include "pfildes.h"


void connection_handler( int sockfd, void* udata ) {
	puts( "hi" );
	int msgid;
	ptransport_t* transport = ptransport_open( sockfd, PT_RO, 0 );

	if ( ptransport_read( transport ) <= 0 || PT_MSG_IS_LAST( transport ) ) {
		puts( "done" );
	}
	msgid = transport->header->msgid;

	puts( inet_ntoa( *(struct in_addr*) transport->message ) );
	puts( transport->message + 4 );
	puts( transport->message + 4 + strlen( transport->message + 4 ) + 1 );

	ptransport_close( transport );

	transport = ptransport_open( sockfd, PT_WO, msgid );
	ptransport_write( transport, "hello world", 11 );
	ptransport_close( transport );
}

int main( int argc, char* argv[ ] ) {
	psocket_t* proxy = psocket_connect( inet_addr( PROXY_ADDR ), PROXY_PORT );

	if ( !proxy ) {
		printf( "could not connect to proxy.\n" );
		return EXIT_FAILURE;
	}

	ptransport_t* trans_out = ptransport_open( proxy->sockfd, PT_WO, -1 );
	ptransport_t* trans_in  = ptransport_open( proxy->sockfd, PT_RO, 0 );
	ptransport_write( trans_out, "podora:8080", 11 );
	ptransport_close( trans_out );
	ptransport_read( trans_in );

	if ( strcmp( "OK", trans_in->message ) != 0 )
		return EXIT_FAILURE;

	pfd_register_type( PFD_TYPE_INT_CONNECTED, PFD_TYPE_HDLR connection_handler );

	pfd_set( proxy->sockfd, PFD_TYPE_INT_CONNECTED, NULL );

	pfd_start( );

	return EXIT_SUCCESS;
}
