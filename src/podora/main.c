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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <netdb.h> // for gethostbyaddr
#include <syslog.h>
#include <errno.h>
#include <unistd.h>

#include "pcom.h"
#include "pfildes.h"
#include "website.h"
#include "psocket.h"
#include "daemonize.h"

#define DAEMON_NAME "podora"

typedef struct {
	int pid;
	int fd;
	psocket_t* socket;
} website_data_t;

typedef struct {
	int fd;
	struct sockaddr_in addr;
} req_info_t;

int res_fd;
int created_podora_info_file = 0; // flag to let remove_podora_info() know if it can remove it

int create_tmpdir( );
void remove_tmpdir( );
int save_podora_info( );
void remove_podora_info( );
int check_if_podora_info_file_exitst( );

void request_accept_handler( int sockfd, void* udata );
void request_handler( int sockfd, req_info_t* req_info );
void response_handler( int fd, pcom_transport_t* transport );

void start_website( pcom_transport_t* transport );
void stop_website( pcom_transport_t* transport );

void remove_website( website_t* website );

void signal_handler( int sig ) {
	switch( sig ) {
		case SIGHUP:
			syslog( LOG_WARNING, "received SIGHUP signal." );
			break;
		case SIGTERM:
			syslog( LOG_WARNING, "received SIGTERM signal." );
			break;
		default:
			syslog( LOG_WARNING, "unhandled signal %d", sig );
			break;
	}
}

void print_usage( ) {
	printf(
"\nUsage:\n\
	-h --help\n\
	--no-daemon\n\
	--force\n\
"
	);
}

int main( int argc, char* argv[ ] ) {
	int ret = EXIT_SUCCESS;
	int make_daemon = 1, daemon_flags = 0;
	int force_start = 0;

	printf( "Podora " PODORA_VERSION " (" BUILD_DATE ")\n" );

	// parse command line options
	int i;
	for ( i = 1; i < argc; i++ ) {
		if ( strcmp( argv[ i ], "--no-daemon" ) == 0 )
			make_daemon = 0;
		else if ( strcmp( argv[ i ], "--force" ) == 0 ) {
			force_start = 1;
		} else {
			if ( strcmp( argv[ i ], "--help" ) != 0 && strcmp( argv[ i ], "-h" ) != 0 )
				printf( "%s\n", argv[ i ] );
			print_usage( );
			exit( 0 );
		}

	}

	// signal handling is also set up in daemonize
	// but this signal handler will log what signal
	// was recieved
	signal( SIGHUP,  signal_handler );
	signal( SIGTERM, signal_handler );
	signal( SIGINT,  signal_handler );
	signal( SIGQUIT, signal_handler );

#if defined(DEBUG)
	daemon_flags |= D_KEEPSTDF;

// Setup syslog logging - see SETLOGMASK(3)
	setlogmask( LOG_UPTO( LOG_DEBUG ) );
	openlog( DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER );
#else
	setlogmask( LOG_UPTO( LOG_INFO ) );
	openlog( DAEMON_NAME, LOG_CONS, LOG_USER );
#endif

	syslog( LOG_INFO, "starting up" );

	// check if podora is already running
	if ( ! force_start && check_if_podora_info_file_exitst( ) ) {
		syslog( LOG_ERR, "daemon already running" );
		printf( "error: podora is already running\n" );
		ret = EXIT_FAILURE;
		goto quit;
	}

	// daemonize
	if ( make_daemon ) {
		syslog( LOG_INFO, "starting the daemonizing process" );
		if ( !daemonize( daemon_flags ) ) {
			ret = EXIT_FAILURE;
			goto quit;
		}
	}

	// create tmp directory to hold named pipe files
	if ( !create_tmpdir( ) ) {
		ret = EXIT_FAILURE;
		goto quit;
	}

	// set the fle handlers for the different types of file desriptors
	// used in pfd_start()'s select() loop
	pfd_register_type( PFD_TYPE_PCOM,          PFD_TYPE_HDLR response_handler );
	pfd_register_type( PFD_TYPE_SOCK_LISTEN,   PFD_TYPE_HDLR request_accept_handler );
	pfd_register_type( PFD_TYPE_SOCK_ACCEPTED, PFD_TYPE_HDLR request_handler );

	if ( ( res_fd = pcom_create( ) ) < 0 ) {
		ret = EXIT_FAILURE;
		goto quit;
	}

	// save pid and res_fd to podora info file in tmp directory
	// this is read by podora websites, providing them with the
	// info to connect
	if ( !save_podora_info( ) ) {
		ret = EXIT_FAILURE;
		goto quit;
	}

	pfd_set( res_fd, PFD_TYPE_PCOM, pcom_open( res_fd, PCOM_RO, 0, 0 ) );

	// starts a select() loop and calls the associated file descriptor handlers
	pfd_start( );

quit:
	// cleanup
	pcom_destroy( res_fd );
	remove_podora_info( );
	remove_tmpdir( );

	if ( ret == EXIT_FAILURE ) {
		printf( "exit: failure\n" );
		syslog( LOG_INFO, "exiting. failure" );
	} else {
		printf( "exit: success\n" );
		syslog( LOG_INFO, "exiting. success" );
	}

	closelog( );
	return ret;
}

