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

#include "pfildes.h"

static fd_set active_fd_set, read_fd_set;
static fd_info_t fd_data[ FD_SETSIZE ];
static fd_type_t fd_types[ 64 ];
static bool first_set = true;
static bool unblock = false;

void pfd_set( int fd, int type, void* udata ) {
	if ( first_set ) {
		FD_ZERO( &read_fd_set );
		memset( fd_data, 0, sizeof( fd_data ) );
		first_set = false;
	}
	FD_SET( fd, &active_fd_set );
	fd_data[ fd ].type = type;
	fd_data[ fd ].udata = udata;
}

void pfd_clr( int fd ) {
	FD_CLR( fd, &active_fd_set );
}

void* pfd_udata( int fd ) {
	return fd_data[ fd ].udata;
}

void pfd_register_type( int type, void (*handler)( int, void* ) ) {
	fd_types[ type ].handler = handler;
}

void pfd_unblock( ) {
	unblock = true;
}

int pfd_select( int tv_sec ) {
	int i;

	struct timeval timeout,* timeout_ptr = NULL;

	if ( unblock ) {
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		timeout_ptr = &timeout;
	} if ( tv_sec ) {
		timeout.tv_sec = tv_sec;
		timeout.tv_usec = 0;
		timeout_ptr = &timeout;
	}
	unblock = false;

	read_fd_set = active_fd_set;

	if ( select( FD_SETSIZE, &read_fd_set, NULL, NULL, timeout_ptr ) < 0 ) {

		if ( errno != EINTR ) /* select() was not interrupted. This is an
								 unanticipated error. */
			perror( "[error] pfd_start: select() failed" );

		/* if interrupted we are trying to quit...return with error (false). */
		else return 0;

	}

	for ( i = 0; i < FD_SETSIZE; i++ )
		if ( FD_ISSET( i, &read_fd_set ) ) {
			fd_types[ fd_data[ i ].type ].handler( i, fd_data[ i ].udata );
		}

	return 1;
}
