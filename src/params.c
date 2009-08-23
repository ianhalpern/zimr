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

#include "params.h"

params_t* params_create( ) {
	params_t* params = (params_t*) malloc( sizeof( params_t ) );
	params->num = 0;

	return params;
}

void params_parse_qs( params_t* params, char* raw, int size ) {
	param_t* param;
	int len;

	char* tmp;
	while ( size > 0 ) {
		param = &params->list[ params->num ];

		// name
		tmp = strstr( raw, "=" );

		if ( !tmp ) {
			break;
		}

		len = tmp - raw;
		if ( len > sizeof( param->name ) )
			len = sizeof( param->name );

		url_decode( raw, param->name, len );
		size -= ( tmp + 1 ) - raw;
		raw = tmp + 1;
		if ( size <= 0 )
			break;

		// value
		tmp = strstr( raw, "&" );
		if ( !tmp || tmp - raw > size ) tmp = raw + size;

		len = tmp - raw;

		param->value = (char*) malloc( len + 1 );
		url_decode( raw, param->value, len );

		size -= tmp - raw;
		raw = tmp;
		if ( *tmp == '&' ) {
			size--;
			raw++;
		}

		params->num++;
	}
}

param_t* params_get_param( params_t* params, const char* name ) {
	int i;

	for( i = 0; i < params->num; i++ ) {
		if ( strcmp( params->list[ i ].name, name ) == 0 )
			return &params->list[ i ];
	}

	return NULL;
}

void params_free( params_t* params ) {
	int i;
	param_t* param;

	for ( i = 0; i < params->num; i++ ) {
		param = &params->list[ i ];
		free( param->value );
	}

	free( params );
}
