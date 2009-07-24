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

#include <Python.h>
#include <structmember.h>

#include "podora.h"

/********** START OF pypodora_response_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	response_t* _response;
} pypodora_response_t;

static PyObject* pypodora_response_set_header( PyObject* self, PyObject* args ) {
	char* name,* value;

	if ( PyArg_ParseTuple( args, "ss", &name, &value ) ) {
		headers_set_header( &( (pypodora_response_t*) self )->_response->headers, name, value );
	} else {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject* pypodora_response_set_status( PyObject* self, PyObject* args ) {
	int status_code;

	if ( PyArg_ParseTuple( args, "i", &status_code ) ) {
		response_set_status( ( (pypodora_response_t*) self )->_response, status_code );
	} else {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	Py_RETURN_NONE;
}


static PyMethodDef pypodora_response_methods[ ] = {
	{ "setStatus", (PyCFunction) pypodora_response_set_status, METH_VARARGS, "Start the website." },
	{ "setHeader", (PyCFunction) pypodora_response_set_header, METH_VARARGS, "Start the website." },
	{ NULL }  /* Sentinel */
};

static PyTypeObject pypodora_response_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"podora.response",             /*tp_name*/
	sizeof( pypodora_response_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	0,//(destructor) pypodora_website_dealloc, /*tp_dealloc*/
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
	"podora website objects",           /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	pypodora_response_methods,             /* tp_methods */
	0,//pypodora_website_members,             /* tp_members */
	0,//pypodora_website_getseters,           /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,//(initproc) pypodora_website_init,      /* tp_init */
	0,                         /* tp_alloc */
	PyType_GenericNew,                 /* tp_new */
};


/********** START OF pypodora_request_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	request_t* _request;
} pypodora_request_t;

static PyObject* pypodora_request_get_param( pypodora_request_t* self, PyObject* args ) {
	const char* param_name;
	param_t* param;

	if ( !PyArg_ParseTuple( args, "s", &param_name ) ) {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	param = params_get_param( &( (pypodora_request_t*) self )->_request->params, param_name );

	if ( !param )
		Py_RETURN_NONE;

	return PyString_FromString( param->value );
}

static PyObject* pypodora_request_get_url( pypodora_request_t* self, void* closure ) {
	if ( !self->_request->url ) Py_RETURN_NONE;
	return PyString_FromString( self->_request->url );
}

static PyObject* pypodora_request_get_post_body( pypodora_request_t* self, void* closure ) {
	if ( !self->_request->post_body ) Py_RETURN_NONE;
	return PyString_FromString( self->_request->post_body );
}

static PyMemberDef pypodora_request_members[ ] = {
	{ NULL }  /* Sentinel */
};

static PyMethodDef pypodora_request_methods[ ] = {
	{ "getParam", (PyCFunction) pypodora_request_get_param, METH_VARARGS, "Start the website." },
	{ NULL }  /* Sentinel */
};

static PyGetSetDef pypodora_request_getseters[ ] = {
	{
	  "url",
	  (getter) pypodora_request_get_url,
	  0,//(setter) pypdora_response_set_public_directory,
	  "the websites public directory", NULL
	},
	{
	  "post_body",
	  (getter) pypodora_request_get_post_body,
	  0,//(setter) pypdora_response_set_public_directory,
	  "the websites public directory", NULL
	},
	{ NULL }  /* Sentinel */
};

static PyTypeObject pypodora_request_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"podora.request",             /*tp_name*/
	sizeof( pypodora_request_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	0,//(destructor) pypodora_website_dealloc, /*tp_dealloc*/
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
	"podora website objects",           /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	pypodora_request_methods,             /* tp_methods */
	pypodora_request_members,             /* tp_members */
	pypodora_request_getseters,           /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,//(initproc) pypodora_website_init,      /* tp_init */
	0,                         /* tp_alloc */
	PyType_GenericNew,                 /* tp_new */
};


