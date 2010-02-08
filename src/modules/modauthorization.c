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

#include <crypt.h>
#include "zimr.h"

time_t htpasswd_mtime;

typedef struct {
	char name[ 64 ];
	char passwd[ 64 ];
	char salt[ 3 ];
} auth_user_t;

auth_user_t auth_users[ 100 ];

unsigned char base64_decode_digit( char c ) {
	switch( c ) {
		case '=' : return 0;
		case '+' : return 62;
		case '/' : return 63;
		default  :
			if ( isdigit( c ) ) return c - '0' + 52;
			else if ( islower( c ) ) return c - 'a' + 26;
			else if ( isupper( c ) ) return c - 'A';
	}
	return 0xff;
}

const char* base64_decode( char* buf, char* s ) {
	char* p;
	unsigned n = 0, i = 0;

	for ( p = s; *p; p++ ) {
		//printf( "%d\n", base64_decode_digit( *p ) );
		n = 64 * n + base64_decode_digit( *p );
		if ( !( ( p - s + 1 ) % 4 ) ) {
			buf[ i++ ] = n >> 16;
			buf[ i++ ] = ( n - ( n >> 16 << 16 ) ) >> 8;
			buf[ i++ ] = n - ( n >> 8 << 8 );
			n = 0;
		}
	}

	buf[ i ] = 0;

	return buf;
}

bool read_htpasswd_file() {
	char buf[ 256 ];
	char* ptr;
	FILE *fp;
	int i = 0;
	memset( &auth_users[ i ], 0, sizeof( auth_users[ i ] ) );

	puts( "reading htpasswd file" );

	if ( ( fp = fopen( ".htpasswd", "r" ) ) == NULL ) {
		fprintf( stderr, "Authentication Module: could not open .htpasswd file.\n" );
		return false;
	}

	for ( ; fgets( buf, sizeof( buf ), fp ) != NULL; i++ ) {
		ptr = strchr( buf, ':' );
		strncpy( auth_users[ i ].name, buf, ptr - buf );
		strncpy( auth_users[ i ].passwd, ptr + 1, strlen( buf ) - ( ptr - buf ) );
		chomp( auth_users[ i ].passwd );
		strncpy( auth_users[ i ].salt, auth_users[ i ].passwd, 2 );
		memset( &auth_users[ i + 1 ], 0, sizeof( auth_users[ i + 1 ] ) );
	}

	return true;
}

bool htpasswd_file_was_modified() {
	struct stat statbuf;
	if ( stat( ".htpasswd", &statbuf ) == -1 )
		return false;

	if ( htpasswd_mtime == statbuf.st_mtime )
		return false;

	htpasswd_mtime = statbuf.st_mtime;

	return true;
}

void modzimr_init() {
	memset( &htpasswd_mtime, 0, sizeof( htpasswd_mtime ) );
	memset( &auth_users[ 0 ], 0, sizeof( auth_users[ 0 ] ) );
}

void* modzimr_website_init( website_t* website, int argc, char* argv[] ) {
	zimr_website_insert_ignored_regex( website, ".*\\.htpasswd$" );
	return NULL;
}

void modzimr_connection_new( connection_t* connection, void* udata ) {
	char buf[ 256 ];

	header_t* auth_header = headers_get_header( &connection->request.headers, "Authorization" );

	if ( auth_header ) {

		if ( !startswith( auth_header->value, "Basic " ) ) {
			fprintf( stderr, "Authentication Module: incorrect basic authorization format.\n" );
			goto not_auth;
		}

		base64_decode( buf, auth_header->value + ( sizeof( "Basic " ) - 1 ) );
		char* ptr = strchr( buf, ':' );

		if ( !ptr ) goto not_auth;

		*ptr = 0;
		ptr++;

		int j;
		for ( j = 0; j < 2; j++ ) {
			int i;
			for ( i = 0; *(bool*)&auth_users[ i ]; i++ ) {
				if ( strcmp( auth_users[ i ].name, buf ) == 0
				 && strcmp( auth_users[ i ].passwd, crypt( ptr, auth_users[ i ].salt ) ) == 0 )
					return;
			}

			if ( j == 1 || !htpasswd_file_was_modified() )
				goto not_auth;

			read_htpasswd_file();
		}

	}

not_auth:
	headers_set_header( &connection->response.headers, "WWW-Authenticate", "Basic realm=\"Secure Area\"" );
	zimr_connection_send_error( connection, 401 );
}
