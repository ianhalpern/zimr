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

static PyObject* m;
/********** START OF pyzimr_response_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	response_t* _response;
} pyzimr_response_t;

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

static PyMethodDef pyzimr_response_methods[ ] = {
	{ "setStatus", (PyCFunction) pyzimr_response_set_status, METH_VARARGS, "set the response status" },
	{ "setHeader", (PyCFunction) pyzimr_response_set_header, METH_VARARGS, "set a response header" },
	{ NULL }  /* Sentinel */
};

static PyTypeObject pyzimr_response_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"zimr.response",             /*tp_name*/
	sizeof( pyzimr_response_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	0,//(destructor) pyzimr_website_dealloc, /*tp_dealloc*/
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
	0,//pyzimr_website_members,             /* tp_members */
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


/********** START OF pyzimr_request_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	request_t* _request;
} pyzimr_request_t;

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
	return PyString_FromString( self->_request->post_body );
}

static PyMemberDef pyzimr_request_members[ ] = {
	{ NULL }  /* Sentinel */
};

static PyMethodDef pyzimr_request_methods[ ] = {
	{ "getParam", (PyCFunction) pyzimr_request_get_param, METH_VARARGS, "get a GET/POST query string parameter value" },
	{ "getHeader", (PyCFunction) pyzimr_request_get_header, METH_VARARGS, "get a request header value" },
	{ NULL }  /* Sentinel */
};

static PyGetSetDef pyzimr_request_getseters[ ] = {
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
	{ NULL }  /* Sentinel */
};

static PyTypeObject pyzimr_request_type = {
	PyObject_HEAD_INIT( NULL )
	0,                         /*ob_size*/
	"zimr.request",             /*tp_name*/
	sizeof( pyzimr_request_t ),             /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	0,//(destructor) pyzimr_website_dealloc, /*tp_dealloc*/
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


/********** START OF pyzimr_connection_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	PyObject* client; //TODO: temporary
	PyObject* response;
	PyObject* request;
	PyObject* website;
	connection_t* _connection;
} pyzimr_connection_t;

static void pyzimr_connection_dealloc( pyzimr_connection_t* self ) {
	Py_XDECREF( self->client );
	Py_DECREF( self->response );
	Py_DECREF( self->request );
	Py_DECREF( self->website );
	//connection_free( self->_connection );
	self->ob_type->tp_free( (PyObject*) self );
}

static PyObject* pyzimr_connection_send( pyzimr_connection_t* self, PyObject* args ) {
	const char* message;

	if ( !PyArg_ParseTuple( args, "s", &message ) ) {
		PyErr_SetString( PyExc_TypeError, "message paramater must be passed" );
		return NULL;
	}
	zimr_connection_send( self->_connection, (void*) message, strlen( message ) );

	Py_RETURN_NONE;
}

static PyObject* pyzimr_connection_send_file( pyzimr_connection_t* self, PyObject* args ) {
	const char* filename = self->_connection->request.url;
	unsigned char use_pubdir = 1;

	if ( !PyArg_ParseTuple( args, "|sb", &filename, &use_pubdir ) ) {
		PyErr_SetString( PyExc_TypeError, "the filename paramater must be passed" );
		return NULL;
	}
	zimr_connection_send_file( self->_connection, (char*) filename, use_pubdir );

	Py_RETURN_NONE;
}

static PyObject* pyzimr_connection_redirect( pyzimr_connection_t* self, PyObject* args ) {
	const char* redir_url = NULL;

	if ( !PyArg_ParseTuple( args, "s", &redir_url ) ) {
		PyErr_SetString( PyExc_TypeError, "the redir_url paramater must be passed" );
		return NULL;
	}
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
	static char* kwlist[ ] = { "name", "value", "expires", "domain", "path", NULL };
	const char* cookie_name,* cookie_value,* cookie_domain = "",* cookie_path = "";
	time_t expires = 0;

	if ( !PyArg_ParseTupleAndKeywords( args, kwargs, "s|siss", kwlist, &cookie_name, &cookie_value, &expires, &cookie_domain, &cookie_path ) ) {
		PyErr_SetString( PyExc_TypeError, "the cookie_name must be passes" );
		return NULL;
	}

	cookies_set_cookie( &self->_connection->cookies, cookie_name, cookie_value, expires, cookie_domain, cookie_path );

	Py_RETURN_NONE;
}

static PyObject* pyzimr_connection_get_hostname( pyzimr_connection_t* self, void* closure ) {
	return PyString_FromString( self->_connection->hostname );
}

static PyObject* pyzimr_connection_get_ip( pyzimr_connection_t* self, void* closure ) {
	return PyString_FromString( inet_ntoa( self->_connection->ip ) );
}

static PyMemberDef pyzimr_connection_members[ ] = {
	{ "client", T_OBJECT_EX, offsetof( pyzimr_connection_t, client ), 0, "temporary object" }, //TODO: temporary
	{ "response", T_OBJECT_EX, offsetof( pyzimr_connection_t, response ), RO, "response object of this connection" },
	{ "request", T_OBJECT_EX, offsetof( pyzimr_connection_t, request ), RO, "request object of this connection" },
	{ "website", T_OBJECT_EX, offsetof( pyzimr_connection_t, website ), RO, "website object from which the request originated" },
	{ NULL }  /* Sentinel */
};

