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

#include <Python.h>
#include <stdio.h>
#include <unistd.h> 

#include "zimr.h"

void modzimr_init() {
	puts( "Hello from modpython!" );

	Py_Initialize();
	if ( !PyRun_SimpleString(
		"import sys\n"
		"sys.path.insert( 0, '' )\n"
		"from pyzimr import zimr\n"
		"from pyzimr.page_handlers import psp\n"
		"zimr.registerPageHandler( 'psp', psp.render )\n"
	) )
		PyErr_Print();
}

void modzimr_destroy() {
	Py_Finalize();
}

void modzimr_website_init( website_t* website ) {
	printf( "Hello website %s from modpython\n", website->url );

	PyObject* main_name,* main_module;

	main_name = PyString_FromString( "main" );
	main_module = PyImport_Import( main_name );
	Py_DECREF( main_name );
	//TODO: error check ^

	if ( !main_module )
		PyErr_Print();
}
