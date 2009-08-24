/*   Pacoda - Next Generation Web Server
 *
 *+  Copyright (c) 2009 Ian Halpern
 *@  http://Pacoda.org
 *
 *   This file is part of Pacoda.
 *
 *   Pacoda is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Pacoda is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Pacoda.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef _PD_PDERR_H
#define _PD_PDERR_H

#include <stdlib.h>

#define PD_OK       0
#define PDERR_FAILED -1
#define PDERR_EXISTS -2
#define PDERR_PSOCK_CREAT -3
#define PDERR_PSOCK_BIND  -4
#define PDERR_PSOCK_LISTN -5
#define PDERR_PSOCK_CONN  -6
#define PDERR_SOCK_CLOSED -7
#define PDERR_LAST -8

int pderrno;

const char* pdstrerror( int pderrno );
void pderr( int pderrno );

#endif
