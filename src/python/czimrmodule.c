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
#include <structmember.h>

#include "zimr.h"
#include "dlog.h"

//static PyThreadState* mainstate;
//PyThreadState* _save = NULL;
static PyObject* m;
static void pyzimr_page_handler( connection_t* connection, const char* filepath, void* udata );
static bool _py_want_reload = false;

typedef struct {
	PyObject_HEAD
	PyObject* connection_handler;
	PyObject* error_handler;
	website_t* _website;
} pyzimr_website_t;

static void pyzimr_error_print( connection_t* connection ) {
	char error_message[ 8192 ] = "";
	size_t error_message_size = 0;

	PyObject* err_type = NULL,* err_value = NULL,* err_traceback = NULL,* err_traceback_lines = NULL,
	  * err_traceback_str = NULL,* traceback_module = NULL,* format_exc_func = NULL,* str_join = NULL;

	PyErr_Fetch( &err_type, &err_value, &err_traceback );
	PyErr_NormalizeException( &err_type, &err_value, &err_traceback );

	if (
	  ( traceback_module = PyImport_ImportModule( "traceback" ) ) &&
	  ( format_exc_func = PyObject_GetAttrString( traceback_module, "format_exception" ) ) &&
	  ( str_join = PyObject_GetAttrString( (PyObject*) &PyString_Type, "join" ) ) &&
	  ( err_traceback_lines = PyObject_CallFunction( format_exc_func, "OOO", err_type, err_value, err_traceback ) ) &&
	  ( err_traceback_str = PyObject_CallFunction( str_join, "sO", "", err_traceback_lines ) ) //&&
	//  ( err_traceback_str = PyObject_Str( err_traceback_str ) )
	) {
		error_message_size = snprintf( error_message, sizeof( error_message ), "<pre>%s</pre>", PyString_AsString( err_traceback_str ) );
		dlog( stderr, "\n%s", PyString_AsString( err_traceback_str ) );
	} else {
		if ( PyErr_Occurred() )
			PyErr_PrintEx(0);
	}

	if ( connection )
		zimr_connection_send_error( connection, 500, error_message, error_message_size );

	Py_XDECREF( err_type );
	Py_XDECREF( err_value );
	Py_XDECREF( err_traceback );
	Py_XDECREF( err_traceback_lines );
	Py_XDECREF( err_traceback_str );
	Py_XDECREF( traceback_module );
	Py_XDECREF( format_exc_func );
	Py_XDECREF( str_join );
}

/********** START OF pyzimr_website_options_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	website_t* _website;
	bool (*_get)( website_t*, unsigned int );
	void (*_set)( website_t*, unsigned int, bool );
} pyzimr_website_options_t;

static PyTypeObject pyzimr_website_options_type;

static pyzimr_website_options_t* pyzimr_website_options_create( website_t* _website,
 bool (*_get)( website_t*, unsigned int ),
 void (*_set)( website_t*, unsigned int, bool ) ) {

	pyzimr_website_options_t* options = (pyzimr_website_options_t*) pyzimr_website_options_type.tp_new( &pyzimr_website_options_type, NULL, NULL );
	options->_website = _website;
	options->_get = _get;
	options->_set = _set;
	return options;
}

/*static int pyzimr_website_options__maplen__( pyzimr_website_options_t* self ) {
	return list_size( (list_t*)self->_headers );
}*/

static unsigned int pyzimr_website_options_get_option_from_name( char* option ) {
	if ( strcmp( option, "PARSE_MULTIPART" ) == 0 )
		return WS_OPTION_PARSE_MULTIPARTS;
	return 0;
}

static PyObject* pyzimr_website_options__mapget__( pyzimr_website_options_t* self, PyObject* key ) {
	char* option = PyString_AsString( key );
	return PyBool_FromLong( self->_get( self->_website, pyzimr_website_options_get_option_from_name( option ) ) );
}

static int pyzimr_website_options__mapset__( pyzimr_website_options_t* self, PyObject* key, PyObject* value ) {
	char* option = PyString_AsString( key );

	if ( !PyBool_Check( value ) ) {
		PyErr_SetString( PyExc_TypeError, "value must be boolean" );
		return 0;
	}

	self->_set( self->_website, pyzimr_website_options_get_option_from_name( option ), PyInt_AsLong( value ) );
	return 0;
}

static PyMappingMethods pyzimr_website_options_as_map = {
	(inquiry)       NULL, //pyzimr_website_options__maplen__, /*mp_length*/
	(binaryfunc)    pyzimr_website_options__mapget__, /*mp_subscript*/
	(objobjargproc) pyzimr_website_options__mapset__, /*mp_ass_subscript*/
};

static PyMethodDef pyzimr_website_options_methods[] = {
	{ NULL }  /* Sentinel */
};

static PyTypeObject pyzimr_website_options_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"zimr.website_options",             /*tp_name*/
	sizeof( pyzimr_website_options_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	0,//(destructor) pyzimr_website_dealloc, /*tp_dealloc*/
	0,                         /*tp_print*/
	0,                         /*tp_getattr*/
	0,                         /*tp_setattr*/
	0,                         /*tp_compare*/
	0,                         /*tp_repr*/
	0,                         /*tp_as_number*/
	0,                         /*tp_as_sequence*/
	&pyzimr_website_options_as_map,     /*tp_as_mapping*/
	0,                         /*tp_hash */
	0,                         /*tp_call*/
	0,                         /*tp_str*/
	0,                         /*tp_getattro*/
	0,                         /*tp_setattro*/
	0,                         /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT, /*tp_flags*/
	"zimr website options",           /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	pyzimr_website_options_methods,             /* tp_methods */
	0,             /* tp_members */
	0,           /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,//(initproc) pyzimr_website_init,      /* tp_init */
	0,                         /* tp_alloc */
	PyType_GenericNew,                 /* tp_new */
};

/********** START OF pyzimr_headers_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	headers_t* _headers;
} pyzimr_headers_t;

static PyTypeObject pyzimr_headers_type;

static pyzimr_headers_t* pyzimr_headers_create( headers_t* _headers ) {
	pyzimr_headers_t* headers = (pyzimr_headers_t*) pyzimr_headers_type.tp_new( &pyzimr_headers_type, NULL, NULL );
	headers->_headers = _headers;
	return headers;
}

static PyObject* pyzimr_headers_keys( pyzimr_headers_t* self ) {
	PyObject* keys = PyTuple_New( self->_headers->num );

	int i;
	for ( i = 0; i < self->_headers->num; i++ ) {
		PyTuple_SetItem( keys, i, PyString_FromString( self->_headers->list[ i ].name ) );
	}

	return keys;
}

static int pyzimr_headers__maplen__( pyzimr_headers_t* self ) {
	return list_size( (list_t*)self->_headers );
}

static PyObject* pyzimr_headers__mapget__( pyzimr_headers_t* self, PyObject* key ) {
	header_t* header = headers_get_header( self->_headers, PyString_AsString( key ) );

	if ( !header ) Py_RETURN_NONE;

	return PyString_FromString( header->value );
}

static int pyzimr_headers__mapset__( pyzimr_headers_t* self, PyObject* key, PyObject* value ) {
	headers_set_header( self->_headers, PyString_AsString( key ), PyString_AsString( value ) );
	return 0;
}

static PyMappingMethods pyzimr_headers_as_map = {
	(inquiry)       pyzimr_headers__maplen__, /*mp_length*/
	(binaryfunc)    pyzimr_headers__mapget__, /*mp_subscript*/
	(objobjargproc) pyzimr_headers__mapset__, /*mp_ass_subscript*/
};