static PyMethodDef pyzimr_connection_methods[ ] = {
	{ "setCookie", (PyCFunction) pyzimr_connection_set_cookie, METH_VARARGS | METH_KEYWORDS, "set a cookie" },
	{ "getCookie", (PyCFunction) pyzimr_connection_get_cookie, METH_VARARGS, "get a value of a cookie" },
	{ "send", (PyCFunction) pyzimr_connection_send, METH_VARARGS, "send a string as the response to the connection" },
	{ "sendFile", (PyCFunction) pyzimr_connection_send_file, METH_VARARGS, "send a file as a response to the connection" },
	{ "redirect", (PyCFunction) pyzimr_connection_redirect, METH_VARARGS, "redirect the connection to a different url" },
	{ NULL }  /* Sentinel */
};

static PyGetSetDef pyzimr_connection_getseters[ ] = {
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

typedef struct {
	PyObject_HEAD
	PyObject* connection_handler;
	website_t* _website;
} pyzimr_website_t;

static void pyzimr_website_connection_handler( connection_t* _connection ) {

	pyzimr_website_t* website = (pyzimr_website_t*) ( (website_data_t*) _connection->website->udata )->udata;

	pyzimr_connection_t* connection = (pyzimr_connection_t*) pyzimr_connection_type.tp_new( &pyzimr_connection_type, NULL, NULL );
	connection->_connection = _connection;
	connection->_connection->udata = connection;

	pyzimr_request_t* request = (pyzimr_request_t*) pyzimr_request_type.tp_new( &pyzimr_request_type, NULL, NULL );
	request->_request = &_connection->request;

	pyzimr_response_t* response = (pyzimr_response_t*) pyzimr_response_type.tp_new( &pyzimr_response_type, NULL, NULL );
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
	char* kwlist[ ] = { "url", NULL };
	char* url;

	PyArg_ParseTupleAndKeywords( args, kwargs, "s", kwlist, &url );
	self->_website = zimr_website_create( url );

	( (website_data_t*) self->_website->udata )->udata = self;

	self->connection_handler = PyObject_GetAttrString( m, "defaultConnectionHandler" );
	zimr_website_set_connection_handler( self->_website, pyzimr_website_connection_handler );

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

static PyObject* pyzimr_website_insert_default_page( pyzimr_website_t* self, PyObject* args, PyObject* kwargs ) {
	static char* kwlist[ ] = { "default_page", "pos", NULL };
	const char* default_page = "";
	int pos = -1;

	if ( !PyArg_ParseTupleAndKeywords( args, kwargs, "s|i", kwlist, &default_page, &pos ) ) {
		PyErr_SetString( PyExc_TypeError, "default_page argument must be passed" );
		return NULL;
	}

	zimr_website_insert_default_page( self->_website, default_page, pos );

	Py_RETURN_NONE;
}

static PyObject* pyzimr_website_get_public_directory( pyzimr_website_t* self, void* closure ) {
	if ( !zimr_website_get_pubdir( self->_website ) ) Py_RETURN_NONE;
	return PyString_FromString( zimr_website_get_pubdir( self->_website ) );
}

static int pypdora_website_set_public_directory( pyzimr_website_t* self, PyObject* value, void* closure ) {
	if ( value == NULL ) {
		PyErr_SetString( PyExc_TypeError, "Cannot delete the public_directory attribute" );
		return -1;
	}

	if ( ! PyString_Check( value ) ) {
		PyErr_SetString( PyExc_TypeError, "The public_directory attribute value must be a string" );
		return -1;
	}

	zimr_website_set_pubdir( self->_website, PyString_AsString( value ) );

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

static PyMethodDef pyzimr_website_methods[ ] = {
	{ "enable", (PyCFunction) pyzimr_website_enable, METH_NOARGS, "enable the website" },
	{ "disable", (PyCFunction) pyzimr_website_disable, METH_NOARGS, "disable the website" },
	{ "insertDefaultPage", (PyCFunction) pyzimr_website_insert_default_page, METH_VARARGS | METH_KEYWORDS, "add a default page" },
	{ NULL }  /* Sentinel */
};

static PyMemberDef pyzimr_website_members[ ] = {
	{ NULL }  /* Sentinel */
};

static PyGetSetDef pyzimr_website_getseters[ ] = {
	{
	  "public_directory",
	  (getter) pyzimr_website_get_public_directory,
	  (setter) pypdora_website_set_public_directory,
	  "the websites public directory", NULL
	},
	{
	  "connection_handler",
	  (getter) pyzimr_website_get_connection_handler,
	  (setter) pypdora_website_set_connection_handler,
	  "function to handle web connections", NULL
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

static PyObject* pyzimr_version( ) {
	return PyString_FromString( zimr_version( ) );
}

static PyObject* pyzimr_start( ) {
	zimr_start( );
	Py_RETURN_NONE;
}

static PyObject* pyzimr_default_connection_handler( PyObject* self, PyObject* args, PyObject* kwargs ) {
	pyzimr_connection_t* connection;

	if ( ! PyArg_ParseTuple( args, "O", &connection ) ) {
		PyErr_SetString( PyExc_TypeError, "connection paramater must be passed" );
		return NULL;
	}

	zimr_connection_send_file( connection->_connection, connection->_connection->request.url, true );
	Py_RETURN_NONE;
}

static void pyzimr_page_handler( connection_t* connection, const char* filepath, void* udata ) {
	PyObject* page_handler = udata;
	PyObject* result;

	result = PyObject_CallFunction( page_handler, "sO", filepath, connection->udata );

	if ( result != NULL ) {
		if ( !PyString_Check( result ) ) {
			PyErr_SetString( PyExc_TypeError, "result from page_handler invalid" );
			return;
		}
		zimr_connection_send( connection, (void*) PyString_AsString( result ), strlen( PyString_AsString( result ) ) );

		Py_DECREF( result );
	}

}

static PyObject* pyzimr_register_page_handler( PyObject* self, PyObject* args ) {
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

	zimr_register_page_handler( page_type, (PAGE_HANDLER) &pyzimr_page_handler, (void*) page_handler );

	Py_RETURN_NONE;
}

static PyMethodDef pyzimr_methods[ ] = {
	{ "version",  (PyCFunction) pyzimr_version, METH_NOARGS, "Returns the version of zimr." },
	{ "start", (PyCFunction) pyzimr_start, METH_NOARGS, "Starts the zimr mainloop." },
	{ "defaultConnectionHandler", (PyCFunction) pyzimr_default_connection_handler, METH_VARARGS, "The zimr default connection handler." },
	{ "registerPageHandler", (PyCFunction) pyzimr_register_page_handler, METH_VARARGS, "Register a page handler." },
	{ NULL }		/* Sentinel */
};

PyMODINIT_FUNC initzimr ( void ) {

	if ( PyType_Ready( &pyzimr_website_type ) < 0 )
		return;

	if ( PyType_Ready( &pyzimr_connection_type ) < 0 )
		return;

	if ( PyType_Ready( &pyzimr_request_type ) < 0 )
		return;

	if ( PyType_Ready( &pyzimr_response_type ) < 0 )
		return;

	m = Py_InitModule( "zimr", pyzimr_methods );
	if ( m == NULL )
		return;

	Py_INCREF( &pyzimr_website_type );
	PyModule_AddObject( m, "website", (PyObject*) &pyzimr_website_type );

	Py_INCREF( &pyzimr_connection_type );
	PyModule_AddObject( m, "connection", (PyObject*) &pyzimr_connection_type );

	Py_INCREF( &pyzimr_request_type );
	PyModule_AddObject( m, "request", (PyObject*) &pyzimr_request_type );

	Py_INCREF( &pyzimr_response_type );
	PyModule_AddObject( m, "response", (PyObject*) &pyzimr_response_type );

	Py_AtExit( &zimr_shutdown );

	zimr_init( "" );

	return;
}
