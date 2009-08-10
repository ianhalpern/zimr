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

#ifndef _PD_PCNF_H
#define _PD_PCNF_H

#include <yaml.h>
#include "config.h"

typedef struct pcnf_website {
	char* url;
	char* pubdir;
	struct pcnf_website* next;
} pcnf_website_t;

typedef struct {
	pcnf_website_t* website_node;
} pcnf_t;

pcnf_t* pcnf_load( );
void pcnf_free( pcnf_t* cnf );
#endif