static PyMethodDef pyzimr_headers_methods[] = {
	{ "keys", (PyCFunction) pyzimr_headers_keys, METH_NOARGS, "set the response status" },
	{ NULL }  /* Sentinel */
};

static PyTypeObject pyzimr_headers_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"zimr.headers",             /*tp_name*/
	sizeof( pyzimr_headers_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	0,//(destructor) pyzimr_website_dealloc, /*tp_dealloc*/
	0,                         /*tp_print*/
	0,                         /*tp_getattr*/
	0,                         /*tp_setattr*/
	0,                         /*tp_compare*/
	0,                         /*tp_repr*/
	0,                         /*tp_as_number*/
	0,                         /*tp_as_sequence*/
	&pyzimr_headers_as_map,     /*tp_as_mapping*/
	0,                         /*tp_hash */
	0,                         /*tp_call*/
	0,                         /*tp_str*/
	0,                         /*tp_getattro*/
	0,                         /*tp_setattro*/
	0,                         /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT, /*tp_flags*/
	"zimr headers dict",           /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	pyzimr_headers_methods,             /* tp_methods */
	0,             /* tp_members */
	0,           /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,//(initproc) pyzimr_website_init,      /* tp_init */
	0,                         /* tp_alloc */
	PyType_GenericNew,                 /* tp_new */
};

/********** START OF pyzimr_response_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	PyObject* headers;
	response_t* _response;
} pyzimr_response_t;

static PyTypeObject pyzimr_response_type;

static pyzimr_response_t* pyzimr_response_create( response_t* _response ) {
	pyzimr_response_t* response = (pyzimr_response_t*) pyzimr_response_type.tp_new( &pyzimr_response_type, NULL, NULL );

	response->headers = (PyObject*) pyzimr_headers_create( &_response->headers );
	response->_response = _response;

	return response;
}

static void pyzimr_response_dealloc( pyzimr_response_t* self ) {
	Py_DECREF( self->headers );
	//connection_free( self->_connection );
	self->ob_type->tp_free( (PyObject*) self );
}

static PyObject* pyzimr_response_set_header( PyObject* self, PyObject* args ) {
	char* name,* value;

	if ( !PyArg_ParseTuple( args, "ss", &name, &value ) ) {
		PyErr_SetString( PyExc_TypeError, "name and value paramater must be passed" );
		return NULL;
	}
	headers_set_header( &( (pyzimr_response_t*) self )->_response->headers, name, value );

	Py_RETURN_NONE;
}

static PyObject* pyzimr_response_set_status( PyObject* self, PyObject* args ) {
	int status_code;

	if ( !PyArg_ParseTuple( args, "i", &status_code ) ) {
		PyErr_SetString( PyExc_TypeError, "status_code paramater must be passed" );
		return NULL;
	}
	response_set_status( ( (pyzimr_response_t*) self )->_response, status_code );

	Py_RETURN_NONE;
}

static PyMethodDef pyzimr_response_methods[] = {
	{ "setStatus", (PyCFunction) pyzimr_response_set_status, METH_VARARGS, "set the response status" },
	{ "setHeader", (PyCFunction) pyzimr_response_set_header, METH_VARARGS, "set a response header" },
	{ NULL }  /* Sentinel */
};

static PyMemberDef pyzimr_response_members[] = {
	{ "headers", T_OBJECT_EX, offsetof( pyzimr_response_t, headers ), RO, "params object for the request" },
	{ NULL }  /* Sentinel */
};

static PyTypeObject pyzimr_response_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"zimr.response",             /*tp_name*/
	sizeof( pyzimr_response_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor) pyzimr_response_dealloc, /*tp_dealloc*/
	0,                         /*tp_print*/
	0,                         /*tp_getattr*/
	0,                         /*tp_setattr*/
	0,                         /*tp_compare*/
	0,                         /*tp_repr*/
	0,                         /*tp_as_number*/
	0,                         /*tp_as_sequence*/
	0,                         /*tp_as_mapping*/
	0,                         /*tp_hash */
	0,                         /*tp_call*/
	0,                         /*tp_str*/
	0,                         /*tp_getattro*/
	0,                         /*tp_setattro*/
	0,                         /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT, /*tp_flags*/
	"zimr response object",           /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	pyzimr_response_methods,             /* tp_methods */
	pyzimr_response_members,             /* tp_members */
	0,//pyzimr_website_getseters,           /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,//(initproc) pyzimr_website_init,      /* tp_init */
	0,                         /* tp_alloc */
	PyType_GenericNew,                 /* tp_new */
};

/********** START OF pyzimr_params_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	params_t* _params;
	int type;
} pyzimr_params_t;

static PyTypeObject pyzimr_params_type;

static pyzimr_params_t* pyzimr_params_create( params_t* _params, int type ) {
	pyzimr_params_t* params = (pyzimr_params_t*) pyzimr_params_type.tp_new( &pyzimr_params_type, NULL, NULL );
	params->_params = _params;
	params->type = type;
	return params;
}

static int pyzimr_params__maplen__( pyzimr_params_t* self ) {
	size_t s = 0;
	if ( !self->type )
		s = list_size( self->_params );
	else {
		int i;
		for ( i = 0; i < list_size( (list_t*)self->_params ); i++ ) {
			param_t* param = list_get_at( (list_t*)self->_params, i );
			if ( self->type == param->type ) s++;
		}
	}
	return s;
}

static int pyzimr_params__mapset__( pyzimr_params_t* self, PyObject* key, PyObject* value ) {
	params_set_param( self->_params, PyString_AsString( key ), PyString_AsString( PyObject_Str( value ) ) );
	return 0;
}

static PyObject* pyzimr_params_keys( pyzimr_params_t* self ) {
	PyObject* keys = PyTuple_New( pyzimr_params__maplen__( self ) );

	int i;
	for ( i = 0; i < list_size( (list_t*)self->_params ); i++ ) {
		param_t* param = list_get_at( (list_t*)self->_params, i );
		if ( self->type && self->type != param->type ) continue;
		PyTuple_SetItem( keys, i, PyString_FromString( param->name ) );
	}

	return keys;
}

static PyObject* pyzimr_params_items( pyzimr_params_t* self ) {
	PyObject* items = PyTuple_New( pyzimr_params__maplen__( self ) );

	int i;
	for ( i = 0; i < list_size( (list_t*)self->_params ); i++ ) {
		param_t* param = list_get_at( (list_t*)self->_params, i );
		if ( self->type && self->type != param->type ) continue;
		PyObject* key_val = PyTuple_New( 2 );
		PyTuple_SetItem( key_val, 0, PyString_FromString( param->name ) );
		PyTuple_SetItem( key_val, 1, PyString_FromString( param->value ) );
		PyTuple_SetItem( items, i, key_val );
	}

	return items;
}

static PyObject* pyzimr_params__mapget__( pyzimr_params_t* self, PyObject* key ) {
	param_t* param = params_get_param( self->_params, PyString_AsString( key ) );

	if ( !param ) Py_RETURN_NONE;

	return PyString_FromString( param->value );
}
/*
static int pyzimr_params__mapset__( pyzimr_cookies_t* self, PyObject* key, PyObject* value ) {
	cookies_set_cookie( self->_cookies, PyString_AsString( key ), PyString_AsString( value ), 0, NULL, NULL );
	return 0;
}*/

