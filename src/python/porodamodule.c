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

#include <Python.h>
#include <structmember.h>

#include "poroda.h"

static PyObject* m;
/********** START OF pyporoda_response_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	response_t* _response;
} pyporoda_response_t;

static PyObject* pyporoda_response_set_header( PyObject* self, PyObject* args ) {
	char* name,* value;

	if ( PyArg_ParseTuple( args, "ss", &name, &value ) ) {
		headers_set_header( &( (pyporoda_response_t*) self )->_response->headers, name, value );
	} else {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject* pyporoda_response_set_status( PyObject* self, PyObject* args ) {
	int status_code;

	if ( PyArg_ParseTuple( args, "i", &status_code ) ) {
		response_set_status( ( (pyporoda_response_t*) self )->_response, status_code );
	} else {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	Py_RETURN_NONE;
}


static PyMethodDef pyporoda_response_methods[ ] = {
	{ "setStatus", (PyCFunction) pyporoda_response_set_status, METH_VARARGS, "Start the website." },
	{ "setHeader", (PyCFunction) pyporoda_response_set_header, METH_VARARGS, "Start the website." },
	{ NULL }  /* Sentinel */
};

static PyTypeObject pyporoda_response_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"poroda.response",             /*tp_name*/
	sizeof( pyporoda_response_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	0,//(destructor) pyporoda_website_dealloc, /*tp_dealloc*/
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
	"poroda website objects",           /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	pyporoda_response_methods,             /* tp_methods */
	0,//pyporoda_website_members,             /* tp_members */
	0,//pyporoda_website_getseters,           /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,//(initproc) pyporoda_website_init,      /* tp_init */
	0,                         /* tp_alloc */
	PyType_GenericNew,                 /* tp_new */
};


/********** START OF pyporoda_request_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	request_t* _request;
} pyporoda_request_t;

static PyObject* pyporoda_request_get_param( pyporoda_request_t* self, PyObject* args ) {
	const char* param_name;
	param_t* param;

	if ( !PyArg_ParseTuple( args, "s", &param_name ) ) {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	param = params_get_param( ( (pyporoda_request_t*) self )->_request->params, param_name );

	if ( !param )
		Py_RETURN_NONE;

	return PyString_FromString( param->value );
}

static PyObject* pyporoda_request_get_url( pyporoda_request_t* self, void* closure ) {
	if ( !self->_request->url ) Py_RETURN_NONE;
	return PyString_FromString( self->_request->url );
}

static PyObject* pyporoda_request_get_post_body( pyporoda_request_t* self, void* closure ) {
	if ( !self->_request->post_body ) Py_RETURN_NONE;
	return PyString_FromString( self->_request->post_body );
}

static PyMemberDef pyporoda_request_members[ ] = {
	{ NULL }  /* Sentinel */
};

static PyMethodDef pyporoda_request_methods[ ] = {
	{ "getParam", (PyCFunction) pyporoda_request_get_param, METH_VARARGS, "Start the website." },
	{ NULL }  /* Sentinel */
};

static PyGetSetDef pyporoda_request_getseters[ ] = {
	{
	  "url",
	  (getter) pyporoda_request_get_url,
	  0,//(setter) pypdora_response_set_public_directory,
	  "the websites public directory", NULL
	},
	{
	  "post_body",
	  (getter) pyporoda_request_get_post_body,
	  0,//(setter) pypdora_response_set_public_directory,
	  "the websites public directory", NULL
	},
	{ NULL }  /* Sentinel */
};

static PyTypeObject pyporoda_request_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"poroda.request",             /*tp_name*/
	sizeof( pyporoda_request_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	0,//(destructor) pyporoda_website_dealloc, /*tp_dealloc*/
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
	"poroda website objects",           /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	pyporoda_request_methods,             /* tp_methods */
	pyporoda_request_members,             /* tp_members */
	pyporoda_request_getseters,           /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,//(initproc) pyporoda_website_init,      /* tp_init */
	0,                         /* tp_alloc */
	PyType_GenericNew,                 /* tp_new */
};


