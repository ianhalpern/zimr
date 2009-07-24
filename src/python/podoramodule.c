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
	response_t _response;
} pypodora_response_t;

static PyObject* pypodora_response_serve( PyObject* self, PyObject* args ) {
	const char* message;

	if ( PyArg_ParseTuple( args, "s", &message ) ) {
		podora_response_serve( &( (pypodora_response_t*) self )->_response, (void*) message, strlen( message ) );
	} else {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject* pypodora_response_serveFile( PyObject* self, PyObject* args ) {
	const char* filename;
	unsigned char use_pubdir = 1;

	if ( PyArg_ParseTuple( args, "s|b", &filename, &use_pubdir ) ) {
		podora_response_serve_file( &( (pypodora_response_t*) self )->_response, (char*) filename, use_pubdir );
	} else {
		PyErr_SetString( PyExc_TypeError, "request paramater must be passed" );
		return NULL;
	}

	Py_RETURN_NONE;
}


static PyMethodDef pypodora_response_methods[ ] = {
	{ "serve", (PyCFunction) pypodora_response_serve, METH_VARARGS, "Start the website." },
	{ "serveFile", (PyCFunction) pypodora_response_serveFile, METH_VARARGS, "Stop the website." },
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
	PyObject* response;
	PyObject* website;
	request_t _request;
} pypodora_request_t;

static PyObject* pypodora_request_get_url( pypodora_request_t* self, void* closure ) {
	if ( !self->_request.url ) Py_RETURN_NONE;
	return PyString_FromString( self->_request.url );
}

static PyMemberDef pypodora_request_members[ ] = {
	{ "response", T_OBJECT_EX, offsetof( pypodora_request_t, response ), RO, "response object of this request" },
	{ "website", T_OBJECT_EX, offsetof( pypodora_request_t, website ), RO, "website object from which the request originated" },
	{ NULL }  /* Sentinel */
};

static PyGetSetDef pypodora_request_getseters[ ] = {
	{
	  "url",
	  (getter) pypodora_request_get_url,
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
	0,//pypodora_website_methods,             /* tp_methods */
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

/********** START OF pypodora_website_t *******/
/**********************************************/

typedef struct {
	PyObject_HEAD
	PyObject* request_handler;
	struct website* _website;
} pypodora_website_t;

static void pypodora_website_dealloc( pypodora_website_t* self ) {
	podora_website_destroy( self->_website );
	self->ob_type->tp_free( (PyObject*) self );
}

void pypodora_website_request_handler( request_t _request ) {

	pypodora_website_t* website = (pypodora_website_t*) ( (website_data_t*) _request.website->data )->udata;

	pypodora_request_t* request = (pypodora_request_t*) pypodora_request_type.tp_new( &pypodora_request_type, NULL, NULL );
	request->_request = _request;

	pypodora_response_t* response = (pypodora_response_t*) pypodora_response_type.tp_new( &pypodora_response_type, NULL, NULL );
	response->_response = _request.response;

	request->response = (PyObject*) response;

	Py_INCREF( website );
	request->website = (PyObject*) website;

	PyObject_CallFunctionObjArgs( website->request_handler, request, NULL );

}

static PyObject* pypodora_website_new( PyTypeObject* type, PyObject* args, PyObject* kwargs ) {
	pypodora_website_t* self = (pypodora_website_t*) type->tp_alloc( type, 0 );

	Py_INCREF( Py_None );
	self->request_handler = Py_None;

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

static PyObject* pypodora_website_get_request_handler( pypodora_website_t* self, void* closure ) {
	Py_INCREF( self->request_handler );
	return self->request_handler;
}

static int pypdora_website_set_request_handler( pypodora_website_t* self, PyObject* value, void* closure ) {

	if ( value == NULL ) {
		Py_DECREF( self->request_handler );
		Py_INCREF( Py_None );
		self->request_handler = Py_None;
		podora_website_unset_request_handler( self->_website );
		return 0;
	}

	if ( ! PyCallable_Check( value ) ) {
		PyErr_SetString( PyExc_TypeError, "The request_handler attribute value must be callable" );
		return -1;
	}

	Py_DECREF( self->request_handler );
	Py_INCREF( value );
	self->request_handler = value;

	podora_website_set_request_handler( self->_website, pypodora_website_request_handler );

	return 0;
}


static PyMethodDef pypodora_website_methods[ ] = {
	{ "enable", (PyCFunction) pypodora_website_enable, METH_NOARGS, "Start the website." },
	{ "disable",  (PyCFunction) pypodora_website_disable, METH_NOARGS, "Stop the website." },
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
	  "request_handler",
	  (getter) pypodora_website_get_request_handler,
	  (setter) pypdora_website_set_request_handler,
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

static PyMethodDef pypodora_methods[ ] = {
	{ "version",  (PyCFunction) pypodora_version, METH_NOARGS, "Returns the version of podora." },
	{ "start", (PyCFunction) pypodora_start, METH_NOARGS, "Returns the version of podora." },
	{ NULL }		/* Sentinel */
};

PyMODINIT_FUNC initpodora ( void ) {
	PyObject* m;

	if ( PyType_Ready( &pypodora_website_type ) < 0 )
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

	Py_INCREF( &pypodora_request_type );
	PyModule_AddObject( m, "request", (PyObject*) &pypodora_request_type );

	Py_INCREF( &pypodora_response_type );
	PyModule_AddObject( m, "response", (PyObject*) &pypodora_response_type );

	Py_AtExit( &podora_shutdown );

	podora_init( );

	return;
}
