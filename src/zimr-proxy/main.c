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
#include <errno.h>
#include <syslog.h>
#include <unistd.h>

#include "msg_switch.h"
#include "zfildes.h"
#include "website.h"
#include "zsocket.h"
#include "daemon.h"
#include "zcnf.h"

#define DAEMON_NAME "zimr-proxy"

#define INLISN 0x01
#define INREAD 0x02
#define INWRIT 0x03
#define EXLISN 0x04
#define EXREAD 0x05
#define EXWRIT 0x06

typedef struct {
	int exlisnfd;
} website_data_t;

typedef struct {
	int exlisnfd;
	struct sockaddr_in addr;
	int website_sockfd;
	int request_type;
	int postlen;
	int is_https;
} conn_data_t;

conn_data_t* connections[ FD_SETSIZE ];

void insock_event_hdlr( int fd, zsocket_event_t event );
void exsock_event_hdlr( int fd, zsocket_event_t event );
void command_handler( msg_switch_t* msg_switch, msg_packet_t* packet );
void cleanup_connection( int sockfd );
void exread( int sockfd, void* buffer, ssize_t len, size_t size );

void signal_handler( int sig ) {
	switch( sig ) {
		case SIGPIPE:
			break;
		default:
			exit(EXIT_FAILURE);
			break;
	}
}

void print_usage() {
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
	bool force_stop = false;

	///////////////////////////////////////////////
	// parse command line options
	int i;
	for ( i = 1; i < argc; i++ ) {
		if ( argv[i][0] != '-' ) break;
		if ( strcmp( argv[i], "--no-daemon" ) == 0 )
			make_daemon = 0;
		else if ( strcmp( argv[i], "--force" ) == 0 ) {
			daemon_flags |= D_NOLOCKCHECK;
			force_stop = true;
		} else if ( strcmp( argv[i], "--no-lockfile" ) == 0 ) {
			daemon_flags |= D_NOLOCKFILE;
		} else {
			if ( strcmp( argv[i], "--help" ) != 0 && strcmp( argv[i], "-h" ) != 0 )
				printf( "\nUnknown Option: %s\n", argv[i] );
			print_usage();
			exit(0);
		}
	}

	if ( i == argc ) {
		print_usage();
		exit(0);
	}

	if ( i != argc - 1 ) {
		printf( "Error: Invalid Command\n\n" );
		print_usage();
		exit(0);
	}

	// parse command line commands
	if ( strcmp( argv[i], "start" ) != 0 ) {
		if ( strcmp( argv[i], "stop" ) == 0 ) {
			if ( !daemon_stop( force_stop ) ) exit( EXIT_FAILURE );
			exit( EXIT_SUCCESS );
		} else if ( strcmp( argv[i], "restart" ) == 0 ) {
			if ( !daemon_stop( force_stop ) ) exit( EXIT_FAILURE );
		} else {
			printf( "Error: Unknown Command: %s\n\n", argv[i] );
			print_usage();
			exit(0);
		}
	}
	/////////////////////////////////////////////////

	if ( make_daemon ) {
		if ( !daemon_init( daemon_flags ) ) {
			exit(0);
		}
	}

	zcnf_proxies_t* proxy_cnf = zcnf_proxy_load();
	if ( !proxy_cnf ) {
		fprintf( stderr, "failed to load config.\n" );
		exit( EXIT_FAILURE );
	}

	// Trap SIGPIPE
	signal( SIGPIPE, signal_handler );

	// call any needed library init functions
	website_init();
	zsocket_init();

	memset( connections, 0, sizeof( connections ) );

	for ( i = 0; i < proxy_cnf->n; i++ ) {
		int sockfd = zsocket( inet_addr( proxy_cnf->proxies[ i ].ip ), proxy_cnf->proxies[ i ].port, ZSOCK_LISTEN, insock_event_hdlr, false );
		if ( sockfd == -1 ) {
			ret = EXIT_FAILURE;
			fprintf( stderr, "Proxy failed on %s:%d\n", proxy_cnf->proxies[ i ].ip, proxy_cnf->proxies[ i ].port );
			goto quit;
		}

		zaccept( sockfd, true );
		printf( " * started listening on %s:%d\n", proxy_cnf->proxies[ i ].ip, proxy_cnf->proxies[ i ].port );
	}

	FL_SET( daemon_flags, D_KEEPSTDIO );

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
		if ( !daemon_detach( daemon_flags ) ) {
			ret = EXIT_FAILURE;
			goto quit;
		}
	}

