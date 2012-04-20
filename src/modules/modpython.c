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
#include <sys/stat.h>

#include "zimr.h"
#include "dlog.h"

static PyThreadState* mainstate;

static bool err_occured = false;

bool modzimr_err_occured() {
	bool ret = err_occured;
	err_occured = false;
	return ret;
}

void modzimr_init() {

	// initialize thread support
	PyEval_InitThreads();

	Py_Initialize();
	mainstate = PyThreadState_Swap(NULL);
	PyEval_ReleaseLock();

	PyGILState_STATE gstate = PyGILState_Ensure();

	PyObject* local_path,* sys_path;

	if ( ( local_path = PyString_FromString( "" ) )
	&& ( sys_path   = PySys_GetObject( "path" ) ) ) {

		PyList_Insert( sys_path, 0, local_path );
		Py_DECREF( local_path );

	}

	char argv[][1] = { { "" } };

	PySys_SetArgv( 0, (char**) argv );

	if ( PyErr_Occurred() ) {
		PyErr_PrintEx(0);
		err_occured = true;
	}

	/*if ( !PyRun_SimpleString(
		"import zimr\n"
		"from zimr.page_handlers import psp\n"
		"zimr.registerPageHandler( 'psp', psp.render )\n"
	) ) {
		PyErr_Print();
	}*/

	PyGILState_Release( gstate );
}

void modzimr_destroy() {
	PyEval_AcquireLock();
	PyThreadState_Swap( mainstate );

	Py_Finalize();
}

void* modzimr_website_init( website_t* website, int argc, char* argv[] ) {
	char* modulename = NULL;
	struct stat   buffer;
	int i;

	PyObject* zimr_module = NULL,* website_type = NULL,* website_obj = NULL,* psp_module = NULL,
	  * psp_render_func = NULL,* register_page_handler = NULL,* insert_default_page = NULL,
	  * func1_ret = NULL,* func2_ret = NULL,* pyfilename = NULL,* module_path = NULL,* webapp_module = NULL;

	PyGILState_STATE gstate = PyGILState_Ensure();

	if ( argc )
		modulename = strdup( argv[0] );

	else if ( stat( "webapp.py", &buffer ) == 0 ) {
		modulename = strdup( "webapp" );
		argc = 1;
	}

	PyObject* pyarg,* sys_argv = PySys_GetObject( "argv" );

	for ( i = 0; i < argc; i++ ) {
		if ( !i ) {
			if ( modulename ) {
				pyarg = PyString_FromString( modulename );
				PyList_SetItem( sys_argv, i, pyarg );
			} else continue;
		} else {
			pyarg = PyString_FromString( argv[i] );
			PyList_Append( sys_argv, pyarg );
			Py_DECREF( pyarg );
		}

	}

	if ( modulename )
		if ( !( webapp_module = PyImport_ImportModule( modulename ) ) )
			goto quit;

	website_options_reset( website );

	if (
	  !( zimr_module     = PyImport_ImportModule( "zimr" ) ) ||
	  !( website_type    = PyObject_GetAttrString( zimr_module, "website" ) ) ||
	  !( website_obj     = PyObject_CallFunction( website_type, "ss", website->full_url, website->ip ) ) ||
	  !( psp_module      = PyImport_ImportModule( "zimr.page_handlers.psp" ) ) ||
	  !( psp_render_func = PyObject_GetAttrString( psp_module, "render" ) ) ||
	  !( register_page_handler = PyObject_GetAttrString( website_obj, "registerPageHandler" ) ) ||
	  !( func1_ret       = PyObject_CallFunction( register_page_handler, "sO", "psp", psp_render_func ) ) ||
	  !( insert_default_page = PyObject_GetAttrString( website_obj, "insertDefaultPage" ) ) ||
	  !( func2_ret       = PyObject_CallFunction( insert_default_page, "s", "default.psp" ) )
	) {
		goto quit;
	}

	zimr_website_insert_ignored_regex( website, "psp_cache" );

	if ( webapp_module ) {
	//PyObject_SetAttrString( PyObject_GetAttrString( zimr_module, "czimr" ), "webapp_module", webapp_module );

		if ( PyObject_HasAttrString( webapp_module, "connection_handler" ) ) {
			PyObject* connection_handler = PyObject_GetAttrString( webapp_module, "connection_handler" );
			PyObject_SetAttrString( website_obj, "connection_handler", connection_handler );
			Py_DECREF( connection_handler );
		}

		if ( PyObject_HasAttrString( webapp_module, "error_handler" ) ) {
			PyObject* error_handler = PyObject_GetAttrString( webapp_module, "error_handler" );
			PyObject_SetAttrString( website_obj, "error_handler", error_handler );
			Py_DECREF( error_handler );
		}
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
	Py_XDECREF( module_path );
	Py_XDECREF( pyfilename );
	Py_XDECREF( webapp_module );

	if ( modulename ) free( modulename );

	if ( PyErr_Occurred() ) {
		puts("");
		PyErr_PrintEx(0);
		puts("");
		err_occured = true;
	}

	PyGILState_Release( gstate );
	return NULL;
}