int create_tmpdir( ) {
	int r;

	if ( ( r = mkdir( PD_TMPDIR, S_IRWXU ) ) == -1 ) {
		if ( errno != EEXIST ) {
			syslog( LOG_ERR, "could not create %s: %s", PD_TMPDIR, strerror( errno ) );
			return 0;
		}
	}

	if ( ( r = chmod( PD_TMPDIR, S_IRWXU | S_IRWXG | S_IRWXO ) ) == -1 ) {
		syslog( LOG_ERR, "could set permissions for %s: %s", PD_TMPDIR, strerror( errno ) );
		return 0;
	}

	char filename[ 128 ];
	sprintf( filename, PD_TMPDIR "/%d", getpid( ) );

	if ( ( r = mkdir( filename, S_IRWXU ) ) == -1 ) {
		syslog( LOG_ERR, "could not create %s: %s", filename, strerror( errno ) );
		return 0;
	}

	if ( ( r = chmod( filename, 0777 ) ) == -1 ) {
		syslog( LOG_ERR, "could set permissions for %s: %s", filename, strerror( errno ) );
		return 0;
	}

	return 1;
}

void remove_tmpdir( ) {
	char filename[ 128 ];
	sprintf( filename, PD_TMPDIR "/%d", getpid( ) );

	remove( filename );

	remove( PD_TMPDIR );
}

void remove_podora_info( ) {
	if ( created_podora_info_file )
		remove( PD_TMPDIR "/" PD_INFO_FILE );
}

int check_if_podora_info_file_exitst( ) {

	if ( open( PD_TMPDIR "/" PD_INFO_FILE, O_RDONLY ) == -1 ) {
		return 0;
	}

	return 1;
}

int save_podora_info( ) {
	int pid = getpid( ), fd;

	if ( ( fd = creat( PD_TMPDIR "/" PD_INFO_FILE, 0644 ) ) < 0 ) {
		syslog( LOG_ERR, "could set create %s: %s", PD_TMPDIR "/" PD_INFO_FILE, strerror( errno ) );
		return 0;
	}

	created_podora_info_file = 1;

	if ( write( fd, &pid, sizeof( pid ) ) <= 0 || write( fd, &res_fd, sizeof( pid ) ) <= 0 ) {
		syslog( LOG_ERR, "could set write %s: %s", PD_TMPDIR "/" PD_INFO_FILE, strerror( errno ) );
		return 0;
	}
	close( fd );

	return 1;
}

char* get_url_from_http_header( char* url, char* raw ) {
	char* ptr,* ptr2;

	if ( !( ptr = strstr( raw, "Host: " ) ) )
		return NULL;

	ptr += 6;

	ptr2 = strstr( ptr, HTTP_HDR_ENDL );
	strncpy( url, ptr, ptr2 - ptr );

	*( url + ( ptr2 - ptr ) ) = '\0';

	raw = strstr( raw, " " );
	raw++;

	ptr = strstr( raw, " " );

	strncat( url, raw, ptr - raw );

	return url;
}