/********** START OF pyporoda_connection_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	PyObject* client; //TODO: temporary
	PyObject* response;
	PyObject* request;
	PyObject* website;
	connection_t* _connection;
} pyporoda_connection_t;

static void pyporoda_connection_dealloc( pyporoda_connection_t* self ) {
	Py_XDECREF( self->client );
	Py_DECREF( self->response );
	Py_DECREF( self->request );
	Py_DECREF( self->website );
	connection_free( self->_connection );
	self->ob_type->tp_free( (PyObject*) self );
}

static PyObject* pyporoda_connection_send( pyporoda_connection_t* self, PyObject* args ) {
	const char* message;

	if ( PyArg_ParseTuple( args, "s", &message ) ) {
		poroda_connection_send( self->_connection, (void*) message, strlen( message ) );
	} else {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject* pyporoda_connection_send_file( pyporoda_connection_t* self, PyObject* args ) {
	const char* filename = self->_connection->request.url;
	unsigned char use_pubdir = 1;

	if ( PyArg_ParseTuple( args, "|sb", &filename, &use_pubdir ) ) {
		poroda_connection_send_file( self->_connection, (char*) filename, use_pubdir );
	} else {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject* pyporoda_connection_get_cookie( pyporoda_connection_t* self, PyObject* args ) {
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

static PyObject* pyporoda_connection_set_cookie( pyporoda_connection_t* self, PyObject* args, PyObject* kwargs ) {
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

static PyObject* pyporoda_connection_get_hostname( pyporoda_connection_t* self, void* closure ) {
	return PyString_FromString( self->_connection->hostname );
}

static PyObject* pyporoda_connection_get_ip( pyporoda_connection_t* self, void* closure ) {
	return PyString_FromString( inet_ntoa( self->_connection->ip ) );
}

static PyMemberDef pyporoda_connection_members[ ] = {
	{ "client", T_OBJECT_EX, offsetof( pyporoda_connection_t, client ), 0, "response object of this request" }, //TODO: temporary
	{ "response", T_OBJECT_EX, offsetof( pyporoda_connection_t, response ), RO, "response object of this request" },
	{ "request", T_OBJECT_EX, offsetof( pyporoda_connection_t, request ), RO, "response object of this request" },
	{ "website", T_OBJECT_EX, offsetof( pyporoda_connection_t, website ), RO, "website object from which the request originated" },
	{ NULL }  /* Sentinel */
};

static PyMethodDef pyporoda_connection_methods[ ] = {
	{ "setCookie", (PyCFunction) pyporoda_connection_set_cookie, METH_VARARGS | METH_KEYWORDS, "Start the website." },
	{ "getCookie", (PyCFunction) pyporoda_connection_get_cookie, METH_VARARGS, "Start the website." },
	{ "send", (PyCFunction) pyporoda_connection_send, METH_VARARGS, "Start the website." },
	{ "sendFile", (PyCFunction) pyporoda_connection_send_file, METH_VARARGS, "Start the website." },
	{ NULL }  /* Sentinel */
};

static PyGetSetDef pyporoda_connection_getseters[ ] = {
	{
	  "hostname",
	  (getter) pyporoda_connection_get_hostname,
	  0,
	  "the websites public directory", NULL
	},
	{
	  "ip",
	  (getter) pyporoda_connection_get_ip,
	  0,
	  "the websites public directory", NULL
	},
	{ NULL }  /* Sentinel */
};

static PyTypeObject pyporoda_connection_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"poroda.connection",             /*tp_name*/
	sizeof( pyporoda_connection_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor) pyporoda_connection_dealloc, /*tp_dealloc*/
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
	"poroda website objects",           /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	pyporoda_connection_methods,             /* tp_methods */
	pyporoda_connection_members,             /* tp_members */
	pyporoda_connection_getseters,           /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,//(initproc) pyporoda_website_init,      /* tp_init */
	0,                         /* tp_alloc */
	PyType_GenericNew,                 /* tp_new */
};


