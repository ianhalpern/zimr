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

#include "podora.h"
#include "pfildes.h"

static int res_fd;

const char* podora_version( ) {
	return PODORA_VERSION;
}

const char* podora_build_date( ) {
	return BUILD_DATE;
}

int podora_init( ) {
	int pid, res_filenum;

	podora_read_server_info( &pid, &res_filenum );

	if ( ( res_fd = pcom_connect( pid, res_filenum ) ) < 0 ) {
		return 0;
	}

	pfd_register_type( PFD_TYPE_PCOM, PFD_TYPE_HDLR &podora_request_handler );
	pfd_register_type( PFD_TYPE_FILE, PFD_TYPE_HDLR &podora_file_handler );

	return 1;
}

void podora_shutdown( ) {
	while ( website_get_root( ) ) {
		podora_website_destroy( website_get_root( ) );
	}
}

int podora_read_server_info( int* pid, int* res_fn ) {
	int fd;

	if ( ( fd = open( PD_INFO_FILE, O_RDONLY ) ) < 0 ) {
		perror( "[fatal] read_podora_info: open() failed" );
		return 0;
	}

	read( fd, pid, sizeof( *pid ) );
	read( fd, res_fn, sizeof( *res_fn ) );

	close( fd );

	return 1;
}

void podora_start( ) {
	pfd_start( );
}

void podora_send_cmd( int cmd, int key, void* message, int size ) {
	pcom_transport_t* transport = pcom_open( res_fd, PCOM_WO, cmd, key );
	pcom_write( transport, message, size );
	pcom_close( transport );
}

website_t* podora_website_create( char* url ) {
	int req_fd;

	if ( ( req_fd = pcom_create( ) ) < 0 ) {
		return NULL;
	}

	website_t* website;
	website_data_t* website_data;

	if ( !( website = website_add( randkey( ), url ) ) )
		return NULL;

	website_data = (website_data_t*) malloc( sizeof( website_data_t ) );
	website->data = (void*) website_data;

	website_data->req_fd = req_fd;
	website_data->pubdir = strdup( "./" );
	website_data->status = WS_STATUS_STOPPED;

	pfd_set( website_data->req_fd, PFD_TYPE_PCOM, website );

	return website;
}

void podora_website_destroy( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->data;

	podora_website_stop( website );

	if ( website_data->pubdir )
		free( website_data->pubdir );

	pfd_clr( website_data->req_fd );
	close( website_data->req_fd );

	free( website_data );
	website_remove( website );
}

int podora_website_start( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->data;

	if ( website_data->status == WS_STATUS_STARTED )
		return 0;

	int pid = getpid( );
	char message[ sizeof( int ) * 2 + strlen( website->url ) + 1 ];

	memcpy( message, &pid, sizeof( int ) );
	memcpy( message + sizeof( int ), &website_data->req_fd, sizeof( int ) );
	memcpy( message + sizeof( int ) * 2, website->url, strlen( website->url ) + 1 );

	podora_send_cmd( WS_START_CMD, website->id, message, sizeof( message ) );

	website_data->status = WS_STATUS_STARTED;

	return 1;
}

int podora_website_stop( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->data;

	if ( website_data->status == WS_STATUS_STOPPED )
		return 0;

	podora_send_cmd( WS_STOP_CMD, website->id, NULL, 0 );

	return 1;
}

void podora_request_handler( int req_fd, website_t* website ) {
	int fd;

	pcom_transport_t* transport = pcom_open( req_fd, PCOM_RO, 0, 0 );
	pcom_read( transport );

	printf( "%s\n", (char*) transport->message );

	if ( ( fd = open( "in/1.in", O_RDONLY ) ) < 0 ) {
		return;
	}

	pfd_set( fd, PFD_TYPE_FILE, pcom_open( res_fd, PCOM_WO, transport->header->id, website->id ) );

	pcom_close( transport );
}

void podora_file_handler( int fd, pcom_transport_t* transport ) {
	int n;
	char buffer[ 2048 ];

	if ( ( n = read( fd, buffer, sizeof( buffer ) ) ) ) {
		if ( !pcom_write( transport, buffer, n ) ) {
			fprintf( stderr, "[error] file_handler: pcom_write() failed\n" );
			return;
		}
	} else {
		pcom_close( transport );
		pfd_clr( fd );
		close( fd );
	}
}
