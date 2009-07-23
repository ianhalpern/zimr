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

#include "pfildes.h"

static fd_set active_fd_set, read_fd_set;
static fd_info_t fd_data[ FD_SETSIZE ];
static fd_type_t fd_types[ 64 ];
static int first_set = 1;

void pfd_set( int fd, int type, void* udata ) {
	if ( first_set ) {
		FD_ZERO( &read_fd_set );
		first_set = 0;
	}
	FD_SET( fd, &active_fd_set );
	fd_data[ fd ].type = type;
	fd_data[ fd ].udata = udata;
}

void pfd_clr( int fd ) {
	FD_CLR( fd, &active_fd_set );
}

void pfd_register_type( int type, void (*handler)( int, void* ) ) {
	fd_types[ type ].handler = handler;
}

void pfd_start( ) {
	int i;

	while ( 1 ) {

		read_fd_set = active_fd_set;

		if ( select( FD_SETSIZE, &read_fd_set, NULL, NULL, NULL ) < 0 ) {
			if ( errno != EINTR )
				perror( "[error] pfd_start: select() failed" );
			return;
		}

		for ( i = 0; i < FD_SETSIZE; i++ )
			if ( FD_ISSET( i, &read_fd_set ) ) {
				fd_types[ fd_data[ i ].type ].handler( i, fd_data[ i ].udata );
			}

	}

}
