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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <netdb.h> // for gethostbyaddr
#include <syslog.h>
#include <errno.h>
#include <unistd.h>

#include "msg_switch.h"
#include "zfildes.h"
#include "website.h"
#include "zsocket.h"
#include "daemon.h"
#include "zerr.h"

#define DAEMON_NAME "zimr-proxy"

#define INLISN 0x01
#define INREAD 0x02
#define INWRIT 0x03
#define EXLISN 0x04
#define EXREAD 0x05
#define EXWRIT 0x06

typedef struct {
	int exlisnfd;
	msg_switch_t* msg_switch;
} website_data_t;

typedef struct {
	int exlisnfd;
	struct sockaddr_in addr;
	int website_sockfd;
	int request_type;
	int postlen;
	msg_packet_t* pending_packet;
} conn_data_t;

conn_data_t* connections[ FD_SETSIZE ];

void inlisn( int sockfd, void* udata );
void exlisn( int sockfd, void* udata );
void exread( int sockfd, void* udata );
void exwrit( int sockfd, void* udata );

void command_handler( msg_switch_t* msg_switch, msg_packet_t* packet );
void cleanup_connection( int sockfd );

void signal_handler( int sig ) {
	switch( sig ) {
		case SIGHUP:
			syslog( LOG_WARNING, "received SIGHUP signal." );
			break;
		case SIGTERM:
			syslog( LOG_WARNING, "received SIGTERM signal." );
			break;
		case SIGPIPE:
			syslog( LOG_WARNING, "received SIGPIPE signal." );
			break;
		default:
			syslog( LOG_WARNING, "unhandled signal %d", sig );
			break;
	}
}

