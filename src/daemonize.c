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

#include "daemonize.h"

static void empty_sighandler(){}
 
int daemonize( int flags ) {

	void (*prev_sighandler)(int);
	if ( ( prev_sighandler = signal( SIGHUP, empty_sighandler ) ) != SIG_DFL )
		signal( SIGHUP, prev_sighandler );
	if ( ( prev_sighandler = signal( SIGTERM, empty_sighandler ) ) != SIG_DFL )
		signal( SIGTERM, prev_sighandler );
	if ( ( prev_sighandler = signal( SIGINT, empty_sighandler ) ) != SIG_DFL )
		signal( SIGINT, prev_sighandler );
	if ( ( prev_sighandler = signal( SIGQUIT, empty_sighandler ) ) != SIG_DFL )
		signal( SIGQUIT, prev_sighandler );


	/* Our process ID and Session ID */
	pid_t pid, sid;

	/* Fork off the parent process */
	pid = fork( );
	if ( pid < 0 )
		return 0;

	/* If we got a good PID, then
	   we can exit the parent process. */
	if ( pid > 0 )
		exit( EXIT_SUCCESS );

	/* Change the file mode mask */
	umask( 0 );

	/* Create a new SID for the child process */
	sid = setsid( );
	if ( sid < 0 )
		return 0;

	/* Change the current working directory */
	if ( ! flags & D_NOCD && chdir( "/" ) < 0 )
		return 0;

	if ( ! flags & D_KEEPSTDF ) {
		/* repoen the standard file descriptors */
		freopen( "/dev/null", "w", stdout );
		freopen( "/dev/null", "w", stderr );
		freopen( "/dev/null", "r", stdin  );
	}

	return 1;
}