#ifndef DEBUG
	daemon_redirect_stdio();
#endif

	syslog( LOG_INFO, "initialized." );

	// starts a select() loop and calls
	// the associated file descriptor handlers
	// when they are ready to read/write
	do { msg_switch_fire_all_events(); }
	while ( zfd_select(0) ); // The loop is only broken by interrupt

quit:
	// cleanup

	if ( ret == EXIT_FAILURE ) {
		syslog( LOG_INFO, "exit: failure" );
	} else {
		syslog( LOG_INFO, "exit: success" );
	}

	closelog();
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
	char* ptr1,* ptr2,* ptr3;
	char port_str[ 6 ];
	memset( port_str, 0, sizeof( port_str ) );

	ptr1 = strstr( url, ":" ) + 1;
	ptr2 = strstr( ptr1, ":" );
	ptr3 = strstr( ptr1 + 2, "/" );

	if ( !ptr2 ) {
		if ( startswith( url, "https://" ) )
			return HTTPS_DEFAULT_PORT;
		return HTTP_DEFAULT_PORT;
	}

	if ( !ptr3 )
		ptr3 = ptr1 + strlen( ptr1 );

	ptr2++;

	strncpy( port_str, ptr2, ptr3 - ptr2 );
	return atoi( port_str );
}

int remove_website( msg_switch_t* msg_switch ) {
	website_t* website;

	if ( !( website = website_get_by_sockfd( msg_switch->sockfd ) ) ) {
		syslog( LOG_WARNING, "stop_website: tried to stop a nonexisting website" );
		return 0;
	}

	syslog( LOG_INFO, "website: stopping \"%s\"", website->full_url );
	website_data_t* website_data = (website_data_t*) website->udata;

	/*int i;
	for ( i = 0; i < FD_SETSIZE; i++ ) {
		if ( connections[ i ] && connections[ i ]->website_sockfd == website->sockfd )
			cleanup_connection( i );
	}*/

	if ( website_data->exlisnfd != -1 ) {
		zclose( website_data->exlisnfd );
	}

	msg_switch_destroy( website->sockfd );
	zclose( website->sockfd );

	free( website_data );
	website_remove( website );

	return 1;
}

const char* start_website( char* url, msg_switch_t* msg_switch ) {
	static char error_msg[64] = "";
	website_t* website;
	website_data_t* website_data;

	if ( ( website = website_get_by_full_url( url ) ) ) {
		return "FAIL website already exists";
	}

	int exlisnfd = zsocket( INADDR_ANY, get_port_from_url( url ), ZSOCK_LISTEN, exsock_event_hdlr, startswith( url, "https://" ) );
	if ( exlisnfd == -1 ) {
		sprintf( error_msg, "FAIL error opening external socket: %s", strerror( errno ) );
		return error_msg;
	}

	website = website_add( msg_switch->sockfd, url );

	syslog( LOG_INFO, "website: starting \"%s\"", website->full_url );

	website_data = (website_data_t*) malloc( sizeof( website_data_t ) );
	website->udata = website_data;

	website_data->exlisnfd = exlisnfd;

	zaccept( website_data->exlisnfd, true );
	return "OK";
}

// Handlers ////////////////////////////////////////////

