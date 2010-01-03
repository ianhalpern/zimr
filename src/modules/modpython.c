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

	char argv[][ 1 ] = { { "" } };

	PySys_SetArgv( 0, (char**) argv );

	/*if ( !PyRun_SimpleString(
		"import zimr\n"
		"from zimr.page_handlers import psp\n"
		"zimr.registerPageHandler( 'psp', psp.render )\n"
	) ) {
		PyErr_Print();
	}*/
}

void modzimr_destroy() {
	Py_Finalize();
}

void modzimr_website_init( website_t* website, int argc, char* argv[] ) {
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

	PyObject* zimr_module = NULL,* website_type = NULL,* website_obj = NULL,* psp_module = NULL,
	  * psp_render_func = NULL,* register_page_handler = NULL,* insert_default_page = NULL,
	  * func1_ret = NULL,* func2_ret = NULL;

	if (
	  !( zimr_module     = PyImport_ImportModule( "zimr" ) ) ||
	  !( website_type    = PyObject_GetAttrString( zimr_module, "website" ) ) ||
	  !( website_obj     = PyObject_CallFunction( website_type, "s", website->url ) ) ||
	  !( psp_module      = PyImport_ImportModule( "zimr.page_handlers.psp" ) ) ||
	  !( psp_render_func = PyObject_GetAttrString( psp_module, "render" ) ) ||
	  !( register_page_handler = PyObject_GetAttrString( website_obj, "registerPageHandler" ) ) ||
	  !( func1_ret       = PyObject_CallFunction( register_page_handler, "sO", "psp", psp_render_func ) ) ||
	  !( insert_default_page = PyObject_GetAttrString( website_obj, "insertDefaultPage" ) ) ||
	  !( func2_ret       = PyObject_CallFunction( insert_default_page, "s", "default.psp" ) )
	) {
		goto quit;
	}

	/*
	 website_type    = PyObject_GetAttrString( zimr_module, "website" );
	PyObject* website_obj     = PyObject_CallFunction( website_type, "s", website->url );

	PyObject* psp_module      = PyImport_ImportModule( "zimr.page_handlers.psp" );
	PyObject* psp_render_func = PyObject_GetAttrString( psp_module, "render" );
	PyObject* website_register_page_handler_func = PyObject_GetAttrString( website_obj, "registerPageHandler" );
	PyObject_CallFunction( website_register_page_handler_func, "sO", "psp", psp_render_func );
	PyObject* website_insert_default_page_func = PyObject_GetAttrString( website_obj, "insertDefaultPage" );
	PyObject_CallFunction( website_insert_default_page_func, "s", "default.psp" );
	*/
	if ( !fd ) goto quit;

	PyObject* main_module = PyImport_AddModule( "__main__" );
	PyObject* main_dict = PyModule_GetDict( main_module );

	if ( !PyRun_File( fd, filename, Py_file_input, main_dict, main_dict ) ) {
		goto quit;
	}

	if ( PyObject_HasAttrString( main_module, "connection_handler" ) ) {
		PyObject* connection_handler = PyObject_GetAttrString( main_module, "connection_handler" );
		PyObject_SetAttrString( website_obj, "connection_handler", connection_handler );
		Py_DECREF( connection_handler );
	}

quit:
	//Py_XDECREF( website_obj );
	Py_XDECREF( zimr_module );
	Py_XDECREF( psp_module );
	Py_XDECREF( website_type );
	Py_XDECREF( psp_render_func );
	Py_XDECREF( register_page_handler );
	Py_XDECREF( insert_default_page );
	Py_XDECREF( func1_ret );
	Py_XDECREF( func2_ret );

	if ( PyErr_Occurred() )
		PyErr_Print();
}
