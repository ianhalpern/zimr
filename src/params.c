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

void params_parse_qs( list_t* params, char* raw, int size, int type ) {
	param_t* param;
	int len;

	char* name_ptr,* val_ptr;
	while ( size > 0 ) {
		unsigned char name[PARAMS_NAME_MAX_LEN];
		char* value;
		name_ptr = raw;
		val_ptr = raw;

		// name
		while ( ( val_ptr - raw ) < size ) {
			if ( *val_ptr != '&' )
				val_ptr++;
			else break;
			if ( *name_ptr != '=' )
				name_ptr++;
		}

		len = name_ptr - raw;

		if ( len >= PARAMS_NAME_MAX_LEN ) break;
		url_decode( name, raw, len );

		int i = 0;
		for ( ; i < len; i++ ) {
			if ( !name[i] || name[i] > 127 )
				break;
		}
		if ( i != len && name[i] ) {
			//Not Ascii
			break;
		}

		if ( *name_ptr == '=' )
			name_ptr++;

		size -= name_ptr - raw;
		raw = name_ptr;

		// value
		//if ( !name_ptr || name_ptr - raw > size ) name_ptr = raw + size;
		len = val_ptr - raw;
		value = (char*) malloc( len + 1 );
		url_decode( value, raw, len );

		if ( *val_ptr == '&' ) {
			val_ptr++;
		}
		size -= val_ptr - raw;
		raw = val_ptr;

		param = (param_t*) malloc( sizeof( param_t ) );
		strcpy( param->name, name );
		param->value = value;
		param->value_len = len;
		param->type = type;
		param->val_alloced = true;
		list_append( params, param );
	}
}

void params_parse_multiparts( list_t* params, request_t* multiparts[], int type ) {
	param_t* param;
	header_t* header;
	int i = -1;

	//Content-Disposition: form-data; name="fileField"; filename="file-upload.txt"
	while ( multiparts[++i] ) {
		header = headers_get_header( &multiparts[i]->headers, "Content-Disposition" );
		if ( !header || strcmp( headers_get_header_attr( header, NULL ), "form-data" ) != 0 ) continue;

		char* name = headers_get_header_attr( header, "name" );
		if ( !name ) continue;

		param = (param_t*) malloc( sizeof( param_t ) );
		strcpy( param->name, name );
		param->value = multiparts[i]->post_body;
		param->value_len = multiparts[i]->post_body_len;
		param->type = type;
		list_append( params, param );
		param->val_alloced = false;
	}
}

size_t params_qs_len( list_t* params ) {
	size_t l = 0;
	int i;
	for ( i = 0 ; i < list_size( params ); i++ ) {
		param_t* param = list_get_at( params, i );
		l += PARAMS_NAME_MAX_LEN + param->value_len + 2;
	}
	return l;
}

char* params_gen_qs( list_t* params, char* qs ) {
	int i;
	*qs = 0;
	for( i = 0; i < list_size( params ); i++ ) {
		if ( i ) strcat( qs, "&" );
		param_t* param = list_get_at( params, i );
		strcat( qs, param->name );
		strcat( qs, "=" );
		strcat( qs, param->value );
	}

	return qs;
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

param_t* params_set_param( list_t* params, const char* name, char* value ) {
	if ( value == NULL )
		return NULL;
	int i;
	for( i = 0; i < list_size( params ); i++ ) {
		param_t* param = list_get_at( params, i );
		if ( strcmp( param->name, name ) == 0 ) {
			if ( param->val_alloced )
				free( param->value );
			param->value = strdup( value );
			param->val_alloced = true;
			return param;
		}
	}

	param_t* param = (param_t*) malloc( sizeof( param_t ) );
	strcpy( param->name, name );
	param->value = strdup( value );
	param->value_len = strlen( value );
	param->type = PARAM_TYPE_CUSTOM;
	param->val_alloced = true;
	list_append( params, param );

	return param;
}

void params_free( list_t* params ) {
	param_t* param;

	int i;
	for ( i = 0; i < list_size( params ); i++ ) {
		param = list_get_at( params, i );
		//free( param->name );
		param_free( param );
	}

	list_destroy( params );
}

void param_free( param_t* param ) {
	if ( param->val_alloced )
		free( param->value );
	free( param );
}