/********** START OF pypodora_connection_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	PyObject* client; //TODO: temporary
	PyObject* response;
	PyObject* request;
	PyObject* website;
	connection_t _connection;
} pypodora_connection_t;

static PyObject* pypodora_connection_send( pypodora_connection_t* self, PyObject* args ) {
	const char* message;

	if ( PyArg_ParseTuple( args, "s", &message ) ) {
		podora_connection_send( &self->_connection, (void*) message, strlen( message ) );
	} else {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject* pypodora_connection_send_file( pypodora_connection_t* self, PyObject* args ) {
	const char* filename;
	unsigned char use_pubdir = 1;

	if ( PyArg_ParseTuple( args, "s|b", &filename, &use_pubdir ) ) {
		podora_connection_send_file( &self->_connection, (char*) filename, use_pubdir );
	} else {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject* pypodora_connection_get_cookie( pypodora_connection_t* self, PyObject* args ) {
	const char* cookie_name;
	cookie_t* cookie;

	if ( !PyArg_ParseTuple( args, "s", &cookie_name ) ) {
		PyErr_SetString( PyExc_TypeError, "connection paramater must be passed" );
		return NULL;
	}

	cookie = cookies_get_cookie( &self->_connection.cookies, cookie_name );

	if ( !cookie )
		Py_RETURN_NONE;

	return PyString_FromString( cookie->value );
}

static PyObject* pypodora_connection_set_cookie( pypodora_connection_t* self, PyObject* args ) {
	const char* cookie_name,* cookie_value;

	if ( !PyArg_ParseTuple( args, "ss", &cookie_name, &cookie_value ) ) {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	cookies_set_cookie( &self->_connection.cookies, cookie_name, cookie_value );

	Py_RETURN_NONE;
}

static PyMemberDef pypodora_connection_members[ ] = {
	{ "client", T_OBJECT_EX, offsetof( pypodora_connection_t, client ), 0, "response object of this request" }, //TODO: temporary
	{ "response", T_OBJECT_EX, offsetof( pypodora_connection_t, response ), RO, "response object of this request" },
	{ "request", T_OBJECT_EX, offsetof( pypodora_connection_t, request ), RO, "response object of this request" },
	{ "website", T_OBJECT_EX, offsetof( pypodora_connection_t, website ), RO, "website object from which the request originated" },
	{ NULL }  /* Sentinel */
};

static PyMethodDef pypodora_connection_methods[ ] = {
	{ "setCookie", (PyCFunction) pypodora_connection_set_cookie, METH_VARARGS, "Start the website." },
	{ "getCookie", (PyCFunction) pypodora_connection_get_cookie, METH_VARARGS, "Start the website." },
	{ "send", (PyCFunction) pypodora_connection_send, METH_VARARGS, "Start the website." },
	{ "sendFile", (PyCFunction) pypodora_connection_send_file, METH_VARARGS, "Start the website." },
	{ NULL }  /* Sentinel */
};

static PyGetSetDef pypodora_connection_getseters[ ] = {
	{ NULL }  /* Sentinel */
};

static PyTypeObject pypodora_connection_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"podora.connection",             /*tp_name*/
	sizeof( pypodora_connection_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	0,//(destructor) pypodora_website_dealloc, /*tp_dealloc*/
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
	"podora website objects",           /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	pypodora_connection_methods,             /* tp_methods */
	pypodora_connection_members,             /* tp_members */
	pypodora_connection_getseters,           /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,//(initproc) pypodora_website_init,      /* tp_init */
	0,                         /* tp_alloc */
	PyType_GenericNew,                 /* tp_new */
};


