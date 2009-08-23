/*   Poroda - Next Generation Web Server
 *
 *+  Copyright (c) 2009 Ian Halpern
 *@  http://Poroda.org
 *
 *   This file is part of Poroda.
 *
 *   Poroda is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Poroda is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Poroda.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "poroda.h"


/*int die() {

        char *err = NULL;
        strcpy(err, "gonner");
        return 0;
}*/

void empty_sighandler(){}

int main( int argc, char *argv[ ] ) {
	poroda_init( );

	signal( SIGTERM, empty_sighandler );
	signal( SIGINT, empty_sighandler );

	assert( poroda_cnf_load( ) );

//	die( );

	poroda_start( );

	poroda_shutdown( );
	return 0;
}