void msg_event_handler( msg_switch_t* msg_switch, msg_event_t* event ) {
	//printf( "event 0x%03x for msg %d\n", event->type, event->data.msgid );
	switch ( event->type ) {
		case MSG_EVT_READ_START:
			msg_want_data( msg_switch->sockfd, event->data.msgid );
			break;
		case MSG_EVT_WRITE_START:
			break;
		case MSG_EVT_READ_END:
			//zpause( event->data.msgid, true );
			break;
		case MSG_EVT_COMPLETE:
			break;
		case MSG_EVT_WRITE_END:
			if ( event->data.msgid >= 0 ) {
				zread( event->data.msgid, false );
			}
			break;
		case MSG_EVT_DESTROYED:
			if ( event->data.msgid >= 0 ) {
				cleanup_connection( event->data.msgid );
			}
			break;
		case MSG_EVT_WRITE_SPACE_FULL:
			if ( event->data.msgid >= 0 ) {
				zread( event->data.msgid, false );
			}
			break;
		case MSG_EVT_WRITE_SPACE_AVAIL:
			if ( event->data.msgid >= 0 ) {
				zread( event->data.msgid, true );
			}
			break;
		case MSG_EVT_RECVD_DATA:
			if ( event->data.packet.header.msgid < 0 ) {
				/* If the msgid is less than zero, it refers to a command and
				   the message should not be routed to a file descriptor. */

				// the entire command must fit inside one transport
				if ( PACK_IS_FIRST( &event->data.packet ) && PACK_IS_LAST( &event->data.packet ) )
					command_handler( msg_switch, &event->data.packet );
				else
					syslog( LOG_WARNING, "received a command that is to long...ignoring." );

			} else {
				/* if msgid is zero or greater it refers to a socket file
				   descriptor that the message should be routed to. */
				zwrite( event->data.packet.header.msgid, event->data.packet.data, event->data.packet.header.size );
			}
			break;
		case MSG_SWITCH_EVT_IO_FAILED:
			//zclose( msg_switch->sockfd );
			if ( website_get_by_sockfd( msg_switch->sockfd ) )
				remove_website( msg_switch );
			else {
				int sockfd = msg_switch->sockfd;
				msg_switch_destroy( sockfd );
				zclose( sockfd );
			}
			break;
		case MSG_SWITCH_EVT_NEW:
		case MSG_SWITCH_EVT_DESTROY:
			break;
	}
}

void insock_event_hdlr( int fd, zsocket_event_t event ) {
	switch ( event.type ) {
		case ZSE_ACCEPT_ERR:
			fprintf( stderr, "New Connection Failed\n" );
			break;
		case ZSE_ACCEPTED_CONNECTION:
			msg_switch_create( event.data.conn.fd, msg_event_handler );
			break;
		case ZSE_READ_DATA:
		case ZSE_WROTE_DATA:
			fprintf( stderr, "Should not be reading or writing in this zsocket_hdlr\n" );
			break;
	}
}

void exsock_event_hdlr( int fd, zsocket_event_t event ) {
	conn_data_t* conn_data;

	switch ( event.type ) {
		case ZSE_ACCEPT_ERR:
			fprintf( stderr, "New Connection Failed\n" );
			break;
		case ZSE_ACCEPTED_CONNECTION:
			// pass the connection information along...
			conn_data = (conn_data_t*) malloc( sizeof( conn_data_t ) );
			conn_data->exlisnfd = fd;
			conn_data->addr = event.data.conn.addr;
			conn_data->website_sockfd = -1;
			conn_data->request_type = 0;
			conn_data->postlen = 0;
			conn_data->is_https = zsocket_is_ssl( conn_data->exlisnfd );

			assert( !connections[ event.data.conn.fd ] );
			connections[ event.data.conn.fd ] = conn_data;

			zread( event.data.conn.fd, true );
			//msg_start( inconnfd, event.data.conn.fd );
			break;
		case ZSE_READ_DATA:
			exread( fd, event.data.read.buffer, event.data.read.buffer_used, event.data.read.buffer_size );
			break;
		case ZSE_WROTE_DATA:
			if ( event.data.write.buffer_used <= 0 ) {
				zpause( fd, true );
				msg_destroy( connections[ fd ]->website_sockfd, fd );
			}
			msg_want_data( connections[ fd ]->website_sockfd, fd );
			//if ( PACK_IS_LAST( packet ) )
			//	close( sockfd );
			break;
	}
}

void cleanup_connection( int sockfd ) {
	zclose( sockfd );
	free( connections[ sockfd ] );
	connections[ sockfd ] = NULL;
}

