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
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "general.h"
#include "request.h"

request_t request_create( website_t* website, int sockfd, char* raw, size_t size ) {
	request_t r;
	char* tmp, * start = raw;

	r.sockfd = sockfd;

	// type
	if ( startswith( raw, HTTP_GET ) ) {
		r.type = HTTP_GET_TYPE;
		raw += strlen( HTTP_GET ) + 1;
	} else if ( startswith( raw, HTTP_POST ) ) {
		r.type = HTTP_POST_TYPE;
		raw += strlen( HTTP_POST ) + 1;
	}

	//printf( "type: \"%s\"\n", RTYPE( r.type ) );

	// url
	raw++; // skip over forward slash
	tmp = strstr( raw, " " );
	memcpy( r.url, raw, tmp - raw );
	*( r.url + ( tmp - raw ) ) = '\0';
	raw = tmp + 1;

	//printf( "url: \"%s\"\n", r.url );

	// version
	raw += 5; // skips "HTTP/"
	tmp = strstr( raw, HTTP_HDR_ENDL );
	memcpy( r.http_version, raw, tmp - raw + 1 );
	r.http_version[ tmp - raw ] = '\0';
	raw = tmp + 2;

	//printf( "version: \"%s\"\n", r.http_version );


	// headers
	r.headers = headers_parse( raw );

	//int i;
	//for ( i = 0; i < r.headers.num; i++ ) {
	//	printf( "%s: %s\n", r.headers.list[ i ].name, r.headers.list[ i ].value );
	//}


	// body
	if ( r.type == HTTP_POST_TYPE ) {
		if ( ( tmp = strstr( raw, HTTP_HDR_ENDL HTTP_HDR_ENDL ) ) != NULL ) {
			tmp += strlen( HTTP_HDR_ENDL HTTP_HDR_ENDL );
			strncpy( r.post_body, tmp, size - (long) ( tmp - start ) );
			*( r.post_body + ( size - (long) ( tmp - start ) + 1 ) ) = '\0';
		}
	} else r.post_body[ 0 ] = '\0';

	r.website = website;
	r.response = response_create( r.sockfd, r.website, r.http_version );

	return r;
}

