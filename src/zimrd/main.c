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

#define DAEMON_NAME "zimrd"

char html_error_400[] = "HTTP/1.1 400 Bad Request\r\nContent-Length: 65\r\n\r\n<!doctype html><html><body><h1>400 Bad Request</h1></body></html>";
char html_error_404[] = "HTTP/1.1 404 Not Found\r\nContent-Length: 63\r\n\r\n<!doctype html><html><body><h1>404 Not Found</h1></body></html>";
char html_error_414[] = "HTTP/1.1 414 Request-URI Too Long\r\nContent-Length: 74\r\n\r\n<!doctype html><html><body><h1>414 Request-URI Too Long</h1></body></html>";
char html_error_502[] = "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 65\r\n\r\n<!doctype html><html><body><h1>502 Bad Gateway</h1></body></html>";

typedef struct {
	struct sockaddr_in exaddr;
	int exlisnfd;
} website_data_t;

typedef struct {
	int exlisnfd;
	struct sockaddr_in addr;
	int website_sockfd;
	int msgid;
	int request_type;
	size_t postlen;
	int is_https;
} conn_data_t;

conn_data_t* connections[ FD_SETSIZE ];

void insock_event_hdlr( int fd, int event );
void exsock_event_hdlr( int fd, int event );
void command_handler( int fd, int msgid, void* buf, size_t len );
void cleanup_connection( int sockfd );
void exread( int sockfd );

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
	printf( "Zimr Daemon " ZIMR_VERSION " (" BUILD_DATE ") - "  ZIMR_WEBSITE "\n" );
	printf(
"\nUsage: zimrd [OPTIONS] {start|stop|restart}\n\
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
	zs_init();

	memset( connections, 0, sizeof( connections ) );

	for ( i = 0; i < proxy_cnf->n; i++ ) {
		int sockfd = zsocket( inet_addr( proxy_cnf->proxies[ i ].ip ), proxy_cnf->proxies[ i ].port, ZSOCK_LISTEN, insock_event_hdlr, false );
		if ( sockfd == -1 ) {
			ret = EXIT_FAILURE;
			fprintf( stderr, "Proxy failed on %s:%d\n", proxy_cnf->proxies[ i ].ip, proxy_cnf->proxies[ i ].port );
			goto quit;
		}

		zs_set_read( sockfd );
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
	do {
		do {
			msg_switch_select();
			zs_select();
		} while ( msg_switch_need_select() || zs_need_select() );
	} while ( zfd_select(2) );

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

int remove_website( int fd ) {
	website_t* website;

	if ( !( website = website_get_by_sockfd( fd ) ) ) {
		syslog( LOG_WARNING, "stop_website: tried to stop a nonexisting website" );
		return 0;
	}

	syslog( LOG_INFO, "website: stopping \"%s\" on \"%s\"", website->full_url, website->ip );
	website_data_t* website_data = (website_data_t*) website->udata;

	int i;
	for ( i = 0; i < FD_SETSIZE; i++ ) {
		if ( connections[ i ] && connections[ i ]->website_sockfd == website->sockfd )
			cleanup_connection( i );
	}

	msg_switch_destroy( fd );
	zs_close( fd );

	if ( website_data->exlisnfd != -1 ) {
		zs_close( website_data->exlisnfd );
	}

	free( website_data );
	website_remove( website );

	return 1;
}

const char* start_website( int fd, char* url, char* ip ) {
	static char error_msg[64] = "";
	website_t* website;
	website_data_t* website_data;

	if ( ( website = website_get_by_full_url( url, ip ) ) ) {
		return "FAIL website already exists";
	}

	int exlisnfd = zsocket( inet_addr( ip ), get_port_from_url( url ), ZSOCK_LISTEN, exsock_event_hdlr, startswith( url, "https://" ) );
	if ( exlisnfd == -1 ) {
		sprintf( error_msg, "FAIL error opening external socket: %s", strerror( errno ) );
		return error_msg;
	}

	website = website_add( fd, url, ip );

	syslog( LOG_INFO, "website: starting \"%s\" on \"%s\"", website->full_url, website->ip );

	website_data = (website_data_t*) malloc( sizeof( website_data_t ) );
	website->udata = website_data;

	website_data->exlisnfd = exlisnfd;

	zs_set_read( website_data->exlisnfd );
	return "OK";
}

// Handlers ////////////////////////////////////////////

void msg_event_handler( int fd, int msgid, int event ) {
	char buf[PACK_DATA_SIZE];
	ssize_t n;
//	printf( "event 0x%03x for msg %d\n", event, msgid );
	switch ( event ) {
		case MSG_EVT_ACCEPT_READY:
			if ( msg_accept( fd, msgid ) != 0 )
				msg_set_read( fd, msgid );
			// These are commands
			break;
		case MSG_EVT_READ_READY:
			msg_clr_read( fd, msgid );

			n = msg_read( fd, msgid, buf, sizeof( buf ) );
			if ( n <= 0 ) {
				cleanup_connection( msg_get_type( fd, msgid ) );
				break;
			}

			if ( msg_get_type( fd, msgid ) < 0 ) {
				/* If the msgid is less than zero, it refers to a command and
				   the message should not be routed to a file descriptor. */

				// the entire command must fit inside one transport
				command_handler( fd, msgid, buf, n );

			} else {
				zs_set_write( msg_get_type( fd, msgid ) );
				/* if msgid is zero or greater it refers to a socket file
				   descriptor that the message should be routed to. */
				if ( zs_write( msg_get_type( fd, msgid ), buf, n ) == -1 )
					cleanup_connection( msg_get_type( fd, msgid ) );
			}
			break;
		case MSG_EVT_WRITE_READY:
			if ( msgid < 0 ) break;
			zs_set_read( msg_get_type( fd, msgid ) );
			msg_clr_write( fd, msgid );
			break;
		case MSG_SWITCH_EVT_IO_ERROR:
			//zclose( msg_switch->sockfd );
			if ( website_get_by_sockfd( fd ) )
				remove_website( fd );
			else {
				for ( n = 0; n < FD_SETSIZE; n++ ) {
					if ( msg_exists( fd, n ) ) {
						if ( msg_get_type( fd, n ) > 0 )
							cleanup_connection( msg_get_type( fd, n ) );
					//	else
					//		msg_close( fd, n );
					}
				}
				msg_switch_destroy( fd );
				zs_close( fd );
			}
			break;
	}
}

void insock_event_hdlr( int fd, int event ) {
	int newfd;
	switch ( event ) {
		case ZS_EVT_ACCEPT_READY:
			newfd = zs_accept( fd );
			if ( newfd == -1 )
				fprintf( stderr, "New Connection Failed\n" );
			else
				msg_switch_create( newfd, msg_event_handler );
			break;
		case ZS_EVT_READ_READY:
		case ZS_EVT_WRITE_READY:
			fprintf( stderr, "Should not be reading or writing in this zsocket_hdlr\n" );
			break;
	}
}

void exsock_event_hdlr( int fd, int event ) {
	int newfd;

	switch ( event ) {
		case ZS_EVT_ACCEPT_READY:
			newfd = zs_accept( fd );
			if ( newfd == -1 ) {
				perror( "New Connection Failed" );
				//zs_close( fd );
				break;
			}
			assert( !connections[ newfd ] );
			//msg_open( inconnfd, n );
			//msg_set_read( inconnfd, n );
			// pass the connection information along...
			connections[ newfd ] = (conn_data_t*) malloc( sizeof( conn_data_t ) );
			connections[ newfd ]->exlisnfd = fd;
			connections[ newfd ]->addr = *zs_get_addr( newfd );
			connections[ newfd ]->website_sockfd = -1;
			connections[ newfd ]->request_type = 0;
			connections[ newfd ]->postlen = 0;
			connections[ newfd ]->msgid = 0;
			connections[ newfd ]->is_https = zs_is_ssl( connections[ newfd ]->exlisnfd );

			zs_set_read( newfd );
			//msg_start( inconnfd, event.data.conn.fd );
			break;
		case ZS_EVT_READ_READY:
			zs_clr_read( fd );
			exread( fd );
			break;
		case ZS_EVT_WRITE_READY:
			zs_clr_write( fd );
			msg_set_read( connections[ fd ]->website_sockfd, connections[ fd ]->msgid );
			break;
	}
}

void cleanup_connection( int sockfd ) {
	if ( connections[ sockfd ]->website_sockfd != -1
	  && msg_exists( connections[ sockfd ]->website_sockfd, connections[ sockfd ]->msgid ) )
		msg_close( connections[ sockfd ]->website_sockfd, connections[ sockfd ]->msgid );
	zs_close( sockfd );
	free( connections[ sockfd ] );
	connections[ sockfd ] = NULL;
}

void exread( int sockfd ) {
	char buffer[PACK_DATA_SIZE];
	void* ptr;
	conn_data_t* conn_data = connections[ sockfd ];
	website_t* website;

	ssize_t len = zs_read( sockfd, buffer, sizeof( buffer ) );

	// TODO: wait for entire header before looking up website
	if ( len <= 0 ) {
cleanup:
		// cleanup
		cleanup_connection( sockfd );
		return;
	}

	if ( conn_data->postlen == -1 ) /* if we have received all the data already, ignore */
		return;

	//printf( "%d: read %zu\n", sockfd, len );

	memset( buffer + len, 0, sizeof( buffer ) - len );

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
		else {
			zs_write( sockfd, html_error_400, sizeof( html_error_400 ) );
			goto cleanup;
		}

		/* Find website for request from HTTP header */
		char urlbuf[ PACK_DATA_SIZE ];
		if ( !get_url_from_http_header( buffer, urlbuf, sizeof( urlbuf ) ) ) {
			//syslog( LOG_WARNING, "exread: no url found in http request headers: %s %s",
			//  inet_ntoa( conn_data->addr.sin_addr ), hp ? hp->h_name : "" );
			zs_write( sockfd, html_error_400, sizeof( html_error_400 ) );
			goto cleanup;
		}

		if ( !( website = website_find( urlbuf, conn_data->is_https ? "https://" : "http://",
		  inet_ntoa( zs_get_addr( conn_data->exlisnfd )->sin_addr ) ) ) ) {
			//syslog( LOG_WARNING, "exread: no website to service request: %s %s %s",
			//  inet_ntoa( conn_data->addr.sin_addr ), hp ? hp->h_name : "", urlbuf );
			zs_write( sockfd, html_error_404, sizeof( html_error_404 ) );
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
			zs_write( sockfd, html_error_404, sizeof( html_error_404 ) );
			goto cleanup;
		}

		char* endofhdrs;
		if ( !( endofhdrs = strstr( buffer, HTTP_HDR_ENDL HTTP_HDR_ENDL ) ) ) {
			/* If the end of the headers was not found the request was either
			   malformatted or too long, DO NOT send to website. */
			zs_write( sockfd, html_error_414, sizeof( html_error_414 ) );
			syslog( LOG_WARNING, "exread: headers to long for %s", website->url );
			goto cleanup;
		}
		endofhdrs += sizeof( HTTP_HDR_ENDL HTTP_HDR_ENDL ) - 1;
		//printf( "%s\n", buffer );

		/* Create a new message to send the request to the corresponding
		   website. The msgid should be set to the external file descriptor
		   to send the response back to. */
		conn_data->msgid = msg_open( website->sockfd, sockfd );
		zs_set_write( sockfd );

		// If request_type is POST check if there is content after the HTTP header
		char postlenbuf[ 32 ];
		memset( postlenbuf, 0, sizeof( postlenbuf ) );
		if ( conn_data->request_type == HTTP_POST_TYPE && ( ptr = strstr( buffer, "Content-Length: " ) ) ) {
			memcpy( postlenbuf, ptr + 16, (long) strstr( ptr + 16, HTTP_HDR_ENDL ) - (long) ( ptr + 16 ) );
			conn_data->postlen = strtoumax( postlenbuf, NULL, 0 );
		}

		// Write the message length
		size_t msglen = ( endofhdrs - buffer ) + conn_data->postlen + sizeof( conn_data->addr.sin_addr ) + 1;
		if ( hp ) msglen += strlen( hp->h_name );
		if ( msg_write( website->sockfd, conn_data->msgid, (void*) &msglen, sizeof( size_t ) ) == -1 ) {
			zs_write( sockfd, html_error_502, sizeof( html_error_502 ) );
			goto cleanup;
		}

		// Write the ip address and hostname of the request
		if ( msg_write( website->sockfd, conn_data->msgid, (void*) &conn_data->addr.sin_addr, sizeof( conn_data->addr.sin_addr ) ) == -1
		  || ( hp && msg_write( website->sockfd, conn_data->msgid, (void*) hp->h_name, strlen( hp->h_name ) ) == -1 )
		  || msg_write( website->sockfd, conn_data->msgid, (void*) "\0", 1 ) == -1 ) {
			zs_write( sockfd, html_error_502, sizeof( html_error_502 ) );
			goto cleanup;
		}

		// Send the whole header to the website
		if ( msg_write( website->sockfd, conn_data->msgid, (void*) buffer, ( endofhdrs - buffer ) ) == -1 ) {
			zs_write( sockfd, html_error_502, sizeof( html_error_502 ) );
			goto cleanup;
		}

		ptr = endofhdrs;
	}

	else {
		if ( !( website = website_get_by_sockfd( conn_data->website_sockfd ) ) ) {
		//	syslog( LOG_WARNING, "exread: no website to service request" );
			zs_write( sockfd, html_error_502, sizeof( html_error_502 ) );
			goto cleanup;
		}
	}

	if ( conn_data->request_type == HTTP_POST_TYPE && conn_data->postlen ) {
		int left = len - ( ptr - (void*)buffer );

		if ( left > conn_data->postlen )
			conn_data->postlen = left;

		if ( msg_write( website->sockfd, conn_data->msgid, (void*) ptr, left ) == -1 ) {
			zs_write( sockfd, html_error_502, sizeof( html_error_502 ) );
			goto cleanup;
		}

		conn_data->postlen -= left;
	}

	if ( !conn_data->postlen ) { /* If there isn't more data coming, */
		msg_flush( website->sockfd, conn_data->msgid );

		/* must make sure to keep sockfd in read mode to detect a connection close from
		   the other end */
		conn_data->postlen = -1;
		zs_set_read( sockfd );
	} else
		msg_set_write( website->sockfd, conn_data->msgid );

	//printf( "%d: still needs %d post data\n", sockfd, conn_data->postlen );
}

void command_handler( int fd, int msgid, void* buf, size_t len ) {
	const char* ret_msg;
	switch ( msg_get_type( fd, msgid ) ) {

		case ZM_CMD_WS_START:
			ret_msg = start_website( fd, buf, buf + strlen( buf ) + 1 );
			msg_write( fd, msgid, ret_msg, strlen( ret_msg ) + 1 );
			msg_close( fd, msgid );
			break;

		case ZM_CMD_WS_STOP:
			remove_website( fd );
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