/********** START OF pyporoda_website_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	PyObject* connection_handler;
	website_t* _website;
} pyporoda_website_t;

static void pyporoda_website_connection_handler( connection_t* _connection ) {

	pyporoda_website_t* website = (pyporoda_website_t*) ( (website_data_t*) _connection->website->udata )->udata;

	pyporoda_connection_t* connection = (pyporoda_connection_t*) pyporoda_connection_type.tp_new( &pyporoda_connection_type, NULL, NULL );
	connection->_connection = _connection;
	connection->_connection->udata = connection;

	pyporoda_request_t* request = (pyporoda_request_t*) pyporoda_request_type.tp_new( &pyporoda_request_type, NULL, NULL );
	request->_request = &_connection->request;

	pyporoda_response_t* response = (pyporoda_response_t*) pyporoda_response_type.tp_new( &pyporoda_response_type, NULL, NULL );
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

static void pyporoda_website_dealloc( pyporoda_website_t* self ) {
	( (website_data_t*) self->_website->udata )->udata = NULL;
	poroda_website_destroy( self->_website );
	if ( self->connection_handler )
		Py_DECREF( self->connection_handler );
	self->ob_type->tp_free( (PyObject*) self );
}

static PyObject* pyporoda_website_new( PyTypeObject* type, PyObject* args, PyObject* kwargs ) {
	pyporoda_website_t* self = (pyporoda_website_t*) type->tp_alloc( type, 0 );

	return (PyObject*) self;
}

static int pyporoda_website_init( pyporoda_website_t* self, PyObject* args, PyObject* kwargs ) {
	char* kwlist[ ] = { "url", NULL };
	char* url;

	PyArg_ParseTupleAndKeywords( args, kwargs, "s", kwlist, &url );
	self->_website = poroda_website_create( url );

	( (website_data_t*) self->_website->udata )->udata = self;

	self->connection_handler = PyObject_GetAttrString( m, "defaultConnectionHandler" );
	poroda_website_set_connection_handler( self->_website, pyporoda_website_connection_handler );

	return 0;
}

static PyObject* pyporoda_website_enable( pyporoda_website_t* self ) {
	poroda_website_enable( self->_website );
	Py_RETURN_NONE;
}

static PyObject* pyporoda_website_disable( pyporoda_website_t* self ) {
	poroda_website_disable( self->_website );
	Py_RETURN_NONE;
}

static PyObject* pyporoda_website_insert_default_page( pyporoda_website_t* self, PyObject* args, PyObject* kwargs ) {
	static char* kwlist[ ] = { "default_page", "pos", NULL };
	const char* default_page = "";
	int pos = -1;

	if ( !PyArg_ParseTupleAndKeywords( args, kwargs, "s|i", kwlist, &default_page, &pos ) ) {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	poroda_website_insert_default_page( self->_website, default_page, pos );

	Py_RETURN_NONE;
}

static PyObject* pyporoda_website_get_public_directory( pyporoda_website_t* self, void* closure ) {
	if ( !poroda_website_get_pubdir( self->_website ) ) Py_RETURN_NONE;
	return PyString_FromString( poroda_website_get_pubdir( self->_website ) );
}

static int pypdora_website_set_public_directory( pyporoda_website_t* self, PyObject* value, void* closure ) {
	if ( value == NULL ) {
		PyErr_SetString( PyExc_TypeError, "Cannot delete the public_directory attribute" );
		return -1;
	}

	if ( ! PyString_Check( value ) ) {
		PyErr_SetString( PyExc_TypeError, "The public_directory attribute value must be a string" );
		return -1;
	}

	poroda_website_set_pubdir( self->_website, PyString_AsString( value ) );

	Py_DECREF( value );
	return 0;
}

static PyObject* pyporoda_website_get_connection_handler( pyporoda_website_t* self, void* closure ) {
	Py_INCREF( self->connection_handler );
	return self->connection_handler;
}

static int pypdora_website_set_connection_handler( pyporoda_website_t* self, PyObject* value, void* closure ) {

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

static PyMethodDef pyporoda_website_methods[ ] = {
	{ "enable", (PyCFunction) pyporoda_website_enable, METH_NOARGS, "Start the website." },
	{ "disable", (PyCFunction) pyporoda_website_disable, METH_NOARGS, "Stop the website." },
	{ "insertDefaultPage", (PyCFunction) pyporoda_website_insert_default_page, METH_VARARGS | METH_KEYWORDS, "Stop the website." },
	{ NULL }  /* Sentinel */
};

static PyMemberDef pyporoda_website_members[ ] = {
	{ NULL }  /* Sentinel */
};

