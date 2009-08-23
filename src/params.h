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

#ifndef _PD_PARAMS_H
#define _PD_PARAMS_H

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "urldecoder.h"

typedef struct {
	char name[ PARAM_NAME_MAX_LEN ];
	char* value;
} param_t;

typedef struct {
	int num;
	param_t list[ PARAMS_MAX_NUM ];
} params_t;

params_t* params_create( );
void params_parse_qs( params_t* params, char* raw, int size );
param_t* params_get_param( params_t* params, const char* name );
void params_free( params_t* params );

#endif
