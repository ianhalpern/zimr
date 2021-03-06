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
static unsigned int default_website_options = WS_OPTION_PARSE_MULTIPARTS;

void website_init() {
	assert( !initialized );
	initialized = true;
	list_init( &websites );
}

website_t* website_add( int sockfd, char* url, char* ip ) {
	assert( initialized );

	if ( !ip ) ip = "0.0.0.0";

	website_t* w = (website_t*) malloc( sizeof( website_t ) );

	w->options = default_website_options;
	w->sockfd = sockfd;
	strcpy( w->ip, ip );

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

	w->path = strchr( w->url, '/' );
	if ( !w->path ) w->path = w->url + strlen( w->url );

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

website_t* website_get_by_full_url( char* full_url, char* ip ) {
	assert( initialized );

	if ( !ip ) ip = "0.0.0.0";

	int i;
	for ( i = 0; i < list_size( &websites ); i++ ) {
		website_t* w = list_get_at( &websites, i );
		if ( strcmp( full_url, w->full_url ) == 0 && strcmp( ip, w->ip ) == 0 ) return w;
	}

	return NULL;
}

website_t* website_find( char* url, char* protocol, char* ip ) {
	assert( initialized );

	if ( !protocol ) protocol = "http://";
	if ( !ip ) ip = "0.0.0.0";

	int len = 0;
	int found = -1;

	int i;
	for ( i = 0; i < list_size( &websites ); i++ ) {
		website_t* w = list_get_at( &websites, i );

		char* w_url_ptr = w->url,* path_ptr = url,* url_ptr = url;

		path_ptr = strchr( url, '/' );
		if ( !path_ptr ) path_ptr = url + strlen( url );

		if ( w_url_ptr[0] == '*' ) { // Wildcard hostnames
			w_url_ptr += 1;
			url_ptr = strchr( url, ':' );
			if ( !url_ptr ) url_ptr = strchr( url, '/' );
			if ( !url_ptr ) url_ptr = url + strlen( url );
		}

		int host_port_len = ( w->path - w_url_ptr ) > ( path_ptr - url_ptr ) ? w->path - w_url_ptr : path_ptr - url_ptr;

		if ( strncmp( w_url_ptr, url_ptr, host_port_len ) == 0 // match hostname:port
		&& ( !w->path || startswith( path_ptr, w->path ) ) // match path
		&& strlen( w->path ) >= len // is most specific path matched
		&& startswith( w->full_url, protocol ) // match protocol
		&& strcmp( w->ip, ip ) == 0 // match ip
		) {
			len = strlen( w->path ) + ( w->url[0] != '*' ); // adds 1 if not * to the length b/c non-wildcard hostnames get priority
			found = i;
		}
	}

	if ( found != -1 ) {
	//	website_t* w = list_get_at( &websites, found );
	//	printf( "%s -> %s\n", url, w->full_url );
		return list_get_at( &websites, found );
	}

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

void website_options_set(  website_t* website, unsigned int opt, bool val ) {
	unsigned int* options = website ? &website->options : &default_website_options;
	if ( val )
		FL_SET( *(options), opt );
	else
		FL_CLR( *(options), opt );
}

bool website_options_get( website_t* website, unsigned int opt ) {
	unsigned int* options = website ? &website->options : &default_website_options;
	return FL_ISSET( *(options), opt );
}

void website_options_reset( website_t* website ) {
	website->options = default_website_options;
}