static PyGetSetDef pyporoda_website_getseters[ ] = {
	{
	  "public_directory",
	  (getter) pyporoda_website_get_public_directory,
	  (setter) pypdora_website_set_public_directory,
	  "the websites public directory", NULL
	},
	{
	  "connection_handler",
	  (getter) pyporoda_website_get_connection_handler,
	  (setter) pypdora_website_set_connection_handler,
	  "function to be handle web requests", NULL
	},
	{ NULL }  /* Sentinel */
};

static PyTypeObject pyporoda_website_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"poroda.website",             /*tp_name*/
	sizeof( pyporoda_website_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor) pyporoda_website_dealloc, /*tp_dealloc*/
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
	"poroda website objects",           /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	pyporoda_website_methods,             /* tp_methods */
	pyporoda_website_members,             /* tp_members */
	pyporoda_website_getseters,           /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc) pyporoda_website_init,      /* tp_init */
	0,                         /* tp_alloc */
	pyporoda_website_new,                 /* tp_new */
};

/**********************************************/
/********** END OF pyporoda_website_t *********/

static PyObject* pyporoda_version( ) {
	return PyString_FromString( poroda_version( ) );
}

static PyObject* pyporoda_start( ) {
	poroda_start( );
	Py_RETURN_NONE;
}

static PyObject* pyporoda_default_connection_handler( PyObject* self, PyObject* args, PyObject* kwargs ) {
	pyporoda_connection_t* connection;

	if ( ! PyArg_ParseTuple( args, "O", &connection ) ) {
		PyErr_SetString( PyExc_TypeError, "connection paramater must be passed" );
		return NULL;
	}

	poroda_connection_send_file( connection->_connection, connection->_connection->request.url, true );
	Py_RETURN_NONE;
}

static void pyporoda_page_handler( connection_t* connection, const char* filepath, void* udata ) {
	PyObject* page_handler = udata;
	PyObject* result;

	result = PyObject_CallFunction( page_handler, "sO", filepath, connection->udata );

	if ( result != NULL ) {
		if ( ! PyString_Check( result ) ) {
			PyErr_SetString( PyExc_TypeError, "The request_handler attribute value must be callable" );
			return;
		}
		poroda_connection_send( connection, (void*) PyString_AsString( result ), strlen( PyString_AsString( result ) ) );

		Py_DECREF( result );
	}

}

static PyObject* pyporoda_register_page_handler( PyObject* self, PyObject* args ) {
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

	poroda_register_page_handler( page_type, (PAGE_HANDLER) &pyporoda_page_handler, (void*) page_handler );

	Py_RETURN_NONE;
}

static PyMethodDef pyporoda_methods[ ] = {
	{ "version",  (PyCFunction) pyporoda_version, METH_NOARGS, "Returns the version of poroda." },
	{ "start", (PyCFunction) pyporoda_start, METH_NOARGS, "Returns the version of poroda." },
	{ "defaultConnectionHandler", (PyCFunction) pyporoda_default_connection_handler, METH_VARARGS, "Returns the version of poroda." },
	{ "registerPageHandler", (PyCFunction) pyporoda_register_page_handler, METH_VARARGS, "" },
	{ NULL }		/* Sentinel */
};

PyMODINIT_FUNC initporoda ( void ) {

	if ( PyType_Ready( &pyporoda_website_type ) < 0 )
		return;

	if ( PyType_Ready( &pyporoda_connection_type ) < 0 )
		return;

	if ( PyType_Ready( &pyporoda_request_type ) < 0 )
		return;

	if ( PyType_Ready( &pyporoda_response_type ) < 0 )
		return;

	m = Py_InitModule( "poroda", pyporoda_methods );
	if ( m == NULL )
		return;

	Py_INCREF( &pyporoda_website_type );
	PyModule_AddObject( m, "website", (PyObject*) &pyporoda_website_type );

	Py_INCREF( &pyporoda_connection_type );
	PyModule_AddObject( m, "connection", (PyObject*) &pyporoda_connection_type );

	Py_INCREF( &pyporoda_request_type );
	PyModule_AddObject( m, "request", (PyObject*) &pyporoda_request_type );

	Py_INCREF( &pyporoda_response_type );
	PyModule_AddObject( m, "response", (PyObject*) &pyporoda_response_type );

	Py_AtExit( &poroda_shutdown );

	poroda_init( "" );

	return;
}
