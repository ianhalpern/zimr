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

#include "connection.h"

// Reference: http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
const char httpstatus[ ][ 40 ] = {
"100 Continue",
"101 Switching Protocols",
"200 OK",
"201 Created",
"202 Accepted",
"203 Non-Authoritative Information",
"204 No Content",
"205 Reset Content",
"206 Partial Content",
"300 Multiple Choices",
"301 Moved Permanently",
"302 Found",
"303 See Other",
"304 Not Modified",
"305 Use Proxy",
"307 Temporary Redirect",
"400 Bad Request",
"401 Unauthorized",
"403 Forbidden",
"404 Not Found",
"405 Method Not Allowed",
"406 Not Acceptable",
"407 Proxy Authentication Required",
"408 Request Timeout",
"409 Conflict",
"410 Gone",
"411 Length Required",
"412 Precondition Failed",
"413 Request Entity Too Large",
"414 Request-URI Too Long",
"415 Unsupported Media Type",
"416 Requested Range Not Satisfiable",
"417 Expectation Failed",
"500 Internal Server Error",
"501 Not Implemented",
"502 Bad Gateway",
"503 Service Unavailable",
"504 Gateway Timeout",
"505 HTTP Version Not Supported"
};

connection_t* connection_create( website_t* website, int sockfd, char* raw, size_t size ) {
	connection_t* connection = (connection_t*) malloc( sizeof( connection_t ) );
	char* tmp,* start = raw, urlbuf[ sizeof( connection->request.url ) ];
	connection->website = website;
	connection->sockfd  = sockfd;
	connection->udata   = NULL;
	connection->request.post_body = NULL;
	connection->request.params = params_create( );

	memcpy( &connection->ip, raw, sizeof( connection->ip ) );
	raw += sizeof( connection->ip );
	strncpy( connection->hostname, raw, sizeof( connection->hostname ) );
	//printf( "%s %s %d\n", connection->hostname, inet_ntoa( connection->ip ), strlen( raw ) );
	raw += strlen( raw ) + 1;
	// type
	if ( startswith( raw, HTTP_GET ) ) {
		connection->request.type = HTTP_GET_TYPE;
		raw += strlen( HTTP_GET ) + 1;
	} else if ( startswith( raw, HTTP_POST ) ) {
		connection->request.type = HTTP_POST_TYPE;
		raw += strlen( HTTP_POST ) + 1;
	} else return connection;

	//printf( "type: \"%s\"\n", RTYPE( r.type ) );

	// url
	raw++; // skip over forward slash
	tmp = strstr( raw, "?" );
	if ( !tmp || tmp > strstr( raw, HTTP_HDR_ENDL ) ) tmp = strstr( raw, " " );
	url_decode( raw, urlbuf, tmp - raw );

	// Skip over websites leading url.
	// if website is "example.com/test/" and url is "/test/hi" "/test" gets removed
	char* ptr = strstr( connection->website->url, "/" );
	int i = 0;
	if ( ptr ) {
		i = ( strlen( connection->website->url ) + connection->website->url ) - ptr - 1;
	}

	normalize( connection->request.url, &urlbuf[ i ] );

	while ( startswith( connection->request.url, "../" ) ) {
		strcpy( urlbuf, connection->request.url + 3 );
		strcpy( connection->request.url, urlbuf );
	}
	//printf( "url: \"%s\"\n", r.url );

	// parse qstring params
	if ( tmp[ 0 ] == '?' ) {
		raw = tmp + 1;
		tmp = strstr( tmp, " " );
		params_parse_qs( connection->request.params, raw, tmp - raw );
		raw = tmp + 1;
	} else
		raw = strstr( tmp, " " ) + 1;

	// version
	raw += 5; // skips "HTTP/"
	tmp = strstr( raw, HTTP_HDR_ENDL );
	memcpy( connection->http_version, raw, tmp - raw + 1 );
	connection->http_version[ tmp - raw ] = '\0';
	raw = tmp + 2;

	// headers
	connection->request.headers = headers_parse( raw );

	// cookies
	header_t* header = headers_get_header( &connection->request.headers, "Cookie" );
	if ( header )
		connection->cookies = cookies_parse( header->value );
	else
		connection->cookies = cookies_parse( "" );

	// body
	if ( connection->request.type == HTTP_POST_TYPE ) {
		if ( ( tmp = strstr( raw, HTTP_HDR_ENDL HTTP_HDR_ENDL ) ) != NULL ) {
			connection->request.post_body = (char*) malloc( size - (long) ( tmp - start ) );
			memset( connection->request.post_body, 0, size - (long) ( tmp - start ) );
			tmp += strlen( HTTP_HDR_ENDL HTTP_HDR_ENDL );
			strncpy( connection->request.post_body, tmp, size - (long) ( tmp - start ) );
			header_t* header = headers_get_header( &connection->request.headers, "Content-Type" );

			if ( strcmp( header->value, "application/x-www-form-urlencoded" ) == 0 ) {
				params_parse_qs( connection->request.params, connection->request.post_body, size - (long) ( tmp - start ) );
			}
		}
	}

	connection->response.headers.num = 0;

	return connection;
}

void connection_free( connection_t* connection ) {
	params_free( connection->request.params );
	if ( connection->request.post_body )
		free( connection->request.post_body );
	free( connection );
}

const char *response_status( short c ) {
	char status[ 4 ];
	int i;
	sprintf( status, "%d", c );

	for ( i = 0; i < sizeof( httpstatus ) / 40; i++ ) {
		if ( startswith( (char*) httpstatus[ i ], status ) )
			return httpstatus[ i ];
	}

	return NULL;
}

void response_set_status( response_t* response, short status ) {
	response->http_status = status;
}
