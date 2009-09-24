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

#include "headers.h"

char* header_formatname( char* name ) {
	char* ptr = name - 1;

	do {
		ptr++;
		*ptr = toupper( *ptr );
	} while( ( ptr = strstr( ptr, "-" ) ) );

	return name;
}

headers_t headers_parse( char* raw ) {
	headers_t headers;
	header_t* header;
	memset( &headers, 0, sizeof( headers ) );

	char* ptr;
	while ( headers.num < sizeof( headers ) / sizeof( header_t ) && !startswith( raw, HTTP_HDR_ENDL ) && *raw != '\n' && *raw != '\0' ) {
		header = &headers.list[ headers.num ];

		// name
		ptr = strstr( raw, ": " );

		if ( !ptr || !( ptr - raw ) )
			break;

		strncpy( header->name, raw, SMALLEST( ptr - raw, sizeof( header->name ) - 1 ) );
		header_formatname( strtolower( headers.list[ headers.num ].name ) );
		raw = ptr + 2;

		// value
		ptr = strstr( raw, HTTP_HDR_ENDL );
		strncpy( header->value, raw, SMALLEST( ptr - raw, sizeof( header->value ) - 1 ) );

		raw = ptr + strlen( HTTP_HDR_ENDL );
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
		header_formatname( strtolower( header->name ) );
	}

	strcpy( header->value, value );
}

header_t* headers_get_header( headers_t* headers, char* orig_name ) {
	char name[ strlen( orig_name ) + 1 ];
	strcpy( name, orig_name );
	header_formatname( strtolower( name ) );

	int i;
	for ( i = 0; i < headers->num; i++ ) {
		if ( strcmp( headers->list[ i ].name, name ) == 0 ) {
			return &headers->list[ i ];
		}
	}

	return NULL;
}