static PyMappingMethods pyzimr_params_as_map = {
	(inquiry)       pyzimr_params__maplen__, /*mp_length*/
	(binaryfunc)    pyzimr_params__mapget__, /*mp_subscript*/
	(objobjargproc) pyzimr_params__mapset__, /*mp_ass_subscript*/
};

static PyMethodDef pyzimr_params_methods[] = {
	{ "keys", (PyCFunction) pyzimr_params_keys, METH_NOARGS, "set the response status" },
	{ "items", (PyCFunction) pyzimr_params_items, METH_NOARGS, "set the response status" },
	{ NULL }  /* Sentinel */
};

static PyTypeObject pyzimr_params_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"zimr.params",             /*tp_name*/
	sizeof( pyzimr_params_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	0,//(destructor) pyzimr_website_dealloc, /*tp_dealloc*/
	0,                         /*tp_print*/
	0,                         /*tp_getattr*/
	0,                         /*tp_setattr*/
	0,                         /*tp_compare*/
	0,                         /*tp_repr*/
	0,                         /*tp_as_number*/
	0,                         /*tp_as_sequence*/
	&pyzimr_params_as_map,     /*tp_as_mapping*/
	0,                         /*tp_hash */
	0,                         /*tp_call*/
	0,                         /*tp_str*/
	0,                         /*tp_getattro*/
	0,                         /*tp_setattro*/
	0,                         /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT, /*tp_flags*/
	"zimr params dict",           /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	pyzimr_params_methods,             /* tp_methods */
	0,             /* tp_members */
	0,           /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,//(initproc) pyzimr_website_init,      /* tp_init */
	0,                         /* tp_alloc */
	PyType_GenericNew,                 /* tp_new */
};

/********** START OF pyzimr_request_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	PyObject* params;
	PyObject* get_params;
	PyObject* post_params;
	PyObject* headers;
	PyObject* parts;
	request_t* _request;
} pyzimr_request_t;

static PyTypeObject pyzimr_request_type;

static pyzimr_request_t* pyzimr_request_create( request_t* _request ) {
	pyzimr_request_t* self = (pyzimr_request_t*) pyzimr_request_type.tp_new( &pyzimr_request_type, NULL, NULL );

	self->params      = (PyObject*) pyzimr_params_create( &_request->params, 0 );
	self->get_params  = (PyObject*) pyzimr_params_create( &_request->params, PARAM_TYPE_GET );
	self->post_params = (PyObject*) pyzimr_params_create( &_request->params, PARAM_TYPE_POST );
	self->headers     = (PyObject*) pyzimr_headers_create( &_request->headers );

	int i = -1;
	while ( _request->parts[++i] );

	self->parts = PyTuple_New(i);

	for ( i=0; _request->parts[i]; i++ ) {
		PyObject* part = (PyObject*) pyzimr_request_create( _request->parts[i] );
		Py_INCREF( part );
		PyTuple_SetItem( self->parts, i, part );
	}

	self->_request = _request;

	return self;
}

static void pyzimr_request_dealloc( pyzimr_request_t* self ) {
	Py_DECREF( self->params );
	Py_DECREF( self->get_params );
	Py_DECREF( self->post_params );
	Py_DECREF( self->headers );

	int i;
	for ( i=0; i < PyTuple_Size( self->parts ); i++ )
		Py_DECREF( PyTuple_GetItem( self->parts, i ) );

	Py_DECREF( self->parts );
	//connection_free( self->_connection );
	self->ob_type->tp_free( (PyObject*) self );
}

static PyObject* pyzimr_request_get_param( pyzimr_request_t* self, PyObject* args ) {
	const char* param_name;
	param_t* param;

	if ( !PyArg_ParseTuple( args, "s", &param_name ) ) {
		PyErr_SetString( PyExc_TypeError, "param_name paramater must be passed" );
		return NULL;
	}

	param = params_get_param( &( (pyzimr_request_t*) self )->_request->params, param_name );

	if ( !param )
		Py_RETURN_NONE;

	return PyString_FromString( param->value );
}

static PyObject* pyzimr_request_get_header( pyzimr_request_t* self, PyObject* args ) {
	const char* header_name;
	header_t* header;

	if ( !PyArg_ParseTuple( args, "s", &header_name ) ) {
		PyErr_SetString( PyExc_TypeError, "header_name paramater must be passed" );
		return NULL;
	}

	header = headers_get_header( &( (pyzimr_request_t*) self )->_request->headers, (char*) header_name );

	if ( !header )
		Py_RETURN_NONE;

	return PyString_FromString( header->value );
}

static PyObject* pyzimr_request_get_url( pyzimr_request_t* self, void* closure ) {
	if ( !self->_request->url ) Py_RETURN_NONE;
	return PyString_FromString( self->_request->url );
}

static PyObject* pyzimr_request_get_post_body( pyzimr_request_t* self, void* closure ) {
	if ( !self->_request->post_body ) Py_RETURN_NONE;
	//if ( self->_request->charset )
	return PyUnicode_Decode( self->_request->post_body, self->_request->post_body_len, self->_request->charset, "strict" );
	//return PyString_FromString( self->_request->post_body );
}

static PyObject* pyzimr_request_get_method( pyzimr_request_t* self, void* closure ) {
	return PyString_FromString( HTTP_TYPE( self->_request->type ) );
}

static PyMemberDef pyzimr_request_members[] = {
	{ "params", T_OBJECT_EX, offsetof( pyzimr_request_t, params ), RO, "params object for the request" },
	{ "get_params", T_OBJECT_EX, offsetof( pyzimr_request_t, get_params ), RO, "get params object for the request" },
	{ "post_params", T_OBJECT_EX, offsetof( pyzimr_request_t, post_params ), RO, "post params object for the request" },
	{ "headers", T_OBJECT_EX, offsetof( pyzimr_request_t, headers ), RO, "headers object for the request" },
	{ "parts", T_OBJECT_EX, offsetof( pyzimr_request_t, parts ), RO, "request multiparts list" },
	{ NULL }  /* Sentinel */
};

