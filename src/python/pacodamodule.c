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

#include <Python.h>
#include <structmember.h>

#include "pacoda.h"

static PyObject* m;
/********** START OF pypacoda_response_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	response_t* _response;
} pypacoda_response_t;

static PyObject* pypacoda_response_set_header( PyObject* self, PyObject* args ) {
	char* name,* value;

	if ( PyArg_ParseTuple( args, "ss", &name, &value ) ) {
		headers_set_header( &( (pypacoda_response_t*) self )->_response->headers, name, value );
	} else {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject* pypacoda_response_set_status( PyObject* self, PyObject* args ) {
	int status_code;

	if ( PyArg_ParseTuple( args, "i", &status_code ) ) {
		response_set_status( ( (pypacoda_response_t*) self )->_response, status_code );
	} else {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	Py_RETURN_NONE;
}


static PyMethodDef pypacoda_response_methods[ ] = {
	{ "setStatus", (PyCFunction) pypacoda_response_set_status, METH_VARARGS, "Start the website." },
	{ "setHeader", (PyCFunction) pypacoda_response_set_header, METH_VARARGS, "Start the website." },
	{ NULL }  /* Sentinel */
};

static PyTypeObject pypacoda_response_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"pacoda.response",             /*tp_name*/
	sizeof( pypacoda_response_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	0,//(destructor) pypacoda_website_dealloc, /*tp_dealloc*/
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
	"pacoda website objects",           /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	pypacoda_response_methods,             /* tp_methods */
	0,//pypacoda_website_members,             /* tp_members */
	0,//pypacoda_website_getseters,           /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,//(initproc) pypacoda_website_init,      /* tp_init */
	0,                         /* tp_alloc */
	PyType_GenericNew,                 /* tp_new */
};


/********** START OF pypacoda_request_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	request_t* _request;
} pypacoda_request_t;

static PyObject* pypacoda_request_get_param( pypacoda_request_t* self, PyObject* args ) {
	const char* param_name;
	param_t* param;

	if ( !PyArg_ParseTuple( args, "s", &param_name ) ) {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	param = params_get_param( ( (pypacoda_request_t*) self )->_request->params, param_name );

	if ( !param )
		Py_RETURN_NONE;

	return PyString_FromString( param->value );
}

static PyObject* pypacoda_request_get_url( pypacoda_request_t* self, void* closure ) {
	if ( !self->_request->url ) Py_RETURN_NONE;
	return PyString_FromString( self->_request->url );
}

static PyObject* pypacoda_request_get_post_body( pypacoda_request_t* self, void* closure ) {
	if ( !self->_request->post_body ) Py_RETURN_NONE;
	return PyString_FromString( self->_request->post_body );
}

static PyMemberDef pypacoda_request_members[ ] = {
	{ NULL }  /* Sentinel */
};

static PyMethodDef pypacoda_request_methods[ ] = {
	{ "getParam", (PyCFunction) pypacoda_request_get_param, METH_VARARGS, "Start the website." },
	{ NULL }  /* Sentinel */
};

static PyGetSetDef pypacoda_request_getseters[ ] = {
	{
	  "url",
	  (getter) pypacoda_request_get_url,
	  0,//(setter) pypdora_response_set_public_directory,
	  "the websites public directory", NULL
	},
	{
	  "post_body",
	  (getter) pypacoda_request_get_post_body,
	  0,//(setter) pypdora_response_set_public_directory,
	  "the websites public directory", NULL
	},
	{ NULL }  /* Sentinel */
};

static PyTypeObject pypacoda_request_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"pacoda.request",             /*tp_name*/
	sizeof( pypacoda_request_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	0,//(destructor) pypacoda_website_dealloc, /*tp_dealloc*/
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
	"pacoda website objects",           /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	pypacoda_request_methods,             /* tp_methods */
	pypacoda_request_members,             /* tp_members */
	pypacoda_request_getseters,           /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,//(initproc) pypacoda_website_init,      /* tp_init */
	0,                         /* tp_alloc */
	PyType_GenericNew,                 /* tp_new */
};