void exread( int sockfd, void* buffer, ssize_t len, size_t size ) {
	void* ptr;
	conn_data_t* conn_data = connections[ sockfd ];
	website_t* website;

	// TODO: wait for entire header before looking up website
	if ( len <= 0 ) {
cleanup:
		// cleanup
		website = website_get_by_sockfd( conn_data->website_sockfd );
		if ( website && msg_exists( website->sockfd, sockfd ) ) {
			zpause( sockfd, true );
			msg_destroy( website->sockfd, sockfd );
		} else cleanup_connection( sockfd );
		return;
	}

	memset( buffer + len, 0, size - len );

	ptr = buffer;

	if ( conn_data->website_sockfd == -1 ) {
		/* If req_info is NULL this is the start of a request and the
		   HTTP request headers need to be parsed. */
		struct hostent* hp;
		hp = NULL;//gethostbyaddr( (char*) &conn_data->addr, sizeof( conn_data->addr ), AF_INET );

		/* Get HTTP request type */
		if ( startswith( buffer, HTTP_GET ) )
			conn_data->request_type = HTTP_GET_TYPE;
		else if ( startswith( buffer, HTTP_POST ) )
			conn_data->request_type = HTTP_POST_TYPE;
		else goto cleanup;

		/* Find website for request from HTTP header */
		char urlbuf[ PACK_DATA_SIZE ];
		if ( !get_url_from_http_header( buffer, urlbuf, sizeof( urlbuf ) ) ) {
			//syslog( LOG_WARNING, "exread: no url found in http request headers: %s %s",
			//  inet_ntoa( conn_data->addr.sin_addr ), hp ? hp->h_name : "" );
			goto cleanup;
		}
		if ( !( website = website_find( urlbuf, conn_data->is_https ? "https://" : "http://" ) ) ) {
			//syslog( LOG_WARNING, "exread: no website to service request: %s %s %s",
			//  inet_ntoa( conn_data->addr.sin_addr ), hp ? hp->h_name : "", urlbuf );
			goto cleanup;
		}

		/* Set the website id so that furthur sections of this
		   request can check if the website is still alive. */
		conn_data->website_sockfd = website->sockfd;

		/* Check the websites socket to make sure the request came in on
		   the right socket. */
		website_data_t* website_data = website->udata;

		if ( website_data->exlisnfd != conn_data->exlisnfd ) {
			//syslog( LOG_WARNING, "exread: no website to service request: %s %s %s",
			//  inet_ntoa( conn_data->addr.sin_addr ), hp ? hp->h_name : "", urlbuf );
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
		msg_start( website->sockfd, sockfd );

		// Write the ip address and hostname of the request
		msg_send( website->sockfd, sockfd, (void*) &conn_data->addr.sin_addr, sizeof( conn_data->addr.sin_addr ) );
		if ( hp ) msg_send( website->sockfd, sockfd, (void*) hp->h_name, strlen( hp->h_name ) );
		msg_send( website->sockfd, sockfd, (void*) "\0", 1 );

		// If request_type is POST check if there is content after the HTTP header
		char postlenbuf[ 32 ];
		memset( postlenbuf, 0, sizeof( postlenbuf ) );
		if ( conn_data->request_type == HTTP_POST_TYPE && ( ptr = strstr( buffer, "Content-Length: " ) ) ) {
			memcpy( postlenbuf, ptr + 16, (long) strstr( ptr + 16, HTTP_HDR_ENDL ) - (long) ( ptr + 16 ) );
			conn_data->postlen = atoi( postlenbuf );
		}

		// Send the whole header to the website
		ptr = strstr( buffer, HTTP_HDR_ENDL HTTP_HDR_ENDL ) + strlen( HTTP_HDR_ENDL HTTP_HDR_ENDL );
		msg_send( website->sockfd, sockfd, (void*) buffer, ( ptr - buffer ) );
	}

	else {
		if ( !( website = website_get_by_sockfd( conn_data->website_sockfd ) ) ) {
			syslog( LOG_WARNING, "exread: no website to service request" );
			goto cleanup;
		}
	}

	if ( conn_data->request_type == HTTP_POST_TYPE && conn_data->postlen ) {
		int left = len - ( ptr - buffer );

		if ( left > conn_data->postlen )
			conn_data->postlen = left;

		msg_send( website->sockfd, sockfd, (void*) ptr, left );

		conn_data->postlen -= left;
	}

	if ( !conn_data->postlen ) { /* If there isn't still data coming, */
		msg_end( website->sockfd, sockfd );
		//zread( sockfd, false );
	}

}

void command_handler( msg_switch_t* msg_switch, msg_packet_t* packet ) {
	const char* ret_msg;

	switch ( packet->header.msgid ) {

		case ZM_CMD_WS_START:
			ret_msg = start_website( packet->data, msg_switch );
			msg_start( msg_switch->sockfd, ZM_CMD_WS_START );
			msg_send( msg_switch->sockfd, ZM_CMD_WS_START, ret_msg, strlen( ret_msg ) );
			msg_end( msg_switch->sockfd, ZM_CMD_WS_START );
			break;

		case ZM_CMD_WS_STOP:
			remove_website( msg_switch );
			break;

		/*case ZM_CMD_STATUS:
			message = get_status_message( buffer, sizeof( buffer ) );
			ztransport_write( response, message, strlen( message ) );
			break;*/

		default:
			//status = MSG_PACK_RESP_FAIL;
			break;
	}
}

////////////////////////////////////////////////////////////////