static PyMethodDef pyzimr_request_methods[] = {
	{ "getParam", (PyCFunction) pyzimr_request_get_param, METH_VARARGS, "get a GET/POST query string parameter value" },
	{ "getHeader", (PyCFunction) pyzimr_request_get_header, METH_VARARGS, "get a request header value" },
	{ NULL }  /* Sentinel */
};

static PyGetSetDef pyzimr_request_getseters[] = {
	{
	  "url",
	  (getter) pyzimr_request_get_url,
	  0,//(setter) pypdora_response_set_public_directory,
	  "the requested url", NULL
	},
	{
	  "post_body",
	  (getter) pyzimr_request_get_post_body,
	  0,//(setter) pypdora_response_set_public_directory,
	  "the body of a post request", NULL
	},
	{
	  "method",
	  (getter) pyzimr_request_get_method,
	  0,//(setter) pypdora_response_set_public_directory,
	  "the body of a post request", NULL
	},
	{ NULL }  /* Sentinel */
};

static PyTypeObject pyzimr_request_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"zimr.request",             /*tp_name*/
	sizeof( pyzimr_request_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor) pyzimr_request_dealloc, /*tp_dealloc*/
	0,                         /*tp_print*/
	0,                         /*tp_getattr*/
	0,                         /*tp_setattr*/
	0,                         /*tp_compare*/
	0,                         /*tp_repr*/
	0,                         /*tp_as_number*/
	0,                         /*tp_as_sequence*/
	0,                         /*tp_as_mapping*/
	0,                         /*tp_hash */
	0,                         /*tp_call*/
	0,                         /*tp_str*/
	0,                         /*tp_getattro*/
	0,                         /*tp_setattro*/
	0,                         /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT, /*tp_flags*/
	"zimr request object",           /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	pyzimr_request_methods,             /* tp_methods */
	pyzimr_request_members,             /* tp_members */
	pyzimr_request_getseters,           /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,//(initproc) pyzimr_website_init,      /* tp_init */
	0,                         /* tp_alloc */
	PyType_GenericNew,                 /* tp_new */
};

/********** START OF pyzimr_cookies_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	cookies_t* _cookies;
} pyzimr_cookies_t;

static PyTypeObject pyzimr_cookies_type;

static pyzimr_cookies_t* pyzimr_cookies_create( cookies_t* _cookies ) {
	pyzimr_cookies_t* cookies = (pyzimr_cookies_t*) pyzimr_cookies_type.tp_new( &pyzimr_cookies_type, NULL, NULL );
	cookies->_cookies = _cookies;
	return cookies;
}

static PyObject* pyzimr_cookies_keys( pyzimr_cookies_t* self ) {
	PyObject* keys = PyTuple_New( self->_cookies->num );

	int i;
	for ( i = 0; i < self->_cookies->num; i++ ) {
		PyTuple_SetItem( keys, i, PyString_FromString( self->_cookies->list[ i ].name ) );
	}

	return keys;
}

static int pyzimr_cookies___maplen__( pyzimr_cookies_t* self ) {
	return self->_cookies->num;
}

static PyObject* pyzimr_cookies___mapget__( pyzimr_cookies_t* self, PyObject* key ) {
	cookie_t* cookie = cookies_get_cookie( self->_cookies, PyString_AsString( key ) );

	if ( !cookie ) Py_RETURN_NONE;

	return PyString_FromString( cookie->value );
}

static int pyzimr_cookies___mapset__( pyzimr_cookies_t* self, PyObject* key, PyObject* value ) {
	if ( !value )
		cookies_del_cookie( self->_cookies, PyString_AsString( key ) );
	else
		cookies_set_cookie( self->_cookies, PyString_AsString( key ), PyString_AsString( value ), 0, 0, NULL, NULL, 0, 0 );
	return 0;
}

static PyMappingMethods pyzimr_cookies_as_map = {
	(inquiry)       pyzimr_cookies___maplen__, /*mp_length*/
	(binaryfunc)    pyzimr_cookies___mapget__, /*mp_subscript*/
	(objobjargproc) pyzimr_cookies___mapset__, /*mp_ass_subscript*/
};

static PyMethodDef pyzimr_cookies_methods[] = {
	{ "keys", (PyCFunction) pyzimr_cookies_keys, METH_NOARGS, "set the response status" },
	{ NULL }  /* Sentinel */
};

static PyTypeObject pyzimr_cookies_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"zimr.cookies",             /*tp_name*/
	sizeof( pyzimr_cookies_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	0,//(destructor) pyzimr_website_dealloc, /*tp_dealloc*/
	0,                         /*tp_print*/
	0,                         /*tp_getattr*/
	0,                         /*tp_setattr*/
	0,                         /*tp_compare*/
	0,                         /*tp_repr*/
	0,                         /*tp_as_number*/
	0,                         /*tp_as_sequence*/
	&pyzimr_cookies_as_map,     /*tp_as_mapping*/
	0,                         /*tp_hash */
	0,                         /*tp_call*/
	0,                         /*tp_str*/
	0,                         /*tp_getattro*/
	0,                         /*tp_setattro*/
	0,                         /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT, /*tp_flags*/
	"zimr cookies dict",           /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	pyzimr_cookies_methods,             /* tp_methods */
	0,             /* tp_members */
	0,           /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,//(initproc) pyzimr_website_init,      /* tp_init */
	0,                         /* tp_alloc */
	PyType_GenericNew,                 /* tp_new */
};

