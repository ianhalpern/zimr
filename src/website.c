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

void website_init() {
	assert( !initialized );
	initialized = true;
	list_init( &websites );
}

website_t* website_add( int sockfd, char* url ) {
	assert( initialized );

	website_t* w = (website_t*) malloc( sizeof( website_t ) );

	w->sockfd = sockfd;

	if ( startswith( url, "https://" ) ) {
		w->full_url = strdup( url );
		w->url = w->full_url + 8;
	} else if ( startswith( url, "http://" ) ) {
		w->full_url = strdup( url );
		w->url = w->full_url + 7;
	} else {
		w->full_url = malloc( 8 + strlen( url ) );
		strcpy( w->full_url, "http://" );
		strcat( w->full_url, url );
		w->url = w->full_url + 7;
	}

	list_append( &websites, w );

	return w;
}

void website_remove( website_t* w ) {
	assert( initialized );

	int i = list_locate( &websites, w );
	if ( i >=0 )
		list_delete_at( &websites, i );

	free( w->full_url );
	free( w );
}

website_t* website_get_by_full_url( char* full_url ) {
	assert( initialized );

	int i;
	for ( i = 0; i < list_size( &websites ); i++ ) {
		website_t* w = list_get_at( &websites, i );
		if ( strcmp( full_url, w->full_url ) == 0 ) return w;
	}

	return NULL;
}

website_t* website_find( char* url, char* protocol ) {
	assert( initialized );

	if ( !protocol ) protocol = "http://";

	int len = 0;
	int found = -1;

	int i;
	for ( i = 0; i < list_size( &websites ); i++ ) {
		website_t* w = list_get_at( &websites, i );
		if ( startswith( url, w->url ) && strlen( w->url ) > len && startswith( w->full_url, protocol ) ) {
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

const char* website_protocol( website_t* w ) {
	if ( startswith( w->full_url, "https://" ) )
		return "https://";
	return "http://";
}
