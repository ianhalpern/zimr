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

#include "params.h"

list_t params_create() {
	list_t params;
	list_init( &params );
	return params;
}

void params_parse_qs( list_t* params, char* raw, int size ) {
	param_t* param;
	int len;

	char* ptr;
	while ( size > 0 ) {
		char* name;
		char* value;

		// name
		ptr = strchr( raw, '=' );

		if ( !ptr ) {
			break;
		}

		size -= ( ptr + 1 ) - raw;
		if ( size <= 0 )
			break;

		len = ptr - raw;

		name = (char*) malloc( len + 1 );
		url_decode( name, raw, len );
		raw = ptr + 1;

		// value
		ptr = strchr( raw, '&' );
		if ( !ptr || ptr - raw > size ) ptr = raw + size;

		len = ptr - raw;

		value = (char*) malloc( len + 1 );
		url_decode( value, raw, len );

		size -= ptr - raw;
		raw = ptr;
		if ( *ptr == '&' ) {
			size--;
			raw++;
		}

		param = (param_t*) malloc( sizeof( param_t ) );
		param->name = name;
		param->value = value;
		list_append( params, param );
	}
}

param_t* params_get_param( list_t* params, const char* name ) {
	int i;
	for( i = 0; i < list_size( params ); i++ ) {
		param_t* param = list_get_at( params, i );
		if ( strcmp( param->name, name ) == 0 )
			return param;
	}

	return NULL;
}

void params_free( list_t* params ) {
	param_t* param;

	int i;
	for ( i = 0; i < list_size( params ); i++ ) {
		param = list_get_at( params, i );
		free( param->name );
		free( param->value );
		free( param );
	}

	list_destroy( params );
}
