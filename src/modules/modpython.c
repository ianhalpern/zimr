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
	Py_Initialize();

	PyObject* local_path = PyString_FromString( "" );
	PyObject* sys_path   = PySys_GetObject( "path" );

	PyList_Insert( sys_path, 0, local_path );
	Py_DECREF( local_path );

	if ( !PyRun_SimpleString(
		"import zimr\n"
		"from zimr.page_handlers import psp\n"
		"zimr.registerPageHandler( 'psp', psp.render )\n"
	) ) {
		PyErr_Print();
	}
}

void modzimr_destroy() {
	Py_Finalize();
}

void modzimr_website_init( website_t* website, int argc, char* argv[] ) {
	int i;
	for ( i = 0; i < argc; i++ )
		puts( argv[ i ] );

	char* filename;
	FILE* fd;

	if ( !argc ) {
		filename = "main.py";
		fd = fopen( filename, "r" );
	}

	else {
		filename = argv[ 0 ];
		if ( !( fd = fopen( filename, "r" ) ) ) {
			fprintf( stderr, "%s: modpython could not open file %s.\n", strerror( errno ), filename );
			return;
		}
	}

	//TODO: Look for memleaks, errors, etc
	PyObject* zimr_module  = PyImport_ImportModule( "zimr" );
	PyObject* website_type = PyObject_GetAttrString( zimr_module, "website" );
	PyObject* website_obj  = PyObject_CallFunction( website_type, "s", website->url );

	if ( !fd ) return;

	PyObject* main_module = PyImport_AddModule( "__main__" );
	PyObject* main_dict = PyModule_GetDict( main_module );

	if ( !PyRun_File( fd, filename, Py_file_input, main_dict, main_dict ) ) {
		PyErr_Print();
		return;
	}

	if ( PyObject_HasAttrString( main_module, "connection_handler" ) ) {
		PyObject* connection_handler = PyObject_GetAttrString( main_module, "connection_handler" );
		PyObject_SetAttrString( website_obj, "connection_handler", connection_handler );
		Py_DECREF( connection_handler );
	}

}
