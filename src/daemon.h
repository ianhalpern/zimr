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

#ifndef _ZM_DAEMON_H
#define _ZM_DAEMON_H

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>

#include "general.h"
#include "config.h"

// to change the lockfile path
// just define D_LOCKFILE_PATH
// before including "daemonize.h"
#ifndef D_LOCKFILE_PATH
#define D_LOCKFILE_PATH "daemon.pid"
#endif

#define D_KEEPSTDIO   0x01
#define D_NOCD        0x02
#define D_NOLOCKFILE  0x04
#define D_NOLOCKCHECK 0x08

int daemon_start( int flags );
int daemon_stop( );
void daemon_redirect_stdio( );
#endif
