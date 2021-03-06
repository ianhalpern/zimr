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
		ptr = strstr( raw, ":" );

		if ( !ptr || !( ptr - raw ) )
			break;

		strncpy( header->name, raw, SMALLEST( ptr - raw, sizeof( header->name ) - 1 ) );
		header_formatname( strtolower( headers.list[ headers.num ].name ) );
		raw = ptr + 1;

		while ( *raw == ' ' ) raw++;

		// value
		if ( !( ptr = strstr( raw, HTTP_HDR_ENDL ) ) )
			break;
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

header_t* headers_set_header( headers_t* headers, const char* name, const char* value ) {
	header_t* header = headers_get_header( headers, name );

	if ( !header || strcmp( header->name, "Set-Cookie" ) == 0 ) {
		header = &headers->list[ headers->num++ ];
		strcpy( header->name, name );
		header_formatname( strtolower( header->name ) );
	}
	//printf( "\n\nnum header %d\n", headers->num );

	strcpy( header->value, value );

	return header;
}

header_t* headers_get_header( headers_t* headers, const char* orig_name ) {
	char name[ strlen( orig_name ) + 1 ];
	strcpy( name, orig_name );
	header_formatname( strtolower( name ) );

	int i;

//	printf( "get header %s from %d headers @ 0x%x\n", name, headers->num, &headers );
	for ( i = 0; i < headers->num; i++ ) {
	//	printf( "header %d: %s == %s\n", i, headers->list[ i ].name, name );
		if ( strcmp( headers->list[ i ].name, name ) == 0 ) {
			return &headers->list[ i ];
		}
	}

	return NULL;
}

void headers_header_range_parse( header_t* header, int* range_start, int* range_end ) {
	*range_start = 0; *range_end = -1;
	char* bytes = headers_get_header_attr( header, "bytes" ),* sep = NULL;

	if ( !bytes ) return;
	if ( !( sep = strchr( bytes, '-' ) ) ) return;

	if ( sep - bytes )
		*range_start = strtol( bytes, &sep, 10 );

	sep++;
	if ( *sep )
		*range_end = atoi( sep );
}

char* headers_get_header_attr( header_t* header, char* name ) {
	char value[HEADER_VALUE_MAX_LEN] = "";
	char* head = header->value,* tail = NULL;

	if ( !name ) {
		if( !( tail = strchr( head, ';' ) ) )
			tail = head + strlen( head );
	} else {
		while ( ( head = strstr( head, name ) ) ) {
			head += strlen( name );
			if ( head[0] == '=' ) {
				head++;
				break;
			}
		}
		if ( !head ) return NULL;

		if ( head[0] != '"' || !( tail = strchr( head + 1, '"' ) ) ) {
			if( !( tail = strchr( head, ';' ) ) )
				tail = head + strlen( head );
		} else head++;
	}
	strncat( value, head, tail - head );

	return value;
}

