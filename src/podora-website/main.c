#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "general.h"
#include "pcom.h"

fd_set read_fd_set;
void* fd_data[ 256 ];
int res_pd, req_pd;
int key;

void start_website( );

void quitproc( ) {
	printf( "quit\n" );
}

int main( int argc, char *argv[ ] ) {
	char buffer[ 2048 ];
	char filename[ 100 ];
	int fd;
	int n, i;

	key = randkey( );

	if ( argc < 2 ) {
		return EXIT_FAILURE;
	}

	signal( SIGINT, quitproc );

	if ( ( res_pd = pcom_connect( atoi( argv[ 1 ] ), 3 ) ) < 0 ) {
		return EXIT_FAILURE;
	}

	FD_ZERO( &read_fd_set );

	if ( ( req_pd = pcom_create( ) ) < 0 ) {
		return EXIT_FAILURE;
	}

	FD_SET( req_pd, &read_fd_set );

	start_website( );

	while ( 1 ) {

		if ( select( FD_SETSIZE, &read_fd_set, NULL, NULL, NULL ) < 0 ) {
			perror( "main: select() failed" );
			return 0;
		}

		for ( i = 0; i < FD_SETSIZE; i++ )
			if ( FD_ISSET( i, &read_fd_set ) ) {

				if ( i == req_pd ) {
					pcom_transport_t* transport = pcom_open( req_pd, PCOM_RO, 0, 0 );
					pcom_read( transport );
					printf( "requested: %d\n", transport->header->id );

					sprintf( filename, "in/%d.in", transport->header->id );
					if ( ( fd = open( filename, O_RDONLY ) ) < 0 ) {
						return EXIT_FAILURE;
					}

					FD_SET( fd, &read_fd_set );

					fd_data[ fd ] = pcom_open( res_pd, PCOM_WO, transport->header->id, key );

					pcom_close( transport );
				} else {
					if ( ( n = read( i, buffer, sizeof( buffer ) ) ) ) {
						if ( !pcom_write( fd_data[ i ], buffer, n ) ) {
							fprintf( stderr, "[error] main: pcom_write() failed\n" );
							exit( EXIT_FAILURE );
						}
					} else {
						pcom_close( fd_data[ i ] );
						FD_CLR( i, &read_fd_set );
						close( i );
					}
				}

			}

	}

	return 0;
}

void send_cmd( int cmd, void* message, int size ) {
	pcom_transport_t* transport = pcom_open( res_pd, PCOM_WO, cmd, key );
	pcom_write( transport, message, size );
	pcom_close( transport );
}

void start_website( ) {
	int pid = getpid( );
	char message[ sizeof( int ) * 2 ];
	memcpy( message, &pid, sizeof( int ) );
	memcpy( message + sizeof( int ), &req_pd, sizeof( int ) );
	send_cmd( -1, message, sizeof( message ) );
}
