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

#include "zerr.h"

static const char* errorstrings[ ] = {
/* ZM_OK */				"OK",
/* ZMERR_FAILED  */		"Failed",
/* ZMERR_EXISTS  */		"Already Exists",
/* ZMERR_PSOCK_CREAT */	"Could not create socket",
/* ZMERR_PSOCK_BIND  */	"Could not bind to socket",
/* ZMERR_PSOCK_LISTN */	"Could not listen on socket",
/* ZMERR_PSOCK_CONN  */	"Could not connect to socket",
/* ZMERR_SOCK_CLOSED */	"Connection Closed"
};

const char* pdstrerror( int my_zerrno ) {
	if ( my_zerrno > 0 || my_zerrno <= ZMERR_LAST )
		return "";
	return errorstrings[ abs( my_zerrno ) ];
}

void zerr( int my_zerrno ) {
	if ( my_zerrno > 0 || my_zerrno <= ZMERR_LAST )
		return;
	zerrno = my_zerrno;
}
