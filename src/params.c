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

#include "params.h"

params_t params_parse_qs( char* raw, int size ) {
	params_t params;
	params.num = 0;

	char* tmp = raw;
	while ( size ) {
		//TODO: check string lengths

		// name
		tmp = strstr( raw, "=" );

		if ( !tmp ) {
			break;
		}

		strncpy( params.list[ params.num ].name, raw, tmp - raw );
		params.list[ params.num ].name[ tmp - raw ] = '\0';
		size -= ( tmp + 1 ) - raw;
		raw = tmp + 1;

		// value
		tmp = strstr( raw, "&" );
		if ( !tmp || tmp - raw > size ) tmp = raw + size;
		strncpy( params.list[ params.num ].value, raw, tmp - raw );
		params.list[ params.num ].value[ tmp - raw ] = '\0';

		size -= tmp - raw;
		raw = tmp;
		if ( *tmp == '&' ) {
			size--;
			raw++;
		}

		params.num++;
	}

	return params;
}

param_t* params_get_param( params_t* params, const char* name ) {
	int i;

	for( i = 0; i < params->num; i++ ) {
		if ( strcmp( params->list[ i ].name, name ) == 0 )
			return &params->list[ i ];
	}

	return NULL;
}
