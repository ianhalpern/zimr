#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "pcom.h"

int fds[ 1024 ][ 1024 ];

int main( ) {
	int pd, fd;
	char filename[ 100 ];

	if ( ( pd = pcom_create( ) ) < 0 ) {
		return EXIT_FAILURE;
	}

	pcom_transport_t* transport = pcom_open( pd, PCOM_RO, 0, 0 );

	while ( pcom_read( transport ) ) {

		if ( PCOM_MSG_IS_FIRST( transport ) ) {
			if ( transport->header->id < 0 ) {
				printf( "%d:%d:%d\n", *(int*) transport->message, *(int*) ( transport->message + sizeof( int ) ), transport->header->key );
				int req_pd = pcom_connect( *(int*) transport->message, *(int*) ( transport->message + sizeof( int ) ) );

				pcom_transport_t* transport2 = pcom_open( req_pd, PCOM_WO, 1, transport->header->key );
				pcom_close( transport2 );

				transport2 = pcom_open( req_pd, PCOM_WO, 2, transport->header->key );
				pcom_close( transport2 );

				transport2 = pcom_open( req_pd, PCOM_WO, 3, transport->header->key );
				pcom_close( transport2 );

				close( req_pd );
				continue;
			}

			sprintf( filename, "out/%d", transport->header->key );
			mkdir( filename, 0777 );
			sprintf( filename, "out/%d/%d.out", transport->header->key, transport->header->id );
			if ( ( fd = creat( filename, 0644 ) ) < 0 ) {
				perror( "[error] main: creat() could not create file" );
				continue;
			}
			fds[ transport->header->key ][ transport->header->id ] = fd;
			printf( "%d: create %d\n", transport->header->key, transport->header->id );
		}

		if ( write( fds[ transport->header->key ][ transport->header->id ], transport->message, transport->header->size ) != transport->header->size )
			perror( "[error] main: write failed" );

		if ( PCOM_MSG_IS_LAST( transport ) ) {
			close( fds[ transport->header->key ][ transport->header->id ] );
			printf( "%d: close %d\n", transport->header->key, transport->header->id );
		}
	}

	close( pd );

	return 1;
}
