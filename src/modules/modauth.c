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
#include <security/pam_appl.h>

#include "zimr.h"

#define AUTH_HTPASSWD 0x1
#define AUTH_PAM      0x2

time_t htpasswd_mtime;

typedef struct {
	char type;
	char args[5][128];
} auth_info_t;

typedef struct {
	char *user;
	char *pass;
} pam_userpass_t;


typedef struct {
	char name[64];
	char passwd[64];
} auth_user_t;

auth_user_t auth_users[100];

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

static int pam_userpass_conv( int num_msg, const struct pam_message **msg,
  struct pam_response **resp, void *appdata_ptr ) {
	pam_userpass_t *userpass = (pam_userpass_t *)appdata_ptr;
	char *string;
	int i;

	if (!(*resp = malloc(num_msg * sizeof(struct pam_response))))
		return PAM_CONV_ERR;

	for (i = 0; i < num_msg; i++) {
		string = NULL;
		switch (msg[i]->msg_style) {
		case PAM_PROMPT_ECHO_ON:
			string = userpass->user;
		case PAM_PROMPT_ECHO_OFF:
			if (!string)
				string = userpass->pass;
			if (!(string = strdup(string)))
				break;
		case PAM_ERROR_MSG:
		case PAM_TEXT_INFO:
			(*resp)[i].resp_retcode = PAM_SUCCESS;
			(*resp)[i].resp = string;
			continue;
		}

		while (--i >= 0) {
			if (!(*resp)[i].resp) continue;
			memset((*resp)[i].resp, 0, strlen((*resp)[i].resp));
			free((*resp)[i].resp);
			(*resp)[i].resp = NULL;
		}

		free(*resp);
		*resp = NULL;

		return PAM_CONV_ERR;
	}

	return PAM_SUCCESS;
}

bool read_htpasswd_file( const char* htpasswd_filename ) {
	char buf[ 256 ];
	char* ptr;
	FILE *fp;
	int i = 0;
	memset( &auth_users[ i ], 0, sizeof( auth_users[ i ] ) );

	if ( ( fp = fopen( htpasswd_filename, "r" ) ) == NULL ) {
		fprintf( stderr, "Authentication Module: could not open .htpasswd file.\n" );
		return false;
	}

	for ( ; fgets( buf, sizeof( buf ), fp ) != NULL; i++ ) {
		ptr = strchr( buf, ':' );
		strncpy( auth_users[ i ].name, buf, ptr - buf );
		strncpy( auth_users[ i ].passwd, ptr + 1, strlen( buf ) - ( ptr - buf ) );
		chomp( auth_users[ i ].passwd );
		memset( &auth_users[ i + 1 ], 0, sizeof( auth_users[ i + 1 ] ) );
	}

	return true;
}

bool htpasswd_file_was_modified( const char* htpasswd_filename ) {
	struct stat statbuf;
	if ( stat( htpasswd_filename, &statbuf ) == -1 )
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
	auth_info_t* auth_info = malloc( sizeof( auth_info_t ) );

	auth_info->type = AUTH_HTPASSWD;;
	auth_info->args[0][0] = '\0';

	if ( argc ) {
		if ( strcmp( argv[0], "htpasswd" ) == 0 ) auth_info->type = AUTH_HTPASSWD;
		else if ( strcmp( argv[0], "pam" ) == 0 ) auth_info->type = AUTH_PAM;
	}

	switch( auth_info->type ) {

		case AUTH_HTPASSWD:
			if ( argc >=2 ) strcpy( auth_info->args[0], argv[1] );
			else strcpy( auth_info->args[0], ".htpasswd" );
			//zimr_website_insert_ignored_regex( website, ".*\\.htpasswd$" );
			zimr_website_insert_ignored_regex( website, auth_info->args[0] );
			break;

		case AUTH_PAM:
			if ( argc >=2 ) strcpy( auth_info->args[0], argv[1] );
			else strcpy( auth_info->args[0], "login" );
			break;

	}

	return auth_info;
}

void modzimr_connection_new( connection_t* connection, void* udata ) {
	auth_info_t* auth_info = (auth_info_t*) udata;
	char user[256];

	header_t* auth_header = headers_get_header( &connection->request.headers, "Authorization" );

	if ( auth_header ) {

		if ( !startswith( auth_header->value, "Basic " ) ) {
			fprintf( stderr, "Authentication Module: incorrect basic authorization format.\n" );
			goto not_auth;
		}

		base64_decode( user, auth_header->value + ( sizeof( "Basic " ) - 1 ) );
		char* pass = strchr( user, ':' );

		if ( !pass ) goto not_auth;

		*pass = 0;
		pass++;

		int i, j;
		pam_handle_t *pamh;
		pam_userpass_t userpass;
		struct pam_conv conv = {pam_userpass_conv, &userpass};
		int status;
		switch ( auth_info->type ) {

			case AUTH_HTPASSWD:
				for ( j = 0; j < 2; j++ ) {
					for ( i = 0; *(bool*)&auth_users[ i ]; i++ ) {
						if ( strcmp( auth_users[ i ].name, user ) == 0
						 && strcmp( auth_users[ i ].passwd, crypt( pass, auth_users[ i ].passwd ) ) == 0 )
							return;
					}

					if ( j == 1 || !htpasswd_file_was_modified( auth_info->args[0] ) )
						goto not_auth;

					read_htpasswd_file( auth_info->args[0] );
				}
				break;

			case AUTH_PAM:

				userpass.user = user;
				userpass.pass = pass;

				if (pam_start( auth_info->args[0], user, &conv, &pamh ) != PAM_SUCCESS )
					goto not_auth;

				if ( ( status = pam_authenticate( pamh, 0 ) ) != PAM_SUCCESS ) {
					pam_end( pamh, status );
					goto not_auth;
				}

				if ( ( status = pam_acct_mgmt( pamh, 0 ) ) != PAM_SUCCESS ) {
					pam_end( pamh, status );
					goto not_auth;
				}

				if ( pam_end( pamh, PAM_SUCCESS ) != PAM_SUCCESS )
					goto not_auth;

				return;

		}

	}

not_auth:
	headers_set_header( &connection->response.headers, "WWW-Authenticate", "Basic realm=\"Secure Area\"" );
	zimr_connection_send_error( connection, 401 );
}