/********** START OF pyzimr_connection_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	PyObject* client; //TODO: temporary
	PyObject* response;
	PyObject* request;
	PyObject* website;
	PyObject* cookies;
	PyObject* onClose;
	connection_t* _connection;
	int connid;
} pyzimr_connection_t;

static PyTypeObject pyzimr_connection_type;

static pyzimr_connection_t* pyzimr_connection_create( connection_t* _connection ) {
	pyzimr_connection_t* connection = (pyzimr_connection_t*) pyzimr_connection_type.tp_new( &pyzimr_connection_type, NULL, NULL );

	Py_INCREF( connection->client  = Py_None );
	connection->response = (PyObject*) pyzimr_response_create( &_connection->response );
	connection->request  = (PyObject*) pyzimr_request_create( &_connection->request );
	Py_INCREF( connection->website = (PyObject*) ( (website_data_t*) _connection->website->udata )->udata );
	connection->cookies  = (PyObject*) pyzimr_cookies_create( &_connection->cookies );
	Py_INCREF( connection->onClose = Py_None );
	connection->_connection = _connection;
	connection->_connection->udata = connection;
	connection->connid = _connection->sockfd;

	return connection;
}

static void pyzimr_connection_dealloc( pyzimr_connection_t* self ) {
	Py_DECREF( self->client );
	Py_DECREF( self->response );
	Py_DECREF( self->request );
	Py_DECREF( self->website );
	Py_DECREF( self->cookies );
	Py_DECREF( self->onClose );
	//printf( "dealloc connection 0x%x\n", self );
	//connection_free( self->_connection );
	self->ob_type->tp_free( (PyObject*) self );
}

static void pyzimr_connection_onclose_event( connection_t* _connection, pyzimr_connection_t* connection ) {
	PyGILState_STATE gstate = PyGILState_Ensure();

	if ( PyCallable_Check( connection->onClose ) )
		PyObject_CallFunction( connection->onClose, "O", connection );
		//PyObject_CallFunction( connection->onClose, NULL );

	Py_DECREF( connection );
	PyGILState_Release( gstate );
}

static PyObject* pyzimr_connection_send( pyzimr_connection_t* self, PyObject* args ) {
	PyObject* message;
	void* message_ptr;
	Py_ssize_t message_len;

	if ( !PyArg_ParseTuple( args, "O", &message ) ) {
		PyErr_SetString( PyExc_TypeError, "message paramater must be passed" );
		return NULL;
	}

	if ( PyObject_AsReadBuffer( message, (const void**) &message_ptr, (Py_ssize_t*) &message_len ) ) {
		PyErr_SetString( PyExc_TypeError, "invalid message paramater" );
		return NULL;
	}

	if ( !zimr_website_connection_sent( ((pyzimr_website_t*)self->website)->_website->sockfd, self->connid ) )
		zimr_connection_send( self->_connection, message_ptr, message_len );

	Py_RETURN_NONE;
}

static PyObject* pyzimr_connection_send_error( pyzimr_connection_t* self, PyObject* args ) {
	PyObject* message = NULL;
	void* message_ptr = NULL;
	int error_code;
	Py_ssize_t message_len = 0;

	if ( !PyArg_ParseTuple( args, "i|O", &error_code, &message ) ) {
		PyErr_SetString( PyExc_TypeError, "html error code must be passed" );
		return NULL;
	}

	if ( message && PyObject_AsReadBuffer( message, (const void**) &message_ptr, (Py_ssize_t*) &message_len ) ) {
		PyErr_SetString( PyExc_TypeError, "invalid message paramater" );
		return NULL;
	}

	if ( !zimr_website_connection_sent( ((pyzimr_website_t*)self->website)->_website->sockfd, self->connid ) )
		zimr_connection_send_error( self->_connection, error_code, message_ptr, message_len );

	Py_RETURN_NONE;
}

static PyObject* pyzimr_connection_send_file( pyzimr_connection_t* self, PyObject* args ) {
	char* filename = self->_connection->request.url;
	unsigned char use_pubdir = 1;

	if ( !PyArg_ParseTuple( args, "|sb", &filename, &use_pubdir ) ) {
		PyErr_SetString( PyExc_TypeError, "the filename paramater must be passed" );
		return NULL;
	}

	if ( !zimr_website_connection_sent( ((pyzimr_website_t*)self->website)->_website->sockfd, self->connid ) )
		zimr_connection_send_file( self->_connection, (char*) filename, use_pubdir );

	Py_RETURN_NONE;
}

static PyObject* pyzimr_connection_redirect( pyzimr_connection_t* self, PyObject* args ) {
	const char* redir_url = NULL;

	if ( !PyArg_ParseTuple( args, "s", &redir_url ) ) {
		PyErr_SetString( PyExc_TypeError, "the redir_url paramater must be passed" );
		return NULL;
	}

	if ( !zimr_website_connection_sent( ((pyzimr_website_t*)self->website)->_website->sockfd, self->connid ) )
		zimr_connection_send_redirect( self->_connection, (char*) redir_url );

	Py_RETURN_NONE;
}

static PyObject* pyzimr_connection_get_cookie( pyzimr_connection_t* self, PyObject* args ) {
	const char* cookie_name;
	cookie_t* cookie;

	if ( !PyArg_ParseTuple( args, "s", &cookie_name ) ) {
		PyErr_SetString( PyExc_TypeError, "the cookie_name must be passes" );
		return NULL;
	}

	cookie = cookies_get_cookie( &self->_connection->cookies, cookie_name );

	if ( !cookie )
		Py_RETURN_NONE;

	return PyString_FromString( cookie->value );
}

static PyObject* pyzimr_connection_set_cookie( pyzimr_connection_t* self, PyObject* args, PyObject* kwargs ) {
	static char* kwlist[] = { "name", "value", "expires", "max_age", "domain", "path", "http_only", "secure", NULL };
	const char* cookie_name,* cookie_value,* cookie_domain = "",* cookie_path = "";
	time_t expires = 0;
	int max_age = 0;
	bool secure = false, http_only = false;

	if ( !PyArg_ParseTupleAndKeywords( args, kwargs, "s|siissbb", kwlist, &cookie_name, &cookie_value, &expires, &max_age, &cookie_domain, &cookie_path, &http_only, &secure ) ) {
		PyErr_SetString( PyExc_TypeError, "the cookie_name must be passes" );
		return NULL;
	}

	cookies_set_cookie( &self->_connection->cookies, cookie_name, cookie_value, expires, max_age, cookie_domain, cookie_path, http_only, secure );

	Py_RETURN_NONE;
}

static PyObject* pyzimr_connection_get_hostname( pyzimr_connection_t* self, void* closure ) {
	return PyString_FromString( self->_connection->hostname );
}

static PyObject* pyzimr_connection_get_ip( pyzimr_connection_t* self, void* closure ) {
	return PyString_FromString( inet_ntoa( self->_connection->ip ) );
}

static PyMemberDef pyzimr_connection_members[] = {
	{ "client", T_OBJECT_EX, offsetof( pyzimr_connection_t, client ), 0, "temporary object" }, //TODO: temporary
	{ "response", T_OBJECT_EX, offsetof( pyzimr_connection_t, response ), RO, "response object of this connection" },
	{ "request", T_OBJECT_EX, offsetof( pyzimr_connection_t, request ), RO, "request object of this connection" },
	{ "website", T_OBJECT_EX, offsetof( pyzimr_connection_t, website ), RO, "website object from which the request originated" },
	{ "cookies", T_OBJECT_EX, offsetof( pyzimr_connection_t, cookies ), RO, "cookies object for the connection" },
	{ "onClose", T_OBJECT_EX, offsetof( pyzimr_connection_t, onClose ), 0, "event to be fired when the connection is closed." },
	{ NULL }  /* Sentinel */
};