void print_usage( ) {
	printf( "Zimr Proxy " ZIMR_VERSION " (" BUILD_DATE ") - "  ZIMR_WEBSITE "\n" );
	printf(
"\nUsage: zimr-proxy [OPTIONS] {start|stop|restart}\n\
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

	// Trap SIGPIPE
	signal( SIGPIPE, signal_handler );

	FL_SET( daemon_flags, D_KEEPSTDIO );

	// Setup syslog logging - see SETLOGMASK(3)
#if defined(DEBUG)
	FL_SET( daemon_flags, D_NOCD );
	setlogmask( LOG_UPTO( LOG_DEBUG ) );
	openlog( DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER );
#else
	setlogmask( LOG_UPTO( LOG_INFO ) );
	openlog( DAEMON_NAME, LOG_CONS, LOG_USER );
#endif

	// daemonize
	if ( make_daemon ) {
		if ( !daemon_start( daemon_flags ) ) {
			ret = EXIT_FAILURE;
			goto quit;
		}
	}

	syslog( LOG_INFO, "starting up" );

	// set the fle handlers for the different types of file desriptors
	// used in zfd_select()
	zfd_register_type( INLISN, ZFD_R, ZFD_TYPE_HDLR inlisn );
	zfd_register_type( EXLISN, ZFD_R, ZFD_TYPE_HDLR exlisn );
	zfd_register_type( EXREAD, ZFD_R, ZFD_TYPE_HDLR exread );
	zfd_register_type( EXWRIT, ZFD_W, ZFD_TYPE_HDLR exwrit );

	// call any needed library init functions
	website_init( );
	zsocket_init( );
	msg_switch_init( INREAD, INWRIT );

	memset( connections, 0, sizeof( connections ) );

	int sockfd = zsocket( inet_addr( ZM_PROXY_ADDR ), ZM_PROXY_PORT, ZSOCK_LISTEN );
	if ( sockfd == -1 ) {
		ret = EXIT_FAILURE;
		goto quit;
	}

	zfd_set( sockfd, INLISN, NULL );

#ifndef DEBUG
	daemon_redirect_stdio( );
#endif

	// starts a select() loop and calls
	// the associated file descriptor handlers
	// when they are ready to read/write
	while ( zfd_select( 0 ) ); // The loop is only broken by interrupt

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

/*char* get_status_message( char* buffer, int size ) {
	memset( buffer, 0, size );

	strcat( buffer, "\nSockets:\n" );

	int i;
	for ( i = 0; i < list_size( &zsockets ); i++ ) {
		zsocket_t* socket = list_get_at( &zsockets, i );
		strcat( buffer, "  " );
		inet_ntop( AF_INET, &socket->addr, buffer + strlen( buffer ), INET_ADDRSTRLEN );
		sprintf( buffer + strlen( buffer ), ":%d (%d website(s) connected)\n", socket->portno, socket->n_open );
	}

	strcat( buffer, "\nWebsites:\n" );

	for ( i = 0; i < list_size( &websites ); i++ ) {
		website_t* website = list_get_at( &websites, i );
		strcat( buffer, "  " );
		sprintf( buffer + strlen( buffer ), "%s\n", website->url );
	}

	return buffer;
}*/

char* get_url_from_http_header( char* raw, char* url, int size ) {
	char* ptr,* ptr2;

	if ( !( ptr = strnstr( raw, "Host: ", size ) ) )
		return NULL;

	ptr += 6;

	if ( !( ptr2 = strnstr( ptr, HTTP_HDR_ENDL, size - ( ptr - raw ) - 1 ) ) )
		return NULL;

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

int remove_website( msg_switch_t* msg_switch ) {
	website_t* website;

	if ( !( website = website_get_by_sockfd( msg_switch->sockfd ) ) ) {
		syslog( LOG_WARNING, "stop_website: tried to stop a nonexisting website" );
		return 0;
	}

	syslog( LOG_INFO, "website: stopping \"http://%s\"", website->url );
	website_data_t* website_data = (website_data_t*) website->udata;

	int i;
	for ( i = 0; i < FD_SETSIZE; i++ ) {
		if ( connections[ i ] && connections[ i ]->website_sockfd == website->sockfd )
			cleanup_connection( i );
	}

	if ( website_data->exlisnfd != -1 ) {
		if ( zsocket_get_by_sockfd( website_data->exlisnfd )->n_open == 1 ) {
			zfd_clr( website_data->exlisnfd, EXLISN );
		}
		zsocket_close( website_data->exlisnfd );
	}

	msg_switch_destroy( website_data->msg_switch );

	free( website_data );
	website_remove( website );

	return 1;
}

bool start_website( char* url, msg_switch_t* msg_switch ) {
	website_t* website;
	website_data_t* website_data;

	if ( ( website = website_get_by_url( url ) ) ) {
		syslog( LOG_WARNING, "start_website: tried to add website \"%s\" that already exists", url );
		return false;
	}

	if ( !( website = website_add( msg_switch->sockfd, url ) ) ) {
		syslog( LOG_ERR, "start_website: website_add() failed starting website \"%s\"", url );
		return false;
	}

	syslog( LOG_INFO, "website: starting \"http://%s\"", website->url );

	website_data = (website_data_t*) malloc( sizeof( website_data_t ) );
	website->udata = website_data;

	website_data->exlisnfd = zsocket( INADDR_ANY, get_port_from_url( website->url ), ZSOCK_LISTEN );
	website_data->msg_switch = msg_switch;

	if ( website_data->exlisnfd == -1 ) {
		syslog( LOG_ERR, "start_website: zsocket_open(): %s: %s", pdstrerror( zerrno ), strerror( errno ) );
		remove_website( msg_switch );
		return false;
	}

	zfd_set( website_data->exlisnfd, EXLISN, NULL );
	return true;
}

// Handlers ////////////////////////////////////////////

void msg_event_handler( msg_switch_t* msg_switch, msg_event_t event ) {
	switch ( event.type ) {
		case MSG_EVT_NEW:
			zfd_set( event.data.msgid, EXREAD, NULL );
			break;
		case MSG_EVT_COMPLETE:
		case MSG_EVT_DESTROY:
			zfd_clr( event.data.msgid, EXREAD );
			break;
		case MSG_RECV_EVT_LAST:
		case MSG_RECV_EVT_KILL:
			if ( event.data.msgid >= 0 )
				cleanup_connection( event.data.msgid );
			break;
		case MSG_RECV_EVT_PACK:
			if ( event.data.packet->msgid < 0 ) {
				/* If the msgid is less than zero, it refers to a command and
				   the message should not be routed to a file descriptor. */

				// the entire command must fit inside one transport
				if ( PACK_IS_FIRST( event.data.packet ) && PACK_IS_LAST( event.data.packet ) )
					command_handler( msg_switch, event.data.packet );
				else
					syslog( LOG_WARNING, "received a command that is to long...ignoring." );

			} else {
				/* if msgid is zero or greater it refers to a socket file
				   descriptor that the message should be routed to. */
				connections[ event.data.packet->msgid ]->pending_packet = memdup( event.data.packet, sizeof( msg_packet_t ) );
				zfd_set( event.data.packet->msgid, EXWRIT, NULL );
			}
			break;
		case MSG_RECV_EVT_RESP:
			if ( event.data.resp->status == MSG_PACK_RESP_OK )
				zfd_reset( event.data.resp->msgid, EXREAD );
			else if ( event.data.resp->status == MSG_PACK_RESP_FAIL ) {
				cleanup_connection( event.data.resp->msgid );
			}
			break;
		case MSG_EVT_BUF_FULL:
			zfd_clr( event.data.msgid, EXREAD );
			break;
		case MSG_EVT_BUF_EMPTY:
			zfd_set( event.data.msgid, EXREAD, NULL );
			break;
		case MSG_SWITCH_EVT_IO_FAILED:
			close( msg_switch->sockfd );
			if ( website_get_by_sockfd( msg_switch->sockfd ) )
				remove_website( msg_switch );
			break;
		case MSG_RECV_EVT_FIRST:
		case MSG_SWITCH_EVT_NEW:
		case MSG_SWITCH_EVT_DESTROY:
			break;
	}
}

void inlisn( int sockfd, void* udata ) {
	struct sockaddr_in cli_addr;
	unsigned int cli_len = sizeof( cli_addr );
	int insockfd;

	// Connection request on original socket.
	if ( ( insockfd = accept( sockfd, (struct sockaddr*) &cli_addr, &cli_len ) ) < 0 ) {
		syslog( LOG_ERR, "inlisn: accept() failed: %s", strerror( errno ) );
		return;
	}

	msg_switch_new( insockfd, msg_event_handler );
}

void exlisn( int sockfd, void* udata ) {
	conn_data_t* conn_data;
	struct sockaddr_in cli_addr;
	unsigned int cli_len = sizeof( cli_addr );
	int newsockfd;

	// Connection request on original socket.
	if ( ( newsockfd = accept( sockfd, (struct sockaddr*) &cli_addr, &cli_len ) ) < 0 ) {
		syslog( LOG_ERR, "exlisn: accept() failed: %s", strerror( errno ) );
		return;
	}
	//fcntl( newsockfd, F_SETFL, O_NONBLOCK );

	// pass the connection information along...
	conn_data = (conn_data_t*) malloc( sizeof( conn_data_t ) );
	conn_data->exlisnfd = sockfd;
	conn_data->addr = cli_addr;
	conn_data->website_sockfd = -1;
	conn_data->request_type = 0;
	conn_data->postlen = 0;
	conn_data->pending_packet = NULL;

	connections[ newsockfd ] = conn_data;

	zfd_set( newsockfd, EXREAD, NULL );
}

void cleanup_connection( int sockfd ) {
	conn_data_t* conn_data = connections[ sockfd ];

	zfd_clr( sockfd, EXREAD );
	zfd_clr( sockfd, EXWRIT );
	close( sockfd );

	if ( conn_data->pending_packet )
		fprintf( stderr, "cleanup_connection(): conn_dat->pending_packet is not null!" );
	free( conn_data );
	connections[ sockfd ] = NULL;
}

void exread( int sockfd, void* udata ) {
	char buffer[ PACK_DATA_SIZE ],* ptr;
	int len;
	conn_data_t* conn_data = connections[ sockfd ];
	website_t* website;

	// TODO: wait for entire header before looking up website
	if ( ( len = read( sockfd, buffer, sizeof( buffer ) ) ) <= 0 ) {
cleanup:
		// cleanup
		website = website_get_by_sockfd( conn_data->website_sockfd );
		if ( website && msg_exists( ((website_data_t*)website->udata)->msg_switch, sockfd ) )
			msg_kill( ((website_data_t*)website->udata)->msg_switch, sockfd );
		else
			cleanup_connection( sockfd );
		return;
	}

	memset( buffer + len, 0, sizeof( buffer ) - len );

	ptr = buffer;

	if ( conn_data->website_sockfd == -1 ) {
		/* If req_info is NULL this is the start of a request and the
		   HTTP request headers need to be parsed. */
		struct hostent* hp;
		hp = gethostbyaddr( (char*) &conn_data->addr.sin_addr.s_addr, sizeof( conn_data->addr.sin_addr.s_addr ), AF_INET );

		/* Get HTTP request type */
		if ( startswith( buffer, HTTP_GET ) )
			conn_data->request_type = HTTP_GET_TYPE;
		else if ( startswith( buffer, HTTP_POST ) )
			conn_data->request_type = HTTP_POST_TYPE;
		else goto cleanup;

		/* Find website for request from HTTP header */
		char urlbuf[ PACK_DATA_SIZE ];
		if ( !get_url_from_http_header( buffer, urlbuf, sizeof( urlbuf ) ) ) {
			syslog( LOG_WARNING, "exread: no url found in http request headers: %s %s",
			  inet_ntoa( conn_data->addr.sin_addr ), hp ? hp->h_name : "" );
			goto cleanup;
		}
		if ( !( website = website_find_by_url( urlbuf ) ) ) {
			syslog( LOG_WARNING, "exread: no website to service request: %s %s %s",
			  inet_ntoa( conn_data->addr.sin_addr ), hp ? hp->h_name : "", urlbuf );
			goto cleanup;
		}

		/* Set the website id so that furthur sections of this
		   request can check if the website is still alive. */
		conn_data->website_sockfd = website->sockfd;

		/* Check the websites socket to make sure the request came in on
		   the right socket. */
		website_data_t* website_data = website->udata;

		if ( website_data->exlisnfd != conn_data->exlisnfd ) {
			syslog( LOG_WARNING, "exread: no website to service request: %s %s %s",
			  inet_ntoa( conn_data->addr.sin_addr ), hp ? hp->h_name : "", urlbuf );
			goto cleanup;
		}

		if ( !strstr( buffer, HTTP_HDR_ENDL HTTP_HDR_ENDL ) ) {
			/* If the end of the headers was not found the request was either
			   malformatted or too long, DO NOT send to website. */
			syslog( LOG_WARNING, "exread: headers to long" );
			goto cleanup;
		}

		/* Create a new message to send the request to the corresponding
		   website. The msgid should be set to the external file descriptor
		   to send the response back to. */
		msg_new( website_data->msg_switch, sockfd );

		// Write the ip address and hostname of the request
		msg_push_data( website_data->msg_switch, sockfd, (void*) &conn_data->addr.sin_addr, sizeof( conn_data->addr.sin_addr ) );
		if ( hp ) msg_push_data( website_data->msg_switch, sockfd, (void*) hp->h_name, strlen( hp->h_name ) );
		msg_push_data( website_data->msg_switch, sockfd, (void*) "\0", 1 );

		// If request_type is POST check if there is content after the HTTP header
		char postlenbuf[ 32 ];
		memset( postlenbuf, 0, sizeof( postlenbuf ) );
		if ( conn_data->request_type == HTTP_POST_TYPE && ( ptr = strstr( buffer, "Content-Length: " ) ) ) {
			memcpy( postlenbuf, ptr + 16, (long) strstr( ptr + 16, HTTP_HDR_ENDL ) - (long) ( ptr + 16 ) );
			conn_data->postlen = atoi( postlenbuf );
		}

		// Send the whole header to the website
		ptr = strstr( buffer, HTTP_HDR_ENDL HTTP_HDR_ENDL ) + strlen( HTTP_HDR_ENDL HTTP_HDR_ENDL );
		msg_push_data( website_data->msg_switch, sockfd, (void*) buffer, ( ptr - buffer ) );
	}

	else {
		if ( !( website = website_get_by_sockfd( conn_data->website_sockfd ) ) ) {
			syslog( LOG_WARNING, "exread: no website to service request" );
			goto cleanup;
		}
	}

	website_data_t* website_data = website->udata;
	if ( conn_data->request_type == HTTP_POST_TYPE && conn_data->postlen ) {
		int left = len - ( ptr - buffer );

		if ( left > conn_data->postlen )
			conn_data->postlen = left;

		msg_push_data( website_data->msg_switch, sockfd, (void*) ptr, left );

		conn_data->postlen -= left;
	}

	if ( !conn_data->postlen ) { /* If there isn't still data coming, */
		msg_end( website_data->msg_switch, sockfd );
	}

}

void exwrit( int sockfd, void* udata ) {
	conn_data_t* conn_data = connections[ sockfd ];
	char status = MSG_PACK_RESP_OK;
	website_t* website = website_get_by_sockfd( conn_data->website_sockfd );
	website_data_t* website_data = website->udata;
	int n;
	msg_packet_t* packet = conn_data->pending_packet;
	conn_data->pending_packet = NULL;

	zfd_clr( sockfd, EXWRIT );

	n = write( sockfd, packet->data, packet->size );

	if ( n != packet->size ) {
		if ( n == -1 )
			syslog( LOG_ERR, "exwrit: write failed: %s", strerror( errno ) );
		status = MSG_PACK_RESP_FAIL;
	}

	msg_switch_send_pack_resp( website_data->msg_switch, packet, status );

	free( packet );
}

void command_handler( msg_switch_t* msg_switch, msg_packet_t* packet ) {
	char status;

	switch ( packet->msgid ) {

		case ZM_CMD_WS_START:
			if ( start_website( packet->data, msg_switch ) )
				status = MSG_PACK_RESP_OK;
			else
				status = MSG_PACK_RESP_FAIL;
			break;

		case ZM_CMD_WS_STOP:
			if ( remove_website( msg_switch ) )
				status = MSG_PACK_RESP_OK;
			else
				status = MSG_PACK_RESP_FAIL;
			break;

		/*case ZM_CMD_STATUS:
			message = get_status_message( buffer, sizeof( buffer ) );
			ztransport_write( response, message, strlen( message ) );
			break;*/

		default:
			status = MSG_PACK_RESP_FAIL;
			break;
	}

	msg_switch_send_pack_resp( msg_switch, packet, status );
}

////////////////////////////////////////////////////////////////