/********** START OF pypodora_website_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	PyObject* connection_handler;
	struct website* _website;
} pypodora_website_t;

static void pypodora_website_dealloc( pypodora_website_t* self ) {
	podora_website_destroy( self->_website );
	if ( self->connection_handler )
		Py_DECREF( self->connection_handler );
	self->ob_type->tp_free( (PyObject*) self );
}

void pypodora_website_connection_handler( connection_t _connection ) {

	pypodora_website_t* website = (pypodora_website_t*) ( (website_data_t*) _connection.website->data )->udata;

	pypodora_connection_t* connection = (pypodora_connection_t*) pypodora_connection_type.tp_new( &pypodora_connection_type, NULL, NULL );
	connection->_connection = _connection;

	pypodora_request_t* request = (pypodora_request_t*) pypodora_request_type.tp_new( &pypodora_request_type, NULL, NULL );
	request->_request = &_connection.request;

	pypodora_response_t* response = (pypodora_response_t*) pypodora_response_type.tp_new( &pypodora_response_type, NULL, NULL );
	response->_response = &_connection.response;

	connection->request = (PyObject*) request;
	connection->response = (PyObject*) response;

	Py_INCREF( website );
	connection->website = (PyObject*) website;

	PyObject_CallFunctionObjArgs( website->connection_handler, connection, NULL );

	if ( PyErr_Occurred( ) ) {
		PyErr_PrintEx( 0 );
	}

}

static PyObject* pypodora_website_new( PyTypeObject* type, PyObject* args, PyObject* kwargs ) {
	pypodora_website_t* self = (pypodora_website_t*) type->tp_alloc( type, 0 );

	Py_INCREF( Py_None );
	self->connection_handler = Py_None;

	return (PyObject*) self;
}

static int pypodora_website_init( pypodora_website_t* self, PyObject* args, PyObject* kwargs ) {
	char* kwlist[ ] = { "url", NULL };
	char* url;

	PyArg_ParseTupleAndKeywords( args, kwargs, "s", kwlist, &url );
	self->_website = podora_website_create( url );

	( (website_data_t*) self->_website->data )->udata = self;

	return 0;
}

static PyObject* pypodora_website_enable( pypodora_website_t* self ) {
	podora_website_enable( self->_website );
	Py_RETURN_NONE;
}

static PyObject* pypodora_website_disable( pypodora_website_t* self ) {
	podora_website_disable( self->_website );
	Py_RETURN_NONE;
}

static PyObject* pypodora_website_get_public_directory( pypodora_website_t* self, void* closure ) {
	if ( !podora_website_get_pubdir( self->_website ) ) Py_RETURN_NONE;
	return PyString_FromString( podora_website_get_pubdir( self->_website ) );
}

static int pypdora_website_set_public_directory( pypodora_website_t* self, PyObject* value, void* closure ) {
	if ( value == NULL ) {
		PyErr_SetString( PyExc_TypeError, "Cannot delete the public_directory attribute" );
		return -1;
	}

	if ( ! PyString_Check( value ) ) {
		PyErr_SetString( PyExc_TypeError, "The public_directory attribute value must be a string" );
		return -1;
	}

	podora_website_set_pubdir( self->_website, PyString_AsString( value ) );

	return 0;
}

static PyObject* pypodora_website_get_connection_handler( pypodora_website_t* self, void* closure ) {
	Py_INCREF( self->connection_handler );
	return self->connection_handler;
}

static int pypdora_website_set_connection_handler( pypodora_website_t* self, PyObject* value, void* closure ) {

	if ( value == NULL ) {
		Py_DECREF( self->connection_handler );
		Py_INCREF( Py_None );
		self->connection_handler = Py_None;
		podora_website_unset_connection_handler( self->_website );
		return 0;
	}

	if ( ! PyCallable_Check( value ) ) {
		PyErr_SetString( PyExc_TypeError, "The connection_handler attribute value must be callable" );
		return -1;
	}

	Py_DECREF( self->connection_handler );
	Py_INCREF( value );
	self->connection_handler = value;

	podora_website_set_connection_handler( self->_website, pypodora_website_connection_handler );

	return 0;
}


static PyMethodDef pypodora_website_methods[ ] = {
	{ "enable", (PyCFunction) pypodora_website_enable, METH_NOARGS, "Start the website." },
	{ "disable", (PyCFunction) pypodora_website_disable, METH_NOARGS, "Stop the website." },
	{ NULL }  /* Sentinel */
};

static PyMemberDef pypodora_website_members[ ] = {
	{ NULL }  /* Sentinel */
};

static PyGetSetDef pypodora_website_getseters[ ] = {
	{
	  "public_directory",
	  (getter) pypodora_website_get_public_directory,
	  (setter) pypdora_website_set_public_directory,
	  "the websites public directory", NULL
	},
	{
	  "connection_handler",
	  (getter) pypodora_website_get_connection_handler,
	  (setter) pypdora_website_set_connection_handler,
	  "function to be handle web requests", NULL
	},
	{ NULL }  /* Sentinel */
};