static PyMethodDef pyzimr_connection_methods[] = {
	{ "setCookie", (PyCFunction) pyzimr_connection_set_cookie, METH_VARARGS | METH_KEYWORDS, "set a cookie" },
	{ "getCookie", (PyCFunction) pyzimr_connection_get_cookie, METH_VARARGS, "get a value of a cookie" },
	{ "send", (PyCFunction) pyzimr_connection_send, METH_VARARGS, "send a string as the response to the connection" },
	{ "sendError", (PyCFunction) pyzimr_connection_send_error, METH_VARARGS, "send a string as the response to the connection" },
	{ "sendFile", (PyCFunction) pyzimr_connection_send_file, METH_VARARGS, "send a file as a response to the connection" },
	{ "redirect", (PyCFunction) pyzimr_connection_redirect, METH_VARARGS, "redirect the connection to a different url" },
	{ NULL }  /* Sentinel */
};

static PyGetSetDef pyzimr_connection_getseters[] = {
	{
	  "hostname",
	  (getter) pyzimr_connection_get_hostname,
	  0,
	  "the hostname of the connection", NULL
	},
	{
	  "ip",
	  (getter) pyzimr_connection_get_ip,
	  0,
	  "the ip address of the connection", NULL
	},
	{ NULL }  /* Sentinel */
};

static PyTypeObject pyzimr_connection_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"zimr.connection",             /*tp_name*/
	sizeof( pyzimr_connection_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor) pyzimr_connection_dealloc, /*tp_dealloc*/
	0,                         /*tp_print*/
	0,                         /*tp_getattr*/
	0,                         /*tp_setattr*/
	0,                         /*tp_compare*/
	0,                         /*tp_repr*/
	0,                         /*tp_as_number*/
	0,                         /*tp_as_sequence*/
	0,                         /*tp_as_mapping*/
	0,                         /*tp_hash */
	0,                         /*tp_call*/
	0,                         /*tp_str*/
	0,                         /*tp_getattro*/
	0,                         /*tp_setattro*/
	0,                         /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT, /*tp_flags*/
	"zimr website connection object",           /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	pyzimr_connection_methods,             /* tp_methods */
	pyzimr_connection_members,             /* tp_members */
	pyzimr_connection_getseters,           /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,//(initproc) pyzimr_website_init,      /* tp_init */
	0,                         /* tp_alloc */
	PyType_GenericNew,                 /* tp_new */
};


/********** START OF pyzimr_website_t *******/
/**********************************************/

// struct at top of file

static void pyzimr_website_connection_handler( connection_t* _connection ) {
	PyGILState_STATE gstate = PyGILState_Ensure();

	// Reload webapp module if exists
	/*if ( PyObject_HasAttrString( m, "webapp_module" ) ) {
		puts("reload webapp module" );
		PyImport_ReloadModule( PyObject_GetAttrString( m, "webapp_module" ) );
	} else puts("no webapp module" );*/
	//

	pyzimr_connection_t* connection = pyzimr_connection_create( _connection );

	Py_INCREF( connection );
	zimr_connection_set_onclose_event( _connection, pyzimr_connection_onclose_event, connection );

	int connfd = _connection->sockfd;

	PyObject_CallFunctionObjArgs( ((pyzimr_website_t*)connection->website)->connection_handler, connection, NULL );

	Py_DECREF( connection );

	if ( PyErr_Occurred() )
		pyzimr_error_print( !zimr_website_connection_sent(
			((pyzimr_website_t*)connection->website)->_website->sockfd, connfd ) ? _connection : NULL );

	PyGILState_Release( gstate );

	if ( _py_want_reload ) {
		_py_want_reload = false;
		zimr_reload_module( "python" );
		if ( !zimr_website_connection_sent( ((pyzimr_website_t*)connection->website)->_website->sockfd, connfd ) )
			pyzimr_website_connection_handler( _connection );
	}
}

static void pyzimr_website_error_handler( connection_t* _connection, int error_code, char* message, size_t len ) {
	PyGILState_STATE gstate = PyGILState_Ensure();

	pyzimr_website_t* website = (pyzimr_website_t*) ( (website_data_t*) _connection->website->udata )->udata;
	pyzimr_connection_t* connection = (pyzimr_connection_t*)_connection->udata;

	if ( website->error_handler ) {
		int connfd = _connection->sockfd;

		PyObject* py_error_code = PyInt_FromLong( error_code );
		PyObject* py_message = PyString_FromStringAndSize( message, len );
		PyObject_CallFunctionObjArgs( website->error_handler, connection, py_error_code, py_message, NULL );
		Py_DECREF( py_message );
		Py_DECREF( py_error_code );

		if ( PyErr_Occurred() )
			pyzimr_error_print( !zimr_website_connection_sent( website->_website->sockfd, connfd ) ? _connection : NULL );
	}

	PyGILState_Release( gstate );
}

static void pyzimr_website_dealloc( pyzimr_website_t* self ) {
	( (website_data_t*) self->_website->udata )->udata = NULL;
	zimr_website_destroy( self->_website );
	if ( self->connection_handler )
		Py_DECREF( self->connection_handler );
	self->ob_type->tp_free( (PyObject*) self );
}

static PyObject* pyzimr_website_new( PyTypeObject* type, PyObject* args, PyObject* kwargs ) {
	pyzimr_website_t* self = (pyzimr_website_t*) type->tp_alloc( type, 0 );
	return (PyObject*) self;
}

static int pyzimr_website_init( pyzimr_website_t* self, PyObject* args, PyObject* kwargs ) {
	char* kwlist[] = { "url", "ip", NULL };
	char* url;
	char* ip = NULL;

	PyArg_ParseTupleAndKeywords( args, kwargs, "s|s", kwlist, &url, &ip );

	if ( !( self->_website = website_get_by_full_url( url, ip ) ) )
		self->_website = zimr_website_create( url );

	( (website_data_t*) self->_website->udata )->udata = self;

	self->connection_handler = PyObject_GetAttrString( m, "defaultConnectionHandler" );
	self->error_handler = NULL;
	zimr_website_set_connection_handler( self->_website, pyzimr_website_connection_handler );
	zimr_website_set_error_handler( self->_website, pyzimr_website_error_handler );

	return 0;
}

static PyObject* pyzimr_website_enable( pyzimr_website_t* self ) {
	zimr_website_enable( self->_website );
	Py_RETURN_NONE;
}