/********** START OF pypacoda_connection_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	PyObject* client; //TODO: temporary
	PyObject* response;
	PyObject* request;
	PyObject* website;
	connection_t* _connection;
} pypacoda_connection_t;

static void pypacoda_connection_dealloc( pypacoda_connection_t* self ) {
	Py_XDECREF( self->client );
	Py_DECREF( self->response );
	Py_DECREF( self->request );
	Py_DECREF( self->website );
	connection_free( self->_connection );
	self->ob_type->tp_free( (PyObject*) self );
}

static PyObject* pypacoda_connection_send( pypacoda_connection_t* self, PyObject* args ) {
	const char* message;

	if ( PyArg_ParseTuple( args, "s", &message ) ) {
		pacoda_connection_send( self->_connection, (void*) message, strlen( message ) );
	} else {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject* pypacoda_connection_send_file( pypacoda_connection_t* self, PyObject* args ) {
	const char* filename = self->_connection->request.url;
	unsigned char use_pubdir = 1;

	if ( PyArg_ParseTuple( args, "|sb", &filename, &use_pubdir ) ) {
		pacoda_connection_send_file( self->_connection, (char*) filename, use_pubdir );
	} else {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject* pypacoda_connection_get_cookie( pypacoda_connection_t* self, PyObject* args ) {
	const char* cookie_name;
	cookie_t* cookie;

	if ( !PyArg_ParseTuple( args, "s", &cookie_name ) ) {
		PyErr_SetString( PyExc_TypeError, "connection paramater must be passed" );
		return NULL;
	}

	cookie = cookies_get_cookie( &self->_connection->cookies, cookie_name );

	if ( !cookie )
		Py_RETURN_NONE;

	return PyString_FromString( cookie->value );
}

static PyObject* pypacoda_connection_set_cookie( pypacoda_connection_t* self, PyObject* args, PyObject* kwargs ) {
	static char* kwlist[ ] = { "name", "value", "expires", "domain", "path", NULL };
	const char* cookie_name,* cookie_value,* cookie_domain = "",* cookie_path = "";
	time_t expires = 0;

	if ( !PyArg_ParseTupleAndKeywords( args, kwargs, "s|siss", kwlist, &cookie_name, &cookie_value, &expires, &cookie_domain, &cookie_path ) ) {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	cookies_set_cookie( &self->_connection->cookies, cookie_name, cookie_value, expires, cookie_domain, cookie_path );

	Py_RETURN_NONE;
}

static PyObject* pypacoda_connection_get_hostname( pypacoda_connection_t* self, void* closure ) {
	return PyString_FromString( self->_connection->hostname );
}

static PyObject* pypacoda_connection_get_ip( pypacoda_connection_t* self, void* closure ) {
	return PyString_FromString( inet_ntoa( self->_connection->ip ) );
}

static PyMemberDef pypacoda_connection_members[ ] = {
	{ "client", T_OBJECT_EX, offsetof( pypacoda_connection_t, client ), 0, "response object of this request" }, //TODO: temporary
	{ "response", T_OBJECT_EX, offsetof( pypacoda_connection_t, response ), RO, "response object of this request" },
	{ "request", T_OBJECT_EX, offsetof( pypacoda_connection_t, request ), RO, "response object of this request" },
	{ "website", T_OBJECT_EX, offsetof( pypacoda_connection_t, website ), RO, "website object from which the request originated" },
	{ NULL }  /* Sentinel */
};

static PyMethodDef pypacoda_connection_methods[ ] = {
	{ "setCookie", (PyCFunction) pypacoda_connection_set_cookie, METH_VARARGS | METH_KEYWORDS, "Start the website." },
	{ "getCookie", (PyCFunction) pypacoda_connection_get_cookie, METH_VARARGS, "Start the website." },
	{ "send", (PyCFunction) pypacoda_connection_send, METH_VARARGS, "Start the website." },
	{ "sendFile", (PyCFunction) pypacoda_connection_send_file, METH_VARARGS, "Start the website." },
	{ NULL }  /* Sentinel */
};

static PyGetSetDef pypacoda_connection_getseters[ ] = {
	{
	  "hostname",
	  (getter) pypacoda_connection_get_hostname,
	  0,
	  "the websites public directory", NULL
	},
	{
	  "ip",
	  (getter) pypacoda_connection_get_ip,
	  0,
	  "the websites public directory", NULL
	},
	{ NULL }  /* Sentinel */
};

static PyTypeObject pypacoda_connection_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"pacoda.connection",             /*tp_name*/
	sizeof( pypacoda_connection_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor) pypacoda_connection_dealloc, /*tp_dealloc*/
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
	"pacoda website objects",           /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	pypacoda_connection_methods,             /* tp_methods */
	pypacoda_connection_members,             /* tp_members */
	pypacoda_connection_getseters,           /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,//(initproc) pypacoda_website_init,      /* tp_init */
	0,                         /* tp_alloc */
	PyType_GenericNew,                 /* tp_new */
};


