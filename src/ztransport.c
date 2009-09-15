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

static bool initialized = false;

void ztinit( ) {
	assert( !initialized );
	memset( zts, 0, sizeof( zts ) );
	initialized = true;
}

static zt_t* ztnew( int fd, int msgid, int flags ) {
	zt_t* zt = (zt_t*) malloc( sizeof( zt_t ) );

	zt->msgid = msgid;
	zt->size = 0;
	zt->flags = flags;

	memset( zt->message, 0, ZT_MSG_SIZE );
	list_append( zts, zt );

	return zt;
}

void ztopen( int fd, int msgid ) {
	assert( initialized );

	if ( !zts[ fd ] ) {
		zts[ fd ] = (list_t*) malloc( sizeof( list_t* ) * FD_SETSIZE );
		memset( zts, 0, sizeof( list_t* ) * FD_SETSIZE );
	}

	if ( !zts[ fd ][ msgid ] ) {
		zt[ fd ][ msgid ] = (list_t*) malloc( sizeof( list_t ) );
		list_init( zts[ fd ][ msgid ] );
	}

	ztnew( fd, msgid, ZT_FIRST );
}

void ztpush( int fd, int msgid, void* message, int size ) {
	assert( initialized );

	zt_t* zt = NULL;

	int i;
	for ( i = list_size( zts[ fd ] ) - 1; i >= 0; i-- ) {
		zt = list_get_at( zts[ fd ], i );
		if ( zt->msgid == msgid ) break;
		zt = NULL;
	}

	if ( !zt )
		ztnew( fd, msgid, 0 );

	int chunk = 0;

	while ( size ) {

		if ( zt->size == ZT_MSG_SIZE )
			zt = ztnew( fd, msgid, 0 );

		if ( size / ( ZT_MSG_SIZE - zt->size ) )
			chunk = ZT_MSG_SIZE - zt->size;
		else
			chunk = size % ( ZT_MSG_SIZE - zt->size );

		memcpy( zt->message + zt->size, message, chunk );

		size -= chunk;
		message += chunk;
		zt->size += chunk;

	}
}

void ztclose( int fd, int msgid ) {
	assert( initialized );

	int i;
	for ( i = list_size( zts[ fd ] ) - 1; i >= 0; i-- ) {
		zt = list_get_at( zts[ fd ], i );
		if ( zt->msgid == msgid ) break;
		zt = NULL;
	}

	if ( !zt )
		ztnew( fd, msgid, 0 );

	zt->flags |= ZT_LAST;
}
