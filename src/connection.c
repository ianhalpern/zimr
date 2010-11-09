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

#include "connection.h"

// Reference: http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
const char httpstatus[][ 40 ] = {
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
	char* ptr,* tmp,* start = raw, urlbuf[ sizeof( connection->request.full_url ) ];

	connection->status = CONN_STATUS_RECEIVED;
	connection->website = website;
	connection->sockfd  = sockfd;
	connection->udata   = NULL;
	connection->request.post_body = NULL;
	connection->request.post_body_len = 0;
	connection->request.charset = NULL;
	connection->request.params = params_create();
	connection->response.headers.num = 0;
	connection->sending_error = false;

	memcpy( &connection->ip, raw, sizeof( connection->ip ) );
	raw += sizeof( connection->ip );
	strncpy( connection->hostname, raw, sizeof( connection->hostname ) );
	raw += strlen( raw ) + 1;

	// type /////////////////////////////////
	if ( startswith( raw, HTTP_GET ) ) {
		connection->request.type = HTTP_GET_TYPE;
		raw += strlen( HTTP_GET );
	} else if ( startswith( raw, HTTP_POST ) ) {
		connection->request.type = HTTP_POST_TYPE;
		raw += strlen( HTTP_POST );
	} else goto fail;
	raw++; // skip over space
	////////////////////////////////////////

	// url /////////////////////////////////
	if ( !( tmp = strstr( raw, HTTP_HDR_ENDL ) ) )
		goto fail;

	if ( !( ptr = strnstr( raw, "?", ( tmp - raw ) ) )
	  && !( ptr = strnstr( raw, " ", ( tmp - raw ) ) ) )
		goto fail;

	url_decode( urlbuf, raw, ptr - raw );
	normalize( connection->request.full_url, urlbuf );

	while ( startswith( connection->request.full_url, "../" ) ) {
		strcpy( urlbuf, connection->request.full_url + 3 );
		strcpy( connection->request.full_url, urlbuf );
	}

	// Skip over websites leading url.
	// if website is "example.com/test/" and url is "/test/hi" "/test" gets removed
	connection->request.url = strchr( connection->website->url, '/' );
	if ( connection->request.url )
		connection->request.url = &connection->request.full_url[
		  ( connection->website->url + strlen( connection->website->url ) ) - connection->request.url ];
	else
		connection->request.url = connection->request.full_url;

	if ( *connection->request.url == '/' ) {
		connection->request.url++;
	}
	////////////////////////////////////////

	// parse qstring params ////////////////
	tmp = strstr( raw, HTTP_HDR_ENDL );
	if ( ptr[ 0 ] == '?' ) {
		raw = ptr + 1;
		if ( !( ptr = strnstr( raw, " ", tmp - raw ) ) )
			goto fail;
		params_parse_qs( &connection->request.params, raw, ptr - raw );
	}

	raw = ptr + 1;
	////////////////////////////////////////

	// version /////////////////////////////
	if ( !startswith( raw, "HTTP/" ) )
		goto fail;
	raw += 5; // skips "HTTP/"
	if ( !( ptr = strstr( raw, HTTP_HDR_ENDL ) ) )
		goto fail;
	strncpy( connection->http_version, raw, SMALLEST( ptr - raw, sizeof( connection->http_version ) - 1 ) );
	connection->http_version[ SMALLEST( ptr - raw, sizeof( connection->http_version ) - 1 ) ] = '\0';
	raw = ptr + strlen( HTTP_HDR_ENDL );
	////////////////////////////////////////

	// headers /////////////////////////////
	connection->request.headers = headers_parse( raw );
	////////////////////////////////////////

	// cookies /////////////////////////////
	header_t* header = headers_get_header( &connection->request.headers, "Cookie" );
	if ( header )
		connection->cookies = cookies_parse( header->value );
	else
		connection->cookies = cookies_parse( "" );
	////////////////////////////////////////

	// body ////////////////////////////////
	if ( connection->request.type == HTTP_POST_TYPE
	 && ( ptr = strstr( raw, HTTP_HDR_ENDL HTTP_HDR_ENDL ) ) != NULL ) {
		ptr += strlen( HTTP_HDR_ENDL HTTP_HDR_ENDL );
		connection->request.post_body_len = size - (long) ( ptr - start );
		connection->request.post_body = (char*) malloc( connection->request.post_body_len + 1 );
		memset( connection->request.post_body, 0, connection->request.post_body_len + 1 );
		strncpy( connection->request.post_body, ptr, connection->request.post_body_len );
		header_t* header = headers_get_header( &connection->request.headers, "Content-Type" );
		if ( header && startswith( header->value, "application/x-www-form-urlencoded" ) ) {
			params_parse_qs( &connection->request.params, connection->request.post_body, connection->request.post_body_len );
		}

		char* charset;
		if ( header && ( charset = strstr( header->value, "charset=" ) ) ) {
			connection->request.charset = charset + 8;
		//	printf( "charset=%s\n", connection->request.charset );
		} else {
			// ISO-8859-1 is the default charset for http defined by RFC
			connection->request.charset = "ISO-8859-1";
		}
	}
	////////////////////////////////////////

	return connection;

fail:
	connection_free( connection );
	return NULL;
}

void connection_free( connection_t* connection ) {
	params_free( &connection->request.params );
	if ( connection->request.post_body )
		free( connection->request.post_body );
	free( connection );
}

const char* response_status( short c ) {
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
