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

#include "cookies.h"

cookies_t cookies_parse( char* raw ) {
	cookies_t cookies;
	cookies.num = 0;

	char* tmp = raw;
	while ( *raw != '\0' ) {
		//TODO: check string lengths

		// name
		tmp = strstr( raw, "=" );

		if ( !tmp ) {
			break;
		}

		strncpy( cookies.list[ cookies.num ].name, raw, tmp - raw );
		cookies.list[ cookies.num ].name[ tmp - raw ] = '\0';
		raw = tmp + 1;

		// value
		tmp = strstr( raw, "; " );
		if ( !tmp ) tmp = raw + strlen( raw );
		strncpy( cookies.list[ cookies.num ].value, raw, tmp - raw );
		cookies.list[ cookies.num ].value[ tmp - raw ] = '\0';

		raw = tmp;
		if ( *tmp == ';' )
			raw += 2;
		cookies.list[ cookies.num ].domain[ 0 ] = '\0';
		cookies.list[ cookies.num ].path[ 0 ] = '\0';
		cookies.list[ cookies.num ].expires[ 0 ] = '\0';
		cookies.list[ cookies.num ].updated = 0;
		cookies.num++;
	}

	return cookies;
}

void cookies_set_cookie( cookies_t* cookies, const char* name, const char* value, time_t* expires, const char* domain, const char* path ) {
	cookie_t* cookie = cookies_get_cookie( cookies, name );

	if ( !cookie ) {
		cookie = &cookies->list[ cookies->num++ ];
		strncpy( cookie->name, name, sizeof( cookie->name ) );
		cookie->name[ sizeof( cookie->name ) -1 ] = '\0';
	}

	if ( value ) {
		strncpy( cookie->value, value, sizeof( cookie->value ) );
		cookie->value[ sizeof( cookie->value ) - 1 ] = '\0';
	}

	if ( expires )
		strftime( cookie->expires, sizeof( cookie->expires ), "%a, %d-%b-%Y %I:%M:%S %Z", localtime( expires ) );

	if ( domain ) {
		strncpy( cookie->domain, domain, sizeof( cookie->domain ) );
		cookie->domain[ sizeof( cookie->domain ) - 1 ] = '\0';
	}

	if ( path ) {
		strncpy( cookie->path, path, sizeof( cookie->path ) );
		cookie->path[ sizeof( cookie->path ) - 1 ] = '\0';
	}

	cookie->updated = 1;
}

cookie_t* cookies_get_cookie( cookies_t* cookies, const char* name ) {
	int i;
	for ( i = 0; i < cookies->num; i++ ) {
		if ( strcmp( cookies->list[ i ].name, name ) == 0 ) {
			return &cookies->list[ i ];
		}
	}

	return NULL;
}

char* cookies_to_string( cookies_t* cookies, char* cookies_str ) {
	*cookies_str = '\0';

	int i;
	for ( i = 0; i < cookies->num; i++ ) {
		if ( cookies->list[ i ].updated ) {
			strcat( cookies_str, cookies->list[ i ].name );
			strcat( cookies_str, "=" );
			strcat( cookies_str, cookies->list[ i ].value );

			if ( strlen( cookies->list[ i ].expires ) ) {
				strcat( cookies_str, "; expires=" );
				strcat( cookies_str, cookies->list[ i ].expires );
			}

			if ( strlen( cookies->list[ i ].path ) ) {
				strcat( cookies_str, "; path=" );
				strcat( cookies_str, cookies->list[ i ].path );
			}

			if ( strlen( cookies->list[ i ].domain ) ) {
				strcat( cookies_str, "; domain=" );
				strcat( cookies_str, cookies->list[ i ].domain );
			}
		}
	}

	return cookies_str;
}
