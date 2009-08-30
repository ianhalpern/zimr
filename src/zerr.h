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

#ifndef _ZM_ZMERR_H
#define _ZM_ZMERR_H

#include <stdlib.h>

#define ZM_OK       0
#define ZMERR_FAILED -1
#define ZMERR_EXISTS -2
#define ZMERR_ZSOCK_CREAT -3
#define ZMERR_ZSOCK_BIND  -4
#define ZMERR_ZSOCK_LISTN -5
#define ZMERR_ZSOCK_CONN  -6
#define ZMERR_SOCK_CLOSED -7
#define ZMERR_LAST -8

int zerrno;

const char* pdstrerror( int zerrno );
void zerr( int zerrno );

#endif
