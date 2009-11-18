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
	//TODO: Look for memleaks

	PyObject* zimr_module  = PyImport_ImportModule( "pyzimr.zimr" );
	PyObject* website_type = PyObject_GetAttrString( zimr_module, "website" );
	PyObject* website_obj  = PyObject_CallFunction( website_type, "s", website->url );

	PyObject* main_name   = PyString_FromString( "main" );
	PyObject* main_module = PyImport_Import( main_name );
	Py_DECREF( main_name );

	if ( PyObject_HasAttrString( main_module, "public_directory" ) ) {
		PyObject* public_dir = PyObject_GetAttrString( main_module, "public_directory" );
		PyObject_SetAttrString( website_obj, "public_directory", public_dir );
		Py_DECREF( public_dir );
	}

	if ( PyObject_HasAttrString( main_module, "connection_handler" ) ) {
		PyObject* connection_handler = PyObject_GetAttrString( main_module, "connection_handler" );
		PyObject_SetAttrString( website_obj, "connection_handler", connection_handler);
		Py_DECREF( connection_handler );
	}

	if ( !main_module )
		PyErr_Print();
}