static PyTypeObject pypodora_website_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"podora.website",             /*tp_name*/
	sizeof( pypodora_website_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor) pypodora_website_dealloc, /*tp_dealloc*/
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
	"podora website objects",           /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	pypodora_website_methods,             /* tp_methods */
	pypodora_website_members,             /* tp_members */
	pypodora_website_getseters,           /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc) pypodora_website_init,      /* tp_init */
	0,                         /* tp_alloc */
	pypodora_website_new,                 /* tp_new */
};

/**********************************************/
/********** END OF pypodora_website_t *********/

static PyObject* pypodora_version( ) {
	return PyString_FromString( podora_version( ) );
}

static PyObject* pypodora_start( ) {
	podora_start( );
	Py_RETURN_NONE;
}

static PyObject* pypodora_default_connection_handler( PyObject* self, PyObject* args, PyObject* kwargs ) {
	pypodora_connection_t* connection;

	if ( ! PyArg_ParseTuple( args, "O", &connection ) ) {
		PyErr_SetString( PyExc_TypeError, "connection paramater must be passed" );
		return NULL;
	}

	podora_website_default_connection_handler( connection->_connection );
	Py_RETURN_NONE;
}

static void pypodora_page_handler( connection_t* connection, const char* filepath, void* udata ) {
	PyObject* page_handler = udata;
	PyObject* result;

	result = PyObject_CallFunctionObjArgs( page_handler, PyString_FromString( filepath ), NULL );

	if ( result != NULL ) {
		if ( ! PyString_Check( result ) ) {
			PyErr_SetString( PyExc_TypeError, "The request_handler attribute value must be callable" );
			return;
		}

		podora_connection_send( connection, (void*) PyString_AsString( result ), strlen( PyString_AsString( result ) ) );

		Py_DECREF( result );
	}

}

static PyObject* pypodora_register_page_handler( PyObject* self, PyObject* args ) {
	PyObject* page_handler;
	char* page_type;

	if ( ! PyArg_ParseTuple( args, "sO", &page_type, &page_handler ) ) {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	if ( ! PyCallable_Check( page_handler ) ) {
		PyErr_SetString( PyExc_TypeError, "The request_handler attribute value must be callable" );
		return NULL;
	}

	Py_INCREF( page_handler );
	podora_register_page_handler( page_type, (PAGE_HANDLER) &pypodora_page_handler, (void*) page_handler );

	Py_RETURN_NONE;
}

static PyMethodDef pypodora_methods[ ] = {
	{ "version",  (PyCFunction) pypodora_version, METH_NOARGS, "Returns the version of podora." },
	{ "start", (PyCFunction) pypodora_start, METH_NOARGS, "Returns the version of podora." },
	{ "defaultConnectionHandler", (PyCFunction) pypodora_default_connection_handler, METH_VARARGS, "Returns the version of podora." },
	{ "registerPageHandler", (PyCFunction) pypodora_register_page_handler, METH_VARARGS, "" },
	{ NULL }		/* Sentinel */
};

PyMODINIT_FUNC initpodora ( void ) {
	PyObject* m;

	if ( PyType_Ready( &pypodora_website_type ) < 0 )
		return;

	if ( PyType_Ready( &pypodora_connection_type ) < 0 )
		return;

	if ( PyType_Ready( &pypodora_request_type ) < 0 )
		return;

	if ( PyType_Ready( &pypodora_response_type ) < 0 )
		return;

	m = Py_InitModule( "podora", pypodora_methods );
	if ( m == NULL )
		return;

	Py_INCREF( &pypodora_website_type );
	PyModule_AddObject( m, "website", (PyObject*) &pypodora_website_type );

	Py_INCREF( &pypodora_connection_type );
	PyModule_AddObject( m, "connection", (PyObject*) &pypodora_connection_type );

	Py_INCREF( &pypodora_request_type );
	PyModule_AddObject( m, "request", (PyObject*) &pypodora_request_type );

	Py_INCREF( &pypodora_response_type );
	PyModule_AddObject( m, "response", (PyObject*) &pypodora_response_type );

	Py_AtExit( &podora_shutdown );

	if ( !podora_init( ) ) {
		fprintf( stderr, "[fatal] initpodora: podora_init: podora failed to initialize\n" );
	}

	return;
}
