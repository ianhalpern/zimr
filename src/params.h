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

#ifndef _ZM_PARAMS_H
#define _ZM_PARAMS_H

#include <stdio.h>
#include <string.h>

#include "simclist.h"

typedef list_t params_t;

#include "config.h"
#include "urldecoder.h"
#include "connection.h"

#define PARAM_TYPE_GET  1
#define PARAM_TYPE_POST 2
#define PARAM_TYPE_CUSTOM 3


typedef struct {
	char name[PARAMS_NAME_MAX_LEN];
	char* value;
	size_t value_len;
	int type;
	bool val_alloced;
} param_t;

params_t params_create();
void params_parse_qs( params_t* params, char* raw, int size, int type );
void params_parse_multiparts( params_t* params, request_t* multiparts[], int type );
char* params_gen_qs( params_t* params, char* qs );
size_t params_qs_len( list_t* params );
param_t* params_get_param( params_t* params, const char* name );
param_t* params_set_param( params_t* params, const char* name, char* value );
void params_free( params_t* params );

#endif
