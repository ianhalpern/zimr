/*   Pacoda - Next Generation Web Server
 *
 *+  Copyright (c) 2009 Ian Halpern
 *@  http://Pacoda.org
 *
 *   This file is part of Pacoda.
 *
 *   Pacoda is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Pacoda is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Pacoda.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "pacoda.h"

void empty_sighandler(){}

int main( int argc, char *argv[ ] ) {
	pacoda_init( );

	signal( SIGTERM, empty_sighandler );
	signal( SIGINT,  empty_sighandler );

	char* cnf_path = NULL;
	if ( argc > 1 )
		cnf_path = argv[ 1 ];

	assert( pacoda_cnf_load( cnf_path ) );

	pacoda_start( );

	pacoda_shutdown( );
	return 0;
}

