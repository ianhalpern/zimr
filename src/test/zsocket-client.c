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

/* A zsocket ping client
 * How to:
 *    Run:
 *    $ test-zsocket-client < inputfile > outputfile
 *
 *    The inputfile should match the output file, to check this run:
 *    $ md5sum inputfile outputfile
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

#include "zsocket.h"
#include "zfildes.h"

int nread = 0;
int nwrote = 0;
int finished = 0;

void zsocket_event_hdlr( int fd, int event ) {
	int n;
	char buf[1024];

	switch ( event ) {
		case ZS_EVT_ACCEPT_READY:
			fprintf( stderr, "Error: should not be accepting\n" );
			break;
		case ZS_EVT_READ_READY:
			//fprintf( stderr, "read\n" );
			n = zs_read( fd, buf, sizeof( buf ) );
			if ( n <= 0 ) {
				if ( n == -1 )
					perror( "zsocket_event_hdlr() error: zs_read" );
				else
					fprintf( stderr, "%d: EOF\n", fd );
				//zs_close( fd );
				zs_clr_read( fd );
			} else {
				nread += n;
				write( STDOUT_FILENO, buf, n );
			}

			if ( !zs_isset_write( fd ) && nread == nwrote ) {
				fprintf( stderr, "finished...closing.\n" );
				zs_close( fd );
				finished = 1;
			}
			break;
		case ZS_EVT_WRITE_READY:
			//fprintf( stderr, "Wrote %d bytes of data\n", (int)event.data.write.buffer_used );
			n = read( STDIN_FILENO, buf, sizeof( buf ) );
			if ( n <= 0 ) {
				if ( n == -1 )
					perror( "zsocket_event_hdlr() error: read" );
				//zs_close( fd );
				zs_clr_write( fd );
				return;
			} else nwrote += n;
			n = zs_write( fd, buf, n );
			if ( n == -1 ) {
				perror( "zsocket_event_hdlr() error: zs_write" );
				zs_close( fd );
			}
			break;
	}
}

int main( int argc, char **argv ) {
	fprintf( stderr, "Hello World, I'm a client.\n" );

	zs_init();

	int fd = zsocket( inet_addr( "127.0.0.1" ), 8080, ZSOCK_CONNECT, zsocket_event_hdlr, false );
	if ( fd < 0 ) {
		perror( "ERROR" );
		return EXIT_FAILURE;
	}

	zs_set_read( fd );
	zs_set_write( fd );

	do {
		while ( zs_select() );
	} while( !finished && zfd_select(2) );

	return EXIT_SUCCESS;
}
