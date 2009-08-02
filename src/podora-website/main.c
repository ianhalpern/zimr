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
#include <signal.h>

#include "podora.h"

void quitproc( ) {
	podora_shutdown( );
}

int main( int argc, char *argv[ ] ) {
	website_t* website;

	signal( SIGINT, quitproc );

	podora_init( );

	if ( argc < 2 ) {
		fprintf( stderr, "[fatal] website url was not provided.\n" );
		return EXIT_FAILURE;
	}
	website = podora_website_create( argv[ 1 ] );
	//podora_website_set_pubdir( website, "test_website/" );
	podora_website_enable( website );

	podora_start( );

	printf( "quit\n" );
	return 0;
}

