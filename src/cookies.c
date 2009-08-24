/*   Pacoda - Next Generation Web Server
 *
 *+  Copyright (c) 2009 Ian Halpern
 *@  http://Pacoda.org
 *
 *   This file is part of Pacoda.
 *
 *   Pacoda is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Pacoda is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Pacoda.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#include "cookies.h"

cookies_t cookies_parse( char* raw ) {
	cookies_t cookies;
	memset( &cookies, 0, sizeof( cookies ) );
	cookie_t* cookie;
	cookies.num = 0;
	int len;
	char* tmp = raw;
	while ( *raw != '\0' ) {
		cookie = &cookies.list[ cookies.num ];

		// name
		tmp = strstr( raw, "=" );

		if ( !tmp ) {
			break;
		}

		len = tmp - raw;
		if ( len > sizeof( cookie->name ) )
			len = sizeof( cookie->name );

		strncpy( cookie->name, raw, len );
		cookie->name[ len ] = '\0';
		raw = tmp + 1;

		// value
		tmp = strstr( raw, "; " );
		if ( !tmp ) tmp = raw + strlen( raw );

		len = tmp - raw;
		if ( len > sizeof( cookie->value ) )
			len = sizeof( cookie->value );

		strncpy( cookie->value, raw, len );
		cookie->value[ len ] = '\0';

		raw = tmp;
		if ( *tmp == ';' )
			raw += 2;

		cookie->domain[ 0 ] = '\0';
		cookie->path[ 0 ] = '\0';
		cookie->expires[ 0 ] = '\0';
		cookie->updated = 0;
		cookies.num++;
	}

	return cookies;
}

void cookies_set_cookie( cookies_t* cookies, const char* name, const char* value, time_t expires, const char* domain, const char* path ) {
	cookie_t* cookie = cookies_get_cookie( cookies, name );

	if ( !cookie ) {
		cookie = &cookies->list[ cookies->num++ ];
		strncpy( cookie->name, name, sizeof( cookie->name ) - 1 );
		cookie->name[ sizeof( cookie->name ) -1 ] = '\0';
	}

	if ( value ) {
		strncpy( cookie->value, value, sizeof( cookie->value ) - 1 );
		cookie->value[ sizeof( cookie->value ) - 1 ] = '\0';
	}

	if ( expires )
		strftime( cookie->expires, sizeof( cookie->expires ), "%a, %d-%b-%Y %I:%M:%S %Z", localtime( &expires ) );

	if ( domain ) {
		strncpy( cookie->domain, domain, sizeof( cookie->domain ) - 1 );
		cookie->domain[ sizeof( cookie->domain ) - 1 ] = '\0';
	}

	if ( path ) {
		strncpy( cookie->path, path, sizeof( cookie->path ) - 1 );
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

char* cookies_to_string( cookies_t* cookies, char* cookies_str, int size ) {
	cookie_t* cookie;
	*cookies_str = '\0';

	int i;
	for ( i = 0; i < cookies->num; i++ ) {
		cookie = &cookies->list[ i ];
		if ( cookie->updated ) {
			strncat( cookies_str, cookie->name, size ); size -= strlen( cookie->name ); if ( size < 0 ) break;
			strncat( cookies_str, "=", size ); size--; if ( size < 0 ) break;
			strncat( cookies_str, cookie->value, size ); size -= strlen( cookie->value ); if ( size < 0 ) break;

			if ( strlen( cookie->expires ) ) {
				strncat( cookies_str, "; expires=", size ); size -= 10; if ( size < 0 ) break;
				strncat( cookies_str, cookie->expires, size ); size -= strlen( cookie->expires ); if ( size < 0 ) break;
			}

			if ( strlen( cookie->path ) ) {
				strncat( cookies_str, "; path=", size ); size -= 7; if ( size < 0 ) break;
				strncat( cookies_str, cookie->path, size ); size -= strlen( cookie->path ); if ( size < 0 ) break;
			}

			if ( strlen( cookie->domain ) ) {
				strncat( cookies_str, "; domain=", size ); size -= 9; if ( size < 0 ) break;
				strncat( cookies_str, cookie->domain, size ); size -= strlen( cookie->domain ); if ( size < 0 ) break;
			}
		}
	}

	return cookies_str;
}