/********** START OF pypacoda_website_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	PyObject* connection_handler;
	website_t* _website;
} pypacoda_website_t;

static void pypacoda_website_connection_handler( connection_t* _connection ) {

	pypacoda_website_t* website = (pypacoda_website_t*) ( (website_data_t*) _connection->website->udata )->udata;

	pypacoda_connection_t* connection = (pypacoda_connection_t*) pypacoda_connection_type.tp_new( &pypacoda_connection_type, NULL, NULL );
	connection->_connection = _connection;
	connection->_connection->udata = connection;

	pypacoda_request_t* request = (pypacoda_request_t*) pypacoda_request_type.tp_new( &pypacoda_request_type, NULL, NULL );
	request->_request = &_connection->request;

	pypacoda_response_t* response = (pypacoda_response_t*) pypacoda_response_type.tp_new( &pypacoda_response_type, NULL, NULL );
	response->_response = &_connection->response;

	connection->request = (PyObject*) request;
	connection->response = (PyObject*) response;

	Py_INCREF( website );
	connection->website = (PyObject*) website;

	PyObject_CallFunctionObjArgs( website->connection_handler, connection, NULL );

	Py_DECREF( connection );
	if ( PyErr_Occurred( ) ) {
		PyErr_PrintEx( 0 );
	}

}

static void pypacoda_website_dealloc( pypacoda_website_t* self ) {
	( (website_data_t*) self->_website->udata )->udata = NULL;
	pacoda_website_destroy( self->_website );
	if ( self->connection_handler )
		Py_DECREF( self->connection_handler );
	self->ob_type->tp_free( (PyObject*) self );
}

static PyObject* pypacoda_website_new( PyTypeObject* type, PyObject* args, PyObject* kwargs ) {
	pypacoda_website_t* self = (pypacoda_website_t*) type->tp_alloc( type, 0 );

	return (PyObject*) self;
}

static int pypacoda_website_init( pypacoda_website_t* self, PyObject* args, PyObject* kwargs ) {
	char* kwlist[ ] = { "url", NULL };
	char* url;

	PyArg_ParseTupleAndKeywords( args, kwargs, "s", kwlist, &url );
	self->_website = pacoda_website_create( url );

	( (website_data_t*) self->_website->udata )->udata = self;

	self->connection_handler = PyObject_GetAttrString( m, "defaultConnectionHandler" );
	pacoda_website_set_connection_handler( self->_website, pypacoda_website_connection_handler );

	return 0;
}

static PyObject* pypacoda_website_enable( pypacoda_website_t* self ) {
	pacoda_website_enable( self->_website );
	Py_RETURN_NONE;
}

static PyObject* pypacoda_website_disable( pypacoda_website_t* self ) {
	pacoda_website_disable( self->_website );
	Py_RETURN_NONE;
}

static PyObject* pypacoda_website_insert_default_page( pypacoda_website_t* self, PyObject* args, PyObject* kwargs ) {
	static char* kwlist[ ] = { "default_page", "pos", NULL };
	const char* default_page = "";
	int pos = -1;

	if ( !PyArg_ParseTupleAndKeywords( args, kwargs, "s|i", kwlist, &default_page, &pos ) ) {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	pacoda_website_insert_default_page( self->_website, default_page, pos );

	Py_RETURN_NONE;
}

static PyObject* pypacoda_website_get_public_directory( pypacoda_website_t* self, void* closure ) {
	if ( !pacoda_website_get_pubdir( self->_website ) ) Py_RETURN_NONE;
	return PyString_FromString( pacoda_website_get_pubdir( self->_website ) );
}

static int pypdora_website_set_public_directory( pypacoda_website_t* self, PyObject* value, void* closure ) {
	if ( value == NULL ) {
		PyErr_SetString( PyExc_TypeError, "Cannot delete the public_directory attribute" );
		return -1;
	}

	if ( ! PyString_Check( value ) ) {
		PyErr_SetString( PyExc_TypeError, "The public_directory attribute value must be a string" );
		return -1;
	}

	pacoda_website_set_pubdir( self->_website, PyString_AsString( value ) );

	Py_DECREF( value );
	return 0;
}

static PyObject* pypacoda_website_get_connection_handler( pypacoda_website_t* self, void* closure ) {
	Py_INCREF( self->connection_handler );
	return self->connection_handler;
}

static int pypdora_website_set_connection_handler( pypacoda_website_t* self, PyObject* value, void* closure ) {

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

static PyMethodDef pypacoda_website_methods[ ] = {
	{ "enable", (PyCFunction) pypacoda_website_enable, METH_NOARGS, "Start the website." },
	{ "disable", (PyCFunction) pypacoda_website_disable, METH_NOARGS, "Stop the website." },
	{ "insertDefaultPage", (PyCFunction) pypacoda_website_insert_default_page, METH_VARARGS | METH_KEYWORDS, "Stop the website." },
	{ NULL }  /* Sentinel */
};

