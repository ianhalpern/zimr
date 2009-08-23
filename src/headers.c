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

#include "headers.h"

headers_t headers_parse( char* raw ) {
	headers_t headers;
	headers.num = 0;

	char* tmp;
	while ( !startswith( raw, HTTP_HDR_ENDL ) && *raw != '\n' && *raw != '\0' ) {
		//TODO: check string lengths

		// name
		tmp = strstr( raw, ": " );

		if ( !tmp ) {
			break;
		}

		strncpy( headers.list[ headers.num ].name, raw, tmp - raw );
		headers.list[ headers.num ].name[ tmp - raw ] = '\0';
		raw = tmp + 2;

		// value
		tmp = strstr( raw, HTTP_HDR_ENDL );
		strncpy( headers.list[ headers.num ].value, raw, tmp - raw );
		headers.list[ headers.num ].value[ tmp - raw ] = '\0';

		raw = tmp + strlen( HTTP_HDR_ENDL );

		headers.num++;
	}

	return headers;
}

char* headers_to_string( headers_t* headers, char* headers_str ) {
	*headers_str = '\0';

	int i;
	for ( i = 0; i < headers->num; i++ ) {
		strcat( headers_str, headers->list[ i ].name );
		strcat( headers_str, ": " );
		strcat( headers_str, headers->list[ i ].value );
		strcat( headers_str, HTTP_HDR_ENDL );
	}

	strcat( headers_str, HTTP_HDR_ENDL );

	return headers_str;
}

void headers_set_header( headers_t* headers, char* name, char* value ) {
	header_t* header = headers_get_header( headers, name );

	if ( !header ) {
		header = &headers->list[ headers->num++ ];
		strcpy( header->name, name );
	}

	strcpy( header->value, value );
}

header_t* headers_get_header( headers_t* headers, char* name ) {
	int i;

	for ( i = 0; i < headers->num; i++ ) {
		if ( strcmp( headers->list[ i ].name, name ) == 0 )
			return &headers->list[ i ];
	}

	return NULL;
}