static PyObject* pyzimr_website_disable( pyzimr_website_t* self ) {
	zimr_website_disable( self->_website );
	Py_RETURN_NONE;
}

static PyObject* pyzimr_website_register_page_handler( pyzimr_website_t* self, PyObject* args ) {
	PyObject* page_handler;
	char* page_type;

	if ( ! PyArg_ParseTuple( args, "sO", &page_type, &page_handler ) ) {
		PyErr_SetString( PyExc_TypeError, "page_type and page_handler must be passed" );
		return NULL;
	}

	if ( ! PyCallable_Check( page_handler ) ) {
		PyErr_SetString( PyExc_TypeError, "The page_handler attribute value must be callable" );
		return NULL;
	}

	zimr_website_register_page_handler( self->_website, page_type, (PAGE_HANDLER) &pyzimr_page_handler, (void*) page_handler );

	Py_RETURN_NONE;
}

static PyObject* pyzimr_website_insert_default_page( pyzimr_website_t* self, PyObject* args, PyObject* kwargs ) {
	static char* kwlist[] = { "default_page", "pos", NULL };
	const char* default_page = "";
	int pos = -1;

	if ( !PyArg_ParseTupleAndKeywords( args, kwargs, "s|i", kwlist, &default_page, &pos ) ) {
		PyErr_SetString( PyExc_TypeError, "default_page argument must be passed" );
		return NULL;
	}

	zimr_website_insert_default_page( self->_website, default_page, pos );

	Py_RETURN_NONE;
}

static PyObject* pyzimr_website_get_url( pyzimr_website_t* self, void* closure ) {
	return PyString_FromString( self->_website->url );
}

static PyObject* pyzimr_website_get_protocol( pyzimr_website_t* self, void* closure ) {
	return PyString_FromString( website_protocol( self->_website ) );
}

/*static PyObject* pyzimr_website_get_public_directory( pyzimr_website_t* self, void* closure ) {
	if ( !zimr_website_get_pubdir( self->_website ) ) Py_RETURN_NONE;
	return PyString_FromString( zimr_website_get_pubdir( self->_website ) );
}(*/

static int pypdora_website_set_public_directory( pyzimr_website_t* self, PyObject* value, void* closure ) {
	if ( value == NULL ) {
		PyErr_SetString( PyExc_TypeError, "Cannot delete the public_directory attribute" );
		return -1;
	}

	if ( ! PyString_Check( value ) ) {
		PyErr_SetString( PyExc_TypeError, "The public_directory attribute value must be a string" );
		return -1;
	}

	zimr_website_insert_pubdir( self->_website, PyString_AsString( value ), 0 );

	Py_DECREF( value );
	return 0;
}

static PyObject* pyzimr_website_get_connection_handler( pyzimr_website_t* self, void* closure ) {
	Py_INCREF( self->connection_handler );
	return self->connection_handler;
}

static int pypdora_website_set_connection_handler( pyzimr_website_t* self, PyObject* value, void* closure ) {

	if ( value == NULL )
		value = PyObject_GetAttrString( m, "defaultConnectionHandler" );
	else
		Py_INCREF( value );

	if ( ! PyCallable_Check( value ) ) {
		PyErr_SetString( PyExc_TypeError, "The connection_handler attribute value must be callable" );
		Py_DECREF( value );
		return -1;
	}

	Py_DECREF( self->connection_handler );
	self->connection_handler = value;

	return 0;
}

static PyObject* pyzimr_website_get_error_handler( pyzimr_website_t* self, void* closure ) {
	if ( self->error_handler ) {
		Py_INCREF( self->error_handler );
		return self->error_handler;
	} else {
		Py_INCREF( Py_None );
		return Py_None;
	}
}

static int pyzimr_website_set_error_handler( pyzimr_website_t* self, PyObject* value, void* closure ) {

	if ( !PyCallable_Check( value ) ) {
		PyErr_SetString( PyExc_TypeError, "The error_handler attribute value must be callable" );
		return -1;
	}

	if ( self->error_handler ) Py_DECREF( self->error_handler );

	Py_INCREF( value );
	self->error_handler = value;

	return 0;
}

static PyMethodDef pyzimr_website_methods[] = {
	{ "enable", (PyCFunction) pyzimr_website_enable, METH_NOARGS, "enable the website" },
	{ "disable", (PyCFunction) pyzimr_website_disable, METH_NOARGS, "disable the website" },
	{ "registerPageHandler", (PyCFunction) pyzimr_website_register_page_handler, METH_VARARGS, "Register a page handler." },
	{ "insertDefaultPage", (PyCFunction) pyzimr_website_insert_default_page, METH_VARARGS | METH_KEYWORDS, "add a default page" },
	{ NULL }  /* Sentinel */
};

static PyMemberDef pyzimr_website_members[] = {
//	{ "OPTION_PARSE_MULTIPARTS", T_UINT, offsetof( pyzimr_website_t, OPTION_PARSE_MULTIPARTS ), RO, "allow zimr to parse multipart requests" },
	{ NULL }  /* Sentinel */
};

static PyGetSetDef pyzimr_website_getseters[] = {
	{
	  "url",
	  (getter) pyzimr_website_get_url,
	  0,//(setter) pypdora_response_set_public_directory,
	  "the requested url", NULL
	},
	{
	  "protocol",
	  (getter) pyzimr_website_get_protocol,
	  0,//(setter) pypdora_response_set_public_directory,
	  "the websites protocol", NULL
	},
	{
	  "public_directory",
	  (getter) NULL,// pyzimr_website_get_public_directory,
	  (setter) pypdora_website_set_public_directory,
	  "the websites public directory", NULL
	},
	{
	  "connection_handler",
	  (getter) pyzimr_website_get_connection_handler,
	  (setter) pypdora_website_set_connection_handler,
	  "function to handle web connections", NULL
	},
	{
	  "error_handler",
	  (getter) pyzimr_website_get_error_handler,
	  (setter) pyzimr_website_set_error_handler,
	  "function to handle errors", NULL
	},
	{ NULL }  /* Sentinel */
};

static PyTypeObject pyzimr_website_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"zimr.website",             /*tp_name*/
	sizeof( pyzimr_website_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor) pyzimr_website_dealloc, /*tp_dealloc*/
	0,                         /*tp_print*/
	0,                         /*tp_getattr*/
	0,                         /*tp_setattr*/
	0,                         /*tp_compare*/
	0,                         /*tp_repr*/
	0,                         /*tp_as_number*/
	0,                         /*tp_as_sequence*/
	0,                         /*tp_as_mapping*/
	0,                         /*tp_hash */
	0,                         /*tp_call*/
	0,                         /*tp_str*/
	0,                         /*tp_getattro*/
	0,                         /*tp_setattro*/
	0,                         /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
	"zimr website object",           /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	pyzimr_website_methods,             /* tp_methods */
	pyzimr_website_members,             /* tp_members */
	pyzimr_website_getseters,           /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc) pyzimr_website_init,      /* tp_init */
	0,                         /* tp_alloc */
	pyzimr_website_new,                 /* tp_new */
};