static PyMemberDef pypacoda_website_members[ ] = {
	{ NULL }  /* Sentinel */
};

static PyGetSetDef pypacoda_website_getseters[ ] = {
	{
	  "public_directory",
	  (getter) pypacoda_website_get_public_directory,
	  (setter) pypdora_website_set_public_directory,
	  "the websites public directory", NULL
	},
	{
	  "connection_handler",
	  (getter) pypacoda_website_get_connection_handler,
	  (setter) pypdora_website_set_connection_handler,
	  "function to be handle web requests", NULL
	},
	{ NULL }  /* Sentinel */
};

static PyTypeObject pypacoda_website_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"pacoda.website",             /*tp_name*/
	sizeof( pypacoda_website_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor) pypacoda_website_dealloc, /*tp_dealloc*/
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
	"pacoda website objects",           /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	pypacoda_website_methods,             /* tp_methods */
	pypacoda_website_members,             /* tp_members */
	pypacoda_website_getseters,           /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc) pypacoda_website_init,      /* tp_init */
	0,                         /* tp_alloc */
	pypacoda_website_new,                 /* tp_new */
};

/**********************************************/
/********** END OF pypacoda_website_t *********/

static PyObject* pypacoda_version( ) {
	return PyString_FromString( pacoda_version( ) );
}

static PyObject* pypacoda_start( ) {
	pacoda_start( );
	Py_RETURN_NONE;
}

static PyObject* pypacoda_default_connection_handler( PyObject* self, PyObject* args, PyObject* kwargs ) {
	pypacoda_connection_t* connection;

	if ( ! PyArg_ParseTuple( args, "O", &connection ) ) {
		PyErr_SetString( PyExc_TypeError, "connection paramater must be passed" );
		return NULL;
	}

	pacoda_connection_send_file( connection->_connection, connection->_connection->request.url, true );
	Py_RETURN_NONE;
}

static void pypacoda_page_handler( connection_t* connection, const char* filepath, void* udata ) {
	PyObject* page_handler = udata;
	PyObject* result;

	result = PyObject_CallFunction( page_handler, "sO", filepath, connection->udata );

	if ( result != NULL ) {
		if ( ! PyString_Check( result ) ) {
			PyErr_SetString( PyExc_TypeError, "The request_handler attribute value must be callable" );
			return;
		}
		pacoda_connection_send( connection, (void*) PyString_AsString( result ), strlen( PyString_AsString( result ) ) );

		Py_DECREF( result );
	}

}

static PyObject* pypacoda_register_page_handler( PyObject* self, PyObject* args ) {
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

	pacoda_register_page_handler( page_type, (PAGE_HANDLER) &pypacoda_page_handler, (void*) page_handler );

	Py_RETURN_NONE;
}

static PyMethodDef pypacoda_methods[ ] = {
	{ "version",  (PyCFunction) pypacoda_version, METH_NOARGS, "Returns the version of pacoda." },
	{ "start", (PyCFunction) pypacoda_start, METH_NOARGS, "Returns the version of pacoda." },
	{ "defaultConnectionHandler", (PyCFunction) pypacoda_default_connection_handler, METH_VARARGS, "Returns the version of pacoda." },
	{ "registerPageHandler", (PyCFunction) pypacoda_register_page_handler, METH_VARARGS, "" },
	{ NULL }		/* Sentinel */
};

PyMODINIT_FUNC initpacoda ( void ) {

	if ( PyType_Ready( &pypacoda_website_type ) < 0 )
		return;

	if ( PyType_Ready( &pypacoda_connection_type ) < 0 )
		return;

	if ( PyType_Ready( &pypacoda_request_type ) < 0 )
		return;

	if ( PyType_Ready( &pypacoda_response_type ) < 0 )
		return;

	m = Py_InitModule( "pacoda", pypacoda_methods );
	if ( m == NULL )
		return;

	Py_INCREF( &pypacoda_website_type );
	PyModule_AddObject( m, "website", (PyObject*) &pypacoda_website_type );

	Py_INCREF( &pypacoda_connection_type );
	PyModule_AddObject( m, "connection", (PyObject*) &pypacoda_connection_type );

	Py_INCREF( &pypacoda_request_type );
	PyModule_AddObject( m, "request", (PyObject*) &pypacoda_request_type );

	Py_INCREF( &pypacoda_response_type );
	PyModule_AddObject( m, "response", (PyObject*) &pypacoda_response_type );

	Py_AtExit( &pacoda_shutdown );

	pacoda_init( "" );

	return;
}
