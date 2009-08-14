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

#include "pderr.h"

static const char* errorstrings[ ] = {
/* PD_OK */				"OK",
/* PDERR_FAILED  */		"Failed",
/* PDERR_EXISTS  */		"Already Exists",
/* PDERR_PSOCK_CREAT */	"Could not create socket",
/* PDERR_PSOCK_BIND  */	"Could not bind to socket",
/* PDERR_PSOCK_LISTN */	"Could not listen on socket",
/* PDERR_PSOCK_CONN  */	"Could not connect to socket",
/* PDERR_SOCK_CLOSED */	"Connection Closed"
};

const char* pdstrerror( int my_pderrno ) {
	if ( my_pderrno > 0 || my_pderrno <= PDERR_LAST )
		return "";
	return errorstrings[ abs( my_pderrno ) ];
}

void pderr( int my_pderrno ) {
	if ( my_pderrno > 0 || my_pderrno <= PDERR_LAST )
		return;
	pderrno = my_pderrno;
}