int get_port_from_url( char* url ) {
	char* ptr1,* ptr2;
	char port_str[ 6 ];

	memset( port_str, 0, sizeof( port_str ) );

	ptr1 = strstr( url, ":" );
	ptr2 = strstr( url, "/" );

	if ( !ptr2 )
		ptr2 = url + strlen( url );

	if ( !ptr1 || ptr2 < ptr1 )
		return HTTP_DEFAULT_PORT;

	ptr1++;

	strncpy( port_str, ptr1, ptr2 - ptr1 );
	return atoi( port_str );
}

void start_website( pcom_transport_t* transport ) {
	website_t* website;
	website_data_t* website_data;

	int pid = *(int*) transport->message;
	int fd = *(int*) ( transport->message + sizeof( int ) );
	char* url = transport->message + sizeof( int ) * 2;

	if ( ( website = website_get_by_url( url ) ) ) {

		// check if website proccess is running
		// remove it if the proccess is not running
		// return with a warning if it does exist
		if ( kill( ((website_data_t*) website->data)->pid, 0 ) == -1 )
			remove_website( website );
		else {
			syslog( LOG_WARNING, "start_website: tried to add website \"%s\" that already exists", url );
			return;
		}

	}

	if ( !( website = website_add( transport->header->key, url ) ) ) {
		syslog( LOG_ERR, "start_website: website_add() failed starting website \"%s\"", url );
		return;
	}

	syslog( LOG_INFO, "website: starting \"http://%s\"", website->url );

	website_data = (website_data_t*) malloc( sizeof( website_data_t ) );

	website->data = website_data;

	website_data->pid = pid;

	if ( !( website_data->fd = pcom_connect( pid, fd ) ) ) {
		syslog( LOG_ERR, "start_website: pcom_connect() failed for website \"%s\"", website->url );
		//stop_website( website->id );
		return;
	}

	website_data->socket = psocket_open( INADDR_ANY, get_port_from_url( website->url ) );

	if ( !website_data->socket ) {
		syslog( LOG_ERR, "start_website: psocket_open() failed" );
		remove_website( website );
		return;
	}

	pfd_set( website_data->socket->sockfd, PFD_TYPE_SOCK_LISTEN, NULL );

}

void stop_website( pcom_transport_t* transport ) {
	website_t* website;

	if ( !( website = website_get_by_key( transport->header->key ) ) ) {
		syslog( LOG_WARNING, "stop_website: tried to stop a nonexisting website" );
		return;
	}

	remove_website( website );
}

void remove_website( website_t* website ) {
	syslog( LOG_INFO, "website: stopping \"http://%s\"", website->url );
	website_data_t* website_data = (website_data_t*) website->data;

	close( website_data->fd );
	if ( website_data->socket ) {
		if ( website_data->socket->n_open == 1 ) {
			pfd_clr( website_data->socket->sockfd );
		}
		psocket_remove( website_data->socket );
	}

	free( website_data );
	website_remove( website );
}

// Handlers ////////////////////////////////////////////

void request_accept_handler( int sockfd, void* udata ) {
	req_info_t* req_info;
	struct sockaddr_in cli_addr;
	unsigned int cli_len = sizeof ( cli_addr );
	int newsockfd;

	/* Connection request on original socket.  */
	if ( ( newsockfd = accept( sockfd, (struct sockaddr *) &cli_addr, &cli_len ) ) < 0 ) {
		syslog( LOG_ERR, "request_accept_handler: accept() failed: %s", strerror( errno ) );
		return;
	}

	req_info = (req_info_t*) malloc( sizeof( req_info_t ) );
	req_info->fd = sockfd;
	req_info->addr = cli_addr;
	pfd_set( newsockfd, PFD_TYPE_SOCK_ACCEPTED, req_info );
}

