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

#include "general.h"

static int srand_called = 0;

int randkey( ) {
	int key;

	if ( !srand_called ) {
		srand( time( 0 ) );
		srand_called = 1;
	}

	key = rand( );
	//TODO: bitshift only temporary, remember to change it back
	return key >> 24;
}

int url2int( const char* url ) {
	int h = 0, i = 0;

	for ( i = 0; i < strlen( url ); i++ ) {
		h = 31 * h + url[ i ];
	}

	return h;
}

void wait_for_kill( ) {
	sigset_t set;
	int sig;

	sigemptyset( &set );
	sigaddset( &set, SIGINT );
	sigprocmask( SIG_BLOCK, &set, NULL );

	if ( sigwait( &set, &sig ) == -1 )
		perror( "[error] wait_for_kill: sigwait() failed" );
	else if ( sig != 2 )
		fprintf( stderr, "[warning] wait_for_kill: sigwait() returned with sig: %d\n", sig );
}

int startswith( const char* s1, const char* s2 ) {
	size_t size;
	int i;

	size = strlen( s2 );

	if( size > strlen( s1 ) ) return 0;

	for ( i = 0; i < size; i++ )
		if ( *(s1 + i) != *(s2 + i) )
			return 0;

	return 1;
}
