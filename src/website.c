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

#include "website.h"

static website_t* root_website_node = NULL;

website_t* website_add( int sockfd, char* url ) {
	website_t* w = (website_t* ) malloc( sizeof( website_t ) );

	w->sockfd = sockfd;
	w->url = strdup( url );

	w->next = root_website_node;
	w->prev = NULL;

	if ( root_website_node != NULL )
		root_website_node->prev = w;

	root_website_node = w;

	return w;
}

void website_remove( website_t* w ) {

	if ( w == root_website_node )
		root_website_node = w->next;

	if ( w->prev != NULL )
		w->prev->next = w->next;
	if ( w->next != NULL )
		w->next->prev = w->prev;

	free( w->url );
	free( w );
}

website_t* website_get_by_url( char* url ) {
	website_t* w = root_website_node;

	while ( w != NULL ) {

		if ( startswith( url, w->url ) )
			return w;

		w = w->next;
	}

	return NULL;
}

website_t* website_get_by_sockfd( int sockfd ) {
	website_t* w = root_website_node;

	while ( w != NULL ) {

		if ( sockfd == w->sockfd )
			return w;

		w = w->next;
	}

	return NULL;
}

website_t* website_get_root( ) {
	return root_website_node;
}