void request_handler( int sockfd, req_info_t* req_info ) {
	char buffer[ PCOM_MSG_SIZE ];
	int n;
	website_t* website;
	website_data_t* website_data;
	pcom_transport_t* transport;
	int request_type;
	char url[ 256 ];
	char* ptr;
	char postlenbuf[ 16 ];
	int postlen = 0;
	struct hostent* hp;

	memset( &buffer, 0, sizeof( buffer ) );

	if ( ( n = read( sockfd, buffer, sizeof( buffer ) ) ) <= 0 ) {
		if ( !n ) { // if no data was read, the socket has been closed from the other end
			pfd_clr( sockfd );
			close( sockfd );
		} else
			syslog( LOG_ERR, "request_handler: read() failed: %s", strerror( errno ) );
		return;
	}

	if ( startswith( buffer, HTTP_GET ) )
		request_type = HTTP_GET_TYPE;
	else if ( startswith( buffer, HTTP_POST ) )
		request_type = HTTP_POST_TYPE;
	else return;

	ptr = get_url_from_http_header( url, buffer );
	if ( !( website = website_get_by_url( ptr ) ) ) {
		syslog( LOG_WARNING, "request_handler: no website to service request %s", ptr );
		pfd_clr( sockfd );
		close( sockfd );
		return;
	}

	website_data = website->data;

	if ( website_data->socket->sockfd != req_info->fd ) {
		syslog( LOG_WARNING, "request_handler: no website to service request %s", ptr );
		pfd_clr( sockfd );
		close( sockfd );
		return;
	}

	transport = pcom_open( website_data->fd, PCOM_WO, sockfd, website->key );

	hp = gethostbyaddr( (char*) &req_info->addr.sin_addr.s_addr, sizeof( req_info->addr.sin_addr.s_addr ), AF_INET );

	pcom_write( transport, (void*) &req_info->addr.sin_addr, sizeof( req_info->addr.sin_addr ) );
	pcom_write( transport, (void*) hp->h_name, strlen( hp->h_name ) + 1 );

	if ( request_type == HTTP_POST_TYPE && ( ptr = strstr( buffer, "Content-Length: " ) ) ) {
		memcpy( postlenbuf, ptr + 16, (long) strstr( ptr + 16, HTTP_HDR_ENDL ) - (long) ( ptr + 16 ) );
		postlen = atoi( postlenbuf );
	}

	if ( ( ptr = strstr( buffer, HTTP_HDR_ENDL HTTP_HDR_ENDL ) ) ) {
		ptr += strlen( HTTP_HDR_ENDL HTTP_HDR_ENDL );
		pcom_write( transport, (void*) buffer, ( ptr - buffer ) );
	}

	if ( request_type == HTTP_POST_TYPE && postlen ) {
		pcom_write( transport, (void*) ptr, postlen );
	}

	if ( !pcom_flush( transport ) ) {
		pfd_clr( transport->header->id );
		close( transport->header->id );
		if ( errno == EPIPE ) {
			syslog( LOG_WARNING, "request_handler: pipe broken...removing website \"%s\"", website->url );
			remove_website( website );
		}
	}

	free( req_info );
	pcom_free( transport );
}

void command_handler( pcom_transport_t* transport ) {

	switch ( transport->header->id ) {

		case WS_START_CMD:
			start_website( transport );
			break;

		case WS_STOP_CMD:
			stop_website( transport );
			break;

	}

}

void response_handler( int res_fd, pcom_transport_t* transport ) {

	if ( !pcom_read( transport ) )
		return;

	if ( PCOM_MSG_IS_FIRST( transport ) ) {
		if ( transport->header->id < 0 ) {
			command_handler( transport );
			return;
		}
	}

	if ( write( transport->header->id, transport->message, transport->header->size ) != transport->header->size ) {
		syslog( LOG_ERR, "response_handler: write failed: %s", strerror( errno ) );
		return;
	}

	if ( PCOM_MSG_IS_LAST( transport ) ) {
		pfd_clr( transport->header->id );
		close( transport->header->id );
	}
}

////////////////////////////////////////////////////////////////