/**********************************************/
/********** END OF pyzimr_website_t *********/

static PyObject* pyzimr_version() {
	return PyString_FromString( zimr_version() );
}

static PyObject* pyzimr_start() {
	Py_BEGIN_ALLOW_THREADS
	zimr_start();
	Py_END_ALLOW_THREADS
	Py_RETURN_NONE;
}

static PyObject* pyzimr_restart() {
	zimr_restart();
	Py_RETURN_NONE;
}

static PyObject* pyzimr_reload() {
	_py_want_reload = true;
	Py_RETURN_NONE;
}

static PyObject* pyzimr_default_connection_handler( PyObject* self, PyObject* args, PyObject* kwargs ) {
	pyzimr_connection_t* connection;

	if ( !PyArg_ParseTuple( args, "O", &connection ) ) {
		PyErr_SetString( PyExc_TypeError, "connection paramater must be passed" );
		return NULL;
	}

	zimr_connection_send_file( connection->_connection, connection->_connection->request.url, true );
	Py_RETURN_NONE;
}

static PyObject* pyzimr_log( PyObject* self, PyObject* args, PyObject* kwargs ) {
	char* message;

	if ( !PyArg_ParseTuple( args, "s", &message ) ) {
		PyErr_SetString( PyExc_TypeError, "message paramater must be passed" );
		return NULL;
	}

	dlog( stderr, "%s", message );
	Py_RETURN_NONE;
}

static void pyzimr_page_handler( connection_t* connection, const char* filepath, void* udata ) {
	PyGILState_STATE gstate = PyGILState_Ensure();
//	PyEval_AcquireLock();
//	PyThreadState_Swap( mainstate );
//	PyEval_AcquireLock();
//	if ( _save ) {
//		Py_BLOCK_THREADS
//	}

	PyObject* page_handler = udata;
	PyObject* result = NULL;
	PyObject* args = NULL,* kwargs = NULL;
	PyObject* connection_obj = connection->udata;
	void* result_ptr;
	Py_ssize_t result_len;

	if ( !connection_obj ) connection_obj = Py_None;

	int websitefd = connection->website->sockfd;
	int connfd    = connection->sockfd;

	if (
		( args = Py_BuildValue( "(s)", filepath ) ) &&
		( kwargs = Py_BuildValue( "{s:O}", "connection", connection_obj ) ) &&
		( result = PyObject_Call( page_handler, args, kwargs ) )
	) {

		if ( PyObject_AsReadBuffer( result, (const void**) &result_ptr, (Py_ssize_t*) &result_len ) ) {
			PyErr_SetString( PyExc_TypeError, "result from page_handler invalid" );
			return;
		}

		if ( !zimr_website_connection_sent( websitefd, connfd ) )
			zimr_connection_send( connection, result_ptr, result_len );

	}

	else {
		if ( PyErr_Occurred() )
			pyzimr_error_print( !zimr_website_connection_sent( websitefd, connfd ) ? connection : NULL );
	}

	Py_XDECREF( result );
	Py_XDECREF( args );
	Py_XDECREF( kwargs );

//	PyEval_ReleaseLock();
	PyGILState_Release( gstate );
//	Py_UNBLOCK_THREADS
}

static PyMethodDef pyzimr_methods[] = {
	{ "version",  (PyCFunction) pyzimr_version, METH_NOARGS, "Returns the version of zimr." },
	{ "start", (PyCFunction) pyzimr_start, METH_NOARGS, "Starts the zimr mainloop." },
	{ "log", (PyCFunction) pyzimr_log, METH_VARARGS, "Write message to zimr.log." },
	{ "defaultConnectionHandler", (PyCFunction) pyzimr_default_connection_handler, METH_VARARGS, "The zimr default connection handler." },
	{ "restart", (PyCFunction) pyzimr_restart, METH_NOARGS, "Trigger a zimr webapp restart." },
	{ "modpythonReload", (PyCFunction) pyzimr_reload, METH_NOARGS, "Reload the zimr python module." },
	{ "modpythonReload", (PyCFunction) pyzimr_reload, METH_NOARGS, "Reload the zimr python module." },
	{ NULL }		/* Sentinel */
};

PyMODINIT_FUNC initczimr( void ) {
	/*PyEval_InitThreads();
	mainstate = PyThreadState_Swap(NULL);
	PyEval_ReleaseLock();

	PyEval_AcquireLock();
	PyThreadState_Swap( mainstate );*/

	if ( PyType_Ready( &pyzimr_website_type ) < 0
	  || PyType_Ready( &pyzimr_connection_type ) < 0
	  || PyType_Ready( &pyzimr_cookies_type ) < 0
	  || PyType_Ready( &pyzimr_request_type ) < 0
	  || PyType_Ready( &pyzimr_params_type ) < 0
	  || PyType_Ready( &pyzimr_response_type ) < 0
	  || PyType_Ready( &pyzimr_headers_type ) < 0
	  || PyType_Ready( &pyzimr_website_options_type ) < 0
	)
		return;

	m = Py_InitModule( "czimr", pyzimr_methods );
	if ( m == NULL )
		return;

	Py_INCREF( &pyzimr_website_type );
	PyModule_AddObject( m, "website", (PyObject*) &pyzimr_website_type );

	Py_INCREF( &pyzimr_connection_type );
	PyModule_AddObject( m, "connection", (PyObject*) &pyzimr_connection_type );

	Py_INCREF( &pyzimr_cookies_type );
	PyModule_AddObject( m, "cookies", (PyObject*) &pyzimr_cookies_type );

	Py_INCREF( &pyzimr_headers_type );
	PyModule_AddObject( m, "headers", (PyObject*) &pyzimr_headers_type );

	Py_INCREF( &pyzimr_request_type );
	PyModule_AddObject( m, "request", (PyObject*) &pyzimr_request_type );

	Py_INCREF( &pyzimr_params_type );
	PyModule_AddObject( m, "params", (PyObject*) &pyzimr_params_type );

	Py_INCREF( &pyzimr_response_type );
	PyModule_AddObject( m, "response", (PyObject*) &pyzimr_response_type );

	Py_INCREF( &pyzimr_website_options_type );
	PyModule_AddObject( m, "website_options", (PyObject*) &pyzimr_website_options_type );

	PyModule_AddObject( m, "default_website_options", (PyObject*)pyzimr_website_options_create( NULL, &website_options_get, &website_options_set ) );

	zimr_init( "" );
	//PyEval_ReleaseLock();

	return;
}
