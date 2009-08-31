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

#include "website.h"

static bool initialized = false;

void website_init( ) {
	assert( !initialized );
	initialized = true;
	list_init( &websites );
}

website_t* website_add( int sockfd, char* url ) {
	assert( initialized );

	website_t* w = (website_t*) malloc( sizeof( website_t ) );

	w->sockfd = sockfd;
	w->url = strdup( url );

	list_append( &websites, w );

	return w;
}

void website_remove( website_t* w ) {
	assert( initialized );

	int i = list_locate( &websites, w );
	if ( i >=0 )
		list_delete_at( &websites, i );

	free( w->url );
	free( w );
}

website_t* website_get_by_url( char* url ) {
	assert( initialized );

	int i;
	for ( i = 0; i < list_size( &websites ); i++ ) {
		website_t* w = list_get_at( &websites, i );
		if ( strcmp( url, w->url ) == 0 ) return w;
	}

	return NULL;
}

website_t* website_find_by_url( char* url ) {
	assert( initialized );

	int len = 0;
	int found = -1;

	int i;
	for ( i = 0; i < list_size( &websites ); i++ ) {
		website_t* w = list_get_at( &websites, i );
		if ( startswith( url, w->url ) && strlen( w->url ) > len ) {
			len = strlen( w->url );
			found = i;
		}
	}

	if ( found != -1 ) return list_get_at( &websites, found );

	return NULL;
}

website_t* website_get_by_sockfd( int sockfd ) {
	assert( initialized );

	int i;
	for ( i = 0; i < list_size( &websites ); i++ ) {
		website_t* w = list_get_at( &websites, i );
		if ( sockfd == w->sockfd ) return w;
	}

	return NULL;
}
