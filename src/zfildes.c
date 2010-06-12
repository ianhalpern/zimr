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

#include "zfildes.h"

static fd_set active_read_fd_set;
static fd_set active_write_fd_set;
static fd_info_t fd_data[ FD_SETSIZE ][2];
static bool first_set = true;
static bool unblock = false;

void zfd_set( int fd, char io_type, void (*handler)( int, void* ), void* udata ) {
	if ( first_set ) {
		FD_ZERO( &active_read_fd_set );
		FD_ZERO( &active_write_fd_set );
		memset( fd_data, 0, sizeof( fd_data ) );
		first_set = false;
	}

	fd_data[ fd ][ io_type - 1 ].handler = handler;
	fd_data[ fd ][ io_type - 1 ].udata = udata;

	if ( io_type == ZFD_R )
		FD_SET( fd, &active_read_fd_set );

	else if ( io_type == ZFD_W )
		FD_SET( fd, &active_write_fd_set );
}

void zfd_clr( int fd, char io_type ) {

	if ( io_type == ZFD_R )
		FD_CLR( fd, &active_read_fd_set );

	else if ( io_type == ZFD_W )
		FD_CLR( fd, &active_write_fd_set );

}

bool zfd_isset( int fd, char io_type ) {

	if ( io_type == ZFD_R )
		return FD_ISSET( fd, &active_read_fd_set );

	else if ( io_type == ZFD_W )
		return FD_ISSET( fd, &active_write_fd_set );

	return false;
}

void zfd_unblock() {
	unblock = true;
}

int zfd_select( int tv_sec ) {
	int i;

	fd_set read_fd_set, write_fd_set;

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

	read_fd_set = active_read_fd_set;
	write_fd_set = active_write_fd_set;

	if ( select( FD_SETSIZE, &read_fd_set, &write_fd_set, NULL, timeout_ptr ) < 0 ) {

		if ( errno != EINTR ) /* select() was not interrupted. This is an
								 unanticipated error. */
			perror( "[error] zfd_select: select() failed" );

		/* if interrupted or select failed we are trying to quit...return with error (false). */
		return 0;

	}

	for ( i = 0; i < FD_SETSIZE; i++ ) {
		if ( FD_ISSET( i, &read_fd_set ) && FD_ISSET( i, &active_read_fd_set ) )
			fd_data[ i ][ ZFD_R - 1 ].handler( i, fd_data[ i ][ ZFD_R - 1 ].udata );

		if ( /*FD_ISSET( i, &write_fd_set &&*/ FD_ISSET( i, &active_write_fd_set ) ) {
			fd_data[ i ][ ZFD_W - 1 ].handler( i, fd_data[ i ][ ZFD_W - 1 ].udata );
		}
	}

	return 1;
}
