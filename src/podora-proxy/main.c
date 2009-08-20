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

#include "ptransport.h"
#include "pfildes.h"
#include "website.h"
#include "psocket.h"
#include "daemon.h"
#include "pderr.h"
#include "pcnf.h"

#define DAEMON_NAME "podora-proxy"

typedef struct {
	psocket_t* socket;
} website_data_t;

typedef struct {
	int fd;
	struct sockaddr_in addr;
	void* udata;
} conn_info_t;

typedef struct {
	int website_sockfd;
	ptransport_t* transport;
	int request_type;
	int postlen;
} req_info_t;

void internal_connection_listener( int sockfd, void* udata );
void external_connection_listener( int sockfd, void* udata );
void internal_connection_handler( int sockfd, conn_info_t* conn_info );
void external_connection_handler( int sockfd, conn_info_t* conn_info );

void command_handler( int sockfd, ptransport_t* transport );

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
"\nUsage: podora-proxy [OPTIONS] {start|stop|restart}\n\
	-h --help\n\
	--no-daemon\n\
	--no-lockfile\n\
	--force\n\
"
	);
}

int main( int argc, char* argv[ ] ) {
	int ret = EXIT_SUCCESS;
	int make_daemon = 1, daemon_flags = 0;

	printf( "Podora Proxy " PODORA_VERSION " (" BUILD_DATE ")\n" );

	///////////////////////////////////////////////
	// parse command line options
	int i;
	for ( i = 1; i < argc; i++ ) {
		if ( argv[ i ][ 0 ] != '-' ) break;
		if ( strcmp( argv[ i ], "--no-daemon" ) == 0 )
			make_daemon = 0;
		else if ( strcmp( argv[ i ], "--force" ) == 0 ) {
			daemon_flags |= D_NOLOCKCHECK;
		} else if ( strcmp( argv[ i ], "--no-lockfile" ) == 0 ) {
			daemon_flags |= D_NOLOCKFILE;
		} else {
			if ( strcmp( argv[ i ], "--help" ) != 0 && strcmp( argv[ i ], "-h" ) != 0 )
				printf( "\nUnknown Option: %s\n", argv[ i ] );
			print_usage( );
			exit( 0 );
		}
	}

	if ( i == argc ) {
		print_usage( );
		exit( 0 );
	}

	// parse command line commands
	for ( i = i; i < argc; i++ ) {
		if ( strcmp( argv[ i ], "start" ) == 0 ) break;
		else if ( strcmp( argv[ i ], "stop" ) == 0 ) {
			if ( !daemon_stop( ) ) exit( EXIT_FAILURE );
			exit( EXIT_SUCCESS );
		} else if ( strcmp( argv[ i ], "restart" ) == 0 ) {
			if ( !daemon_stop( ) ) exit( EXIT_FAILURE );
			break;
		} else {
			printf( "\nUnknown Command: %s\n", argv[ i ] );
			print_usage( );
			exit( 0 );
		}
	}
	/////////////////////////////////////////////////

	// signal handling is also set up in daemon.c
	// but this signal handler will log what signal
	// was received
	signal( SIGHUP,  signal_handler );
	signal( SIGTERM, signal_handler );
	signal( SIGINT,  signal_handler );
	signal( SIGQUIT, signal_handler );

	daemon_flags |= D_KEEPSTDIO;

	// Setup syslog logging - see SETLOGMASK(3)
#if defined(DEBUG)
	daemon_flags |= D_NOCD;
	setlogmask( LOG_UPTO( LOG_DEBUG ) );
	openlog( DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER );
#else
	setlogmask( LOG_UPTO( LOG_INFO ) );
	openlog( DAEMON_NAME, LOG_CONS, LOG_USER );
#endif

	syslog( LOG_INFO, "starting up" );

	// daemonize
	if ( make_daemon ) {
		if ( !daemon_start( daemon_flags ) ) {
			ret = EXIT_FAILURE;
			goto quit;
		}
	}

	// set the fle handlers for the different types of file desriptors
	// used in pfd_select()
	pfd_register_type( PFD_TYPE_INT_LISTEN,    PFD_TYPE_HDLR internal_connection_listener );
	pfd_register_type( PFD_TYPE_EXT_LISTEN,    PFD_TYPE_HDLR external_connection_listener );
	pfd_register_type( PFD_TYPE_INT_CONNECTED, PFD_TYPE_HDLR internal_connection_handler );
	pfd_register_type( PFD_TYPE_EXT_CONNECTED, PFD_TYPE_HDLR external_connection_handler );

	psocket_t* socket = psocket_open( inet_addr( PD_PROXY_ADDR ), PD_PROXY_PORT );
	if ( !socket ) {
		ret = EXIT_FAILURE;
		goto quit;
	}

	pfd_set( socket->sockfd, PFD_TYPE_INT_LISTEN, NULL );

#ifndef DEBUG
	daemon_redirect_stdio( );
#endif

	// starts a select() loop and calls
	// the associated file descriptor handlers
	// when they are ready to read
	while ( pfd_select( 0 ) ); // The loop is only broken by interrupt

quit:
	// cleanup

	if ( ret == EXIT_FAILURE ) {
		puts( "exit: failure" );
		syslog( LOG_INFO, "exit: failure" );
	} else {
		puts( "exit: success" );
		syslog( LOG_INFO, "exit: success" );
	}

	closelog( );
	return ret;
}

