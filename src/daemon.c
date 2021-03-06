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

#include "daemon.h"

static void empty_sighandler() { }

static bool is_daemon = false;

static void removelockfile() {
	remove( D_LOCKFILE_PATH );
}

static int createlockfile() {
	int lockfd;
	char pidbuf[ 10 ];
	memset( pidbuf, 0, sizeof( pidbuf ) );
	sprintf( pidbuf, "%d", getpid( ) );

	if ( ( lockfd = creat( D_LOCKFILE_PATH, 0644 ) ) == -1 )
		return 0;
	else if ( write( lockfd, pidbuf, strlen( pidbuf ) ) == -1 )
		return 0;

	close( lockfd );
	atexit( removelockfile );

	return 1;
}

static int readlockfile() {
	int lockfd;
	char pidbuf[ 10 ];

	if ( ( lockfd = open( D_LOCKFILE_PATH, O_RDONLY ) ) == -1 )
		return 0;

	if ( read( lockfd, pidbuf, sizeof( pidbuf ) ) == -1 )
		return 0;

	return atoi( pidbuf );
}

void daemon_redirect_stdio() {
	/* repoen the standard file descriptors */
	freopen( "/dev/null", "w", stdout );
	freopen( "/dev/null", "w", stderr );
	freopen( "/dev/null", "r", stdin  );
}

int daemon_init( int flags ) {

	if ( is_daemon ) {// already daemonized
		printf( "failed: process already daemonized.\n" );
		return 0;
	}

	pid_t pid;
	if ( ( !FL_ISSET( flags, D_NOLOCKFILE ) && !FL_ISSET( flags, D_NOLOCKCHECK ) ) && ( pid = readlockfile() ) ) {// lockfile previously set
		if ( kill( pid, 0 ) == 0 ) { // process running
			printf( "failed: daemon already running.\n" );
			return false;
		} else {
			printf( "warning: previous daemon did not clean up lockfile on exit.\n" );
		}
	}

	return 1;
}

int daemon_detach( int flags ) {

	printf( " * starting daemon..." );
	fflush( stdout );

	void (*prev_sighandler)( int );

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
	pid = fork();
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
	if ( !FL_ISSET( flags, D_NOCD ) && chdir( "/" ) < 0 )
		return 0;

	if ( !FL_ISSET( flags, D_NOLOCKFILE ) ) {
		if ( !createlockfile( ) ) {
			printf( "failed: could not set lockfile.\n" );
			return 0;
		}
	}

	is_daemon = true;

	printf( "started.\n" );

	if ( !FL_ISSET( flags, D_KEEPSTDIO ) ) {
		daemon_redirect_stdio( );
	}
	return 1;
}

int daemon_stop( bool force ) {
	int pid = readlockfile();

	printf( " * stopping daemon..." );
	fflush( stdout );

	if ( !pid ) {
		printf( "failed: no daemon running\n" );
		return false;
	}

	if ( !stopproc( pid ) ) {
		if ( errno == 3 ) {
			printf( "warning: %s\n", strerror( errno ) );
			return true;
		} else {
			if ( !force || !killproc( pid ) ) {
				printf( "failed: %s\n", strerror( errno ) );
				return false;
			} else if ( force )
				printf( "force " );
		}
	}

	printf( "stopped.\n" );
	return true;
}
