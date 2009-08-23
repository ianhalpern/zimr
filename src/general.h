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

#ifndef _PD_GENERAL_H
#define _PD_GENERAL_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>

#include "config.h"

int randkey( );
int url2int( const char* );
void wait_for_kill( );
int startswith( const char*, const char* );
char* normalize( char* normpath, const char* path );
int xtoi( const char* xs, unsigned int* result );
char* expand_tilde( char* path, char* buffer, int size );
bool stopproc( pid_t pid );
char *strnstr( const char *s, const char *find, size_t slen);

#endif