char* get_status_message( char* buffer, int size ) {
	memset( buffer, 0, size );

	strcat( buffer, "\nSockets:\n" );
	psocket_t* socket = psocket_get_root( );

	while ( socket ) {
		strcat( buffer, "  " );
		inet_ntop( AF_INET, &socket->addr, buffer + strlen( buffer ), INET_ADDRSTRLEN );
		sprintf( buffer + strlen( buffer ), ":%d (%d website(s) connected)\n", socket->portno, socket->n_open );
		socket = socket->next;
	}

	strcat( buffer, "\nWebsites:\n" );
	website_t* website = website_get_root( );

	while ( website ) {
		strcat( buffer, "  " );
		sprintf( buffer + strlen( buffer ), "%s\n", website->url );
		website = website->next;
	}

	return buffer;
}

char* get_url_from_http_header( char* raw, char* url, int size ) {
	char* ptr,* ptr2;

	if ( !( ptr = strnstr( raw, "Host: ", size ) ) )
		return NULL;

	ptr += 6;

	ptr2 = strnstr( ptr, HTTP_HDR_ENDL, size - ( ptr - raw ) - 1 );
	strncpy( url, ptr, ptr2 - ptr );

	*( url + ( ptr2 - ptr ) ) = 0;

	ptr = strnstr( raw, " ", size - 1 );
	ptr++;

	ptr2 = strnstr( ptr, " ", size - ( ptr - raw ) - 1 );
	strncat( url, ptr, ptr2 - ptr );

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

int remove_website( int sockfd ) {
	website_t* website;

	if ( !( website = website_get_by_sockfd( sockfd ) ) ) {
		syslog( LOG_WARNING, "stop_website: tried to stop a nonexisting website" );
		return 0;
	}

	syslog( LOG_INFO, "website: stopping \"http://%s\"", website->url );
	website_data_t* website_data = (website_data_t*) website->udata;

	if ( website_data->socket ) {
		if ( website_data->socket->n_open == 1 ) {
			pfd_clr( website_data->socket->sockfd );
		}
		psocket_close( website_data->socket );
	}

	free( website_data );
	website_remove( website );

	return 1;
}

int start_website( char* url, int sockfd ) {
	website_t* website;
	website_data_t* website_data;

	if ( ( website = website_get_by_url( url ) ) ) {
		syslog( LOG_WARNING, "start_website: tried to add website \"%s\" that already exists", url );
		return 0;
	}

	if ( !( website = website_add( sockfd, url ) ) ) {
		syslog( LOG_ERR, "start_website: website_add() failed starting website \"%s\"", url );
		return 0;
	}

	syslog( LOG_INFO, "website: starting \"http://%s\"", website->url );

	website_data = (website_data_t*) malloc( sizeof( website_data_t ) );
	website->udata = website_data;

	website_data->socket = psocket_open( INADDR_ANY, get_port_from_url( website->url ) );

	if ( !website_data->socket ) {
		syslog( LOG_ERR, "start_website: psocket_open(): %s: %s", pdstrerror( pderrno ), strerror( errno ) );
		remove_website( sockfd );
		return 0;
	}

	pfd_set( website_data->socket->sockfd, PFD_TYPE_EXT_LISTEN, NULL );
	return 1;
}

// Handlers ////////////////////////////////////////////

void general_connection_listener( int sockfd, void* udata, int type ) {
	conn_info_t* conn_info;
	struct sockaddr_in cli_addr;
	unsigned int cli_len = sizeof ( cli_addr );
	int newsockfd;

	// Connection request on original socket.
	if ( ( newsockfd = accept( sockfd, (struct sockaddr *) &cli_addr, &cli_len ) ) < 0 ) {
		syslog( LOG_ERR, "general_connection_listener: accept() failed: %s", strerror( errno ) );
		return;
	}

	// pass the connection information along...
	conn_info = (conn_info_t*) malloc( sizeof( conn_info_t ) );
	conn_info->fd = sockfd;
	conn_info->addr = cli_addr;
	conn_info->udata = NULL;

	pfd_set( newsockfd, type, conn_info );
}

void general_connection_close( int sockfd ) {
	free( pfd_udata( sockfd ) ); // free the conn_info_t object
	pfd_clr( sockfd );
	close( sockfd );
}

void external_connection_listener( int sockfd, void* udata ) {
	general_connection_listener( sockfd, udata, PFD_TYPE_EXT_CONNECTED );
}

void internal_connection_listener( int sockfd, void* udata ) {
	general_connection_listener( sockfd, udata, PFD_TYPE_INT_CONNECTED );
}

void external_connection_handler( int sockfd, conn_info_t* conn_info ) {
	char buffer[ PT_BUF_SIZE ],* ptr;
	memset( &buffer, 0, sizeof( buffer ) );
	int len;

	req_info_t* req_info = conn_info->udata;

	if ( req_info && !website_get_by_sockfd( req_info->website_sockfd ) )
		goto cleanup;

	if ( ( len = read( sockfd, buffer, sizeof( buffer ) ) ) <= 0 ) {
cleanup:
		// cleanup
		if ( req_info ) {
			if ( req_info->transport ) {
				if ( PT_MSG_IS_FIRST( req_info->transport )
				  || !website_get_by_sockfd( req_info->website_sockfd ) ) {
					ptransport_free( req_info->transport );
				} else {
					ptransport_close( req_info->transport );
				}
			}

			free( req_info );
		}

		general_connection_close( sockfd );
		return;
	}

	ptr = buffer;

	if ( !req_info ) {
		/* If req_info is NULL this is the start of a request and the
		   HTTP request headers need to be parsed. */
		req_info = (req_info_t*) malloc( sizeof( req_info_t ) );
		conn_info->udata = req_info;
		req_info->transport = NULL;
		req_info->website_sockfd = -1;
		req_info->postlen = 0;

		struct hostent* hp;
		hp = gethostbyaddr( (char*) &conn_info->addr.sin_addr.s_addr, sizeof( conn_info->addr.sin_addr.s_addr ), AF_INET );

		/* Get HTTP request type */
		if ( startswith( buffer, HTTP_GET ) )
			req_info->request_type = HTTP_GET_TYPE;
		else if ( startswith( buffer, HTTP_POST ) )
			req_info->request_type = HTTP_POST_TYPE;
		else goto cleanup;

		/* Find website for request from HTTP header */
		website_t* website;
		char urlbuf[ PT_BUF_SIZE ];
		if ( !get_url_from_http_header( buffer, urlbuf, sizeof( urlbuf ) ) ) {
			syslog( LOG_WARNING, "external_connection_handler: no url found in http request headers: %s %s",
			  inet_ntoa( conn_info->addr.sin_addr ), hp->h_name );
			goto cleanup;
		}
		if ( !( website = website_get_by_url( urlbuf ) ) ) {
			syslog( LOG_WARNING, "external_connection_handler: no website to service request: %s %s %s",
			  inet_ntoa( conn_info->addr.sin_addr ), hp->h_name, urlbuf );
			goto cleanup;
		}

		/* Set the website id so that furthur sections of this
		   request can check if the website is still alive. */
		req_info->website_sockfd = website->sockfd;

		/* Check the websites socket to make sure the request came in on
		   the right socket. */
		website_data_t* website_data = website->udata;

		if ( website_data->socket->sockfd != conn_info->fd ) {
			syslog( LOG_WARNING, "external_connection_handler: no website to service request: %s %s %s",
			  inet_ntoa( conn_info->addr.sin_addr ), hp->h_name, urlbuf );
			goto cleanup;
		}

		/* Open up a transport to send the request to the corresponding
		   website. The website's id is the internal file descriptor to
		   send the request over and the msgid should be set to the
		   external file descriptor to send the response back to. */
		req_info->transport = ptransport_open( website->sockfd, PT_WO, sockfd );

		// Write the ip address and hostname of the request
		ptransport_write( req_info->transport, (void*) &conn_info->addr.sin_addr, sizeof( conn_info->addr.sin_addr ) );
		if ( hp ) ptransport_write( req_info->transport, (void*) hp->h_name, strlen( hp->h_name ) );
		ptransport_write( req_info->transport, (void*) "\0", 1 );

		// If request_type is POST check if there is content after the HTTP header
		char postlenbuf[ 32 ];
		if ( req_info->request_type == HTTP_POST_TYPE && ( ptr = strstr( buffer, "Content-Length: " ) ) ) {
			memcpy( postlenbuf, ptr + 16, (long) strstr( ptr + 16, HTTP_HDR_ENDL ) - (long) ( ptr + 16 ) );
			req_info->postlen = atoi( postlenbuf );
		}

		// Send the whole header to the website
		if ( ( ptr = strstr( buffer, HTTP_HDR_ENDL HTTP_HDR_ENDL ) ) ) {
			ptr += strlen( HTTP_HDR_ENDL HTTP_HDR_ENDL );
			ptransport_write( req_info->transport, (void*) buffer, ( ptr - buffer ) );
		}

		/* If the end of the headers was not found the request was either
		   malformatted or too long, DO NOT send to website. */
		else {
			syslog( LOG_WARNING, "external_connection_handler: headers to long" );
			goto cleanup;
		}
	}

	if ( req_info->request_type == HTTP_POST_TYPE && req_info->postlen ) {
		int left = len - ( ptr - buffer );

		if ( left > req_info->postlen )
			req_info->postlen = left;

		ptransport_write( req_info->transport, (void*) ptr, left );

		req_info->postlen -= left;

	}

	if ( !req_info->postlen ) { /* If there isn't still data coming, */
		ptransport_close( req_info->transport );
		free( req_info );
		conn_info->udata = NULL;
	}

}

void internal_connection_handler( int sockfd, conn_info_t* conn_info ) {
	ptransport_t* transport = ptransport_open( sockfd, PT_RO, 0 );

	if ( !ptransport_read( transport ) ) {

		// if sockfd is a website, we need to remove it
		if ( website_get_by_sockfd( sockfd ) ) {
			remove_website( sockfd );
		}

		general_connection_close( sockfd );
		goto quit;
	}

	if ( transport->header->msgid < 0 ) {

		/* If the msgid is less than zero, it refers to a command and
		   the message should not be routed to a file descriptor. */

		// the entire command must fit inside one transport
		if ( PT_MSG_IS_FIRST( transport ) && PT_MSG_IS_LAST( transport ) )
			command_handler( sockfd, transport );
		else
			syslog( LOG_WARNING, "received a command that is to long...ignoring." );

	} else {

		/* TODO: make sure file descriptor (msgid) is actually
		   associated with the website it came from */

		/* if msgid is zero or greater it refers to a socket file
		   descriptor that the message should be routed to. */
		int n;
		if ( ( n = write( transport->header->msgid, transport->message, transport->header->size ) ) != transport->header->size
		 || PT_MSG_IS_LAST( transport ) ) {
			// clean up external connection
			pfd_clr( transport->header->msgid );
			close( transport->header->msgid );

			if ( n == -1 )
			/* TODO: sent transmission to stop sending data for this
			   external connection, it does not exist anymore. */
				syslog( LOG_ERR, "internal_connection_handler: write failed: %s", strerror( errno ) );
			//goto quit;
		}

	}

quit:
	ptransport_close( transport );
}


void command_handler( int sockfd, ptransport_t* transport ) {
	ptransport_t* response = ptransport_open( sockfd, PT_WO, transport->header->msgid );
	char* message = "";
	char buffer[ 1024 ] = "";

	switch ( transport->header->msgid ) {

		case PD_CMD_WS_START:
			if ( start_website( transport->message, sockfd ) )
				message = "OK";
			else
				message = "FAIL";

			ptransport_write( response, message, strlen( message ) );
			break;

		case PD_CMD_WS_STOP:
			if ( remove_website( sockfd ) )
				message = "OK";
			else
				message = "FAIL";

			ptransport_write( response, message, strlen( message ) );
			break;

		case PD_CMD_STATUS:
			message = get_status_message( buffer, sizeof( buffer ) );
			ptransport_write( response, message, strlen( message ) );
			break;

		default:
			message = "FAIL";
			ptransport_write( response, message, strlen( message ) );
			break;
	}

	ptransport_close( response );
}

////////////////////////////////////////////////////////////////
