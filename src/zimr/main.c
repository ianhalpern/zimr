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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>
#include <fcntl.h>
#include <pwd.h>
#include <time.h>
#include <proc/readproc.h>

#include "zimr.h"
#include "cli.h"

#define ADD     0x1
#define START   0x2
#define STOP    0x3
#define RESTART 0x4
#define REMOVE  0x5
#define STATUS  0x6

#define CMDSTR(X) (X==ADD?"Adding":X==START?"Starting":X==STOP?"Stopping":X==RESTART?"Restarting":X==REMOVE?"Removing":X==STATUS?"Status":"Unknown")

#define GETCNFARG(cnf_path) (strcmp( "/" ZM_APP_CNF_FILE, strrchr( cnf_path, '/' ) ) == 0 ? "" : " " ),(strcmp( "/" ZM_APP_CNF_FILE, strrchr( cnf_path, '/' ) ) == 0 ? "" : strrchr( cnf_path, '/' ) + 1 )

cli_cmd_t root_cmd;
pid_t ppid = 0;
bool wait_for_child = true;
static bool verbose = true;
static bool on_shutdown_restart = false;
#ifdef DEBUG
static bool on_shutdown_coredump = false;
#endif

bool application_function( uid_t uid, gid_t gid, char* cnf_path, char type, bool force, bool allow_disable );

void print_usage() {
	printf( "Zimr v" ZIMR_VERSION " (" BUILD_DATE ") - " ZIMR_WEBSITE "\n\n" );
	printf( "Commands:\n\n" );
	cli_print( &root_cmd );
#ifdef DEBUG
	puts("\nDebugging options:\n");
	puts("  The extra options \"-coredump\" is enabled for commands \"start\" and \"restart\"");
#endif
}

void sigquit() {
	//exit(0);
}

void sig_from_child() {
	wait_for_child = false;
}

// Functions ////////////////////////////
void application_shutdown() {
	//puts( "shutdown" );
	zimr_shutdown();
}

void event_handler_when_in_background( int event, va_list ap ) {
	switch ( event ) {
		case ZIMR_EVENT_RESTART_REQUEST:
			on_shutdown_restart = true;
			exit( EXIT_SUCCESS );
			break;
	}

	return;

}

void event_handler_verbose( int event, va_list ap ) {

	char* c1,* c2,* c3;
	int i1;
	list_t* l1;

	switch ( event ) {
		case ZIMR_EVENT_INITIALIZING:
			printf( "Starting Zimr webapp (v" ZIMR_VERSION " - " ZIMR_WEBSITE ")...\n" );
			break;
		case ZIMR_EVENT_LOADING_CNF:
			printf("[Loading webapp] %s\n", strrchr( va_arg( ap, char* ), '/') + 1 );
			break;
		case ZIMR_EVENT_MODULE_LOADING:
			printf( "[Loading module] %s\n", va_arg( ap, char* ) );
			break;
		case ZIMR_EVENT_MODULE_LOAD_FAILED:
		case ZIMR_EVENT_WEBSITE_MODULE_INIT_FAILED:
			printf( "[error] %s failed to load\n", va_arg( ap, char* ) );
			goto kill;
		case ZIMR_EVENT_WEBSITE_ENABLE_SUCCEEDED:
			c1 = va_arg( ap, char* );
			c2 = va_arg( ap, char* );
			i1 = va_arg( ap, int );
			//printf( "Website enabled: %s (on proxy %s:%d)\n", c1, c2, i1 );
			break;
		case ZIMR_EVENT_WEBSITE_ENABLE_FAILED:
			c1 = va_arg( ap, char* );
			c2 = va_arg( ap, char* );
			i1 = va_arg( ap, int );
			c3 = va_arg( ap, char* );
			printf( "[Url failed] %s (on proxy %s:%d): %s\n", c1, c2, i1, c3 );
			goto kill;
		case ZIMR_EVENT_WEBSITE_MODULE_INIT:
			c1 = va_arg( ap, char* );
			c2 = va_arg( ap, char* );
			//printf( "Initializing module %s for website %s\n", c1, c2 );
			break;
		case ZIMR_EVENT_ALL_WEBSITES_ENABLED:
			printf( "Service is now running.\n" );
			goto background;
			break;
		case ZIMR_EVENT_REGISTERING_WEBSITES:
			l1 = va_arg( ap, list_t* );
			//printf("[Registering urls]\n");
			for ( i1 = 0; i1 < list_size( &websites ); i1++ )
				printf( "[Registering url] %s\n", ((website_t*)list_get_at( &websites, i1 ))->full_url );

			break;
	}

	return;

background:
	zimr_register_event_handler( event_handler_when_in_background );
	if ( ppid && wait_for_child ) {
		kill( ppid, SIGCHLD );
	}
	return;

kill:
	printf( "Stopping webapp service...\n" );
	exit(EXIT_FAILURE);
	//zimr_register_event_handler( NULL );
	//if ( ppid && wait_for_child ) {
	//	kill( ppid, SIGINT );
	//}
	return;
}

void event_handler_basic( int event, va_list ap ) {
	switch ( event ) {
		case ZIMR_EVENT_ALL_WEBSITES_ENABLED:
			goto background;
			break;
		case ZIMR_EVENT_WEBSITE_ENABLE_FAILED:
			goto background;
			break;
	}

background:
	zimr_register_event_handler( event_handler_when_in_background );
	if ( ppid && wait_for_child )
		kill( ppid, SIGCHLD );
}

pid_t application_exec( uid_t uid, gid_t gid, char* path, bool nofork ) {
	if ( !nofork )
		ppid = getpid();

	pid_t pid = !nofork ? fork() : 0;
	char dir[ PATH_MAX ],* ptr;

	if ( pid == 0 ) {
		// in the child
		ptr = strrchr( path, '/' );
		strncpy( dir, path, ptr - path );
		dir[ ptr - path ] = 0;
		chdir( dir );

		setuid( uid );
		setgid( gid );

		if ( verbose )
			zimr_register_event_handler( event_handler_verbose );
		else
			zimr_register_event_handler( event_handler_basic );

		if ( !verbose )
			freopen( ZM_ERR_LOGFILE, "a", stderr );

		zimr_init();

		signal( SIGTERM, sigquit );
		signal( SIGINT,  sigquit );

		assert( zimr_cnf_load( path ) );

		atexit( application_shutdown );

		//freopen( ZM_OUT_LOGFILE, "w", stdout );
		if ( verbose )
			freopen( ZM_ERR_LOGFILE, "a", stderr );

		zimr_start();

		if ( on_shutdown_restart ) {
			int ret;
			pid = fork();
			if ( pid == 0 ) {
				char *cmd[] = { "zimr", "restart", path, 0 };
				ret = execvp( cmd[0], cmd );
				if ( ret == -1 )
					printf( "%s: %d\n", strerror( errno ), errno );
			}
		}

#ifdef DEBUG
		if ( on_shutdown_coredump ) {
			puts("Core dumped for debugging");
			abort();
		}
#endif

		exit( EXIT_SUCCESS );
	} else if ( pid == (pid_t) -1 ) {
		// still in parent, but there is no child
		puts( "parent error: no child" );
		return -1;
	} else if ( wait_for_child ) {
		signal( SIGCHLD, sig_from_child );
		//sleep( 1 );
		//int exitstat;
		while ( wait_for_child )
			sleep(1);
		wait_for_child = true;
		/*if ( exitstat ) {
			//waitpid( pid, &exitstat, 0 );
			return -1;
		}*/
	}

	return pid;
}

bool application_kill( pid_t pid, bool force ) {
	if (verbose) {
		printf( "Stopping webapp service..." );
		fflush(stdout);
	}
	if ( !pid ) goto success;

	if ( force ) {
		if ( !killproc( pid ) && errno != ESRCH )
			goto fail;
	}

	else if ( !stopproc( pid ) && errno != ESRCH ) {
		goto fail;
	}

success:
	if (verbose) printf( "Stopped.\n" );
	return true;
fail:
	if (verbose) printf("\n");
	return false;
}

bool application_function( uid_t uid, gid_t gid, char* cnf_path, char type, bool force, bool allow_disable ) {
	//printf( " * %s %s\n", CMDSTR( type ), cnf_path ); //fflush( stdout );

	userdir_init( uid );
	zcnf_state_t* state = zcnf_state_load( uid );

	if ( !state ) {
		puts( "failed to load state" );
		return false;
	}

	bool retval = true;

	zcnf_state_app_t* app = NULL;

	int i, j;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		if ( strcmp( app->path, cnf_path ) == 0 ) {
			if ( type == ADD ) {
				printf( "Failed: already added. To start the webapp run 'zimr start%s%s'\n", GETCNFARG( cnf_path ) );
				retval = false;
				goto quit;
			}
			break;
		}
		app = NULL;
	}

	if ( type != ADD && !app ) {
		if ( type == REMOVE )
			printf( "Nothing to do, webapp has already been removed\n" );
		else
			printf( "Failed: webapp does not exist. Run 'zimr add%s%s' to add the webapp\n", GETCNFARG( cnf_path ) );
		retval = false;
		goto quit;
	}

	else {
		pid_t pid;
		switch ( type ) {
			case ADD:
				printf( "Zimr webapp added. To start the webapp run 'zimr start%s%s'\n", GETCNFARG( cnf_path ) );
				zcnf_state_set_app( state, cnf_path, 0, true );
				break;
			case START:

				if ( !verbose ) {
					printf( "[Starting] %s...", cnf_path );
					fflush(stdout);
				}

				if ( !app->stopped && app->pid && kill( app->pid, 0 ) != -1 ) { // process still running, not dead
					retval = false;
					printf( "Failed: already running, try 'zimr restart%s%s' instead.\n", GETCNFARG( cnf_path ) );
					goto quit;
				}
				if ( ( pid = application_exec( uid, gid, cnf_path, false ) ) == -1 ) {
					retval = false;
					printf( "Failed\n" );
					goto quit;
				}

				if ( !verbose ) printf("Started.\n");

				zcnf_state_set_app( state, cnf_path, pid, false );
				break;
			case RESTART:
				if ( app->stopped ) {
					retval = false;
					printf( "Failed: webapp is not running. To start a webapp run 'zimr start%s%s'\n", GETCNFARG( cnf_path ) );
					goto quit;
				}

				if ( !verbose ) {
					printf( "[Restaring] %s...", cnf_path );
					fflush(stdout);
				}

				if ( !application_kill( app->pid, force ) ) {
					retval = false;
					printf( "Failed: %s\n", strerror( errno ) );
					goto quit;
				}

				if ( verbose ) printf("\n");

				if ( ( pid = application_exec( uid, gid, app->path, false ) ) == -1 ) {
					retval = false;
					printf( "Failed\n" );
					//print_errors( app->path );
					goto quit;
				}

				if ( !verbose ) printf("Restarted.\n");

				//print_errors( app->path );
				zcnf_state_set_app( state, app->path, pid, !retval );
				break;
			case STOP:
				if ( !verbose ) {
					printf( "[Stopping] %s...", cnf_path );
					fflush(stdout);
				}
				if ( !app->pid ) {
					printf( "Nothing to do, webapp is not running\n" );
				} else if ( !application_kill( app->pid, force ) ) {
					retval = false;
					printf( "Failed: %s\n", strerror( errno ) );
					goto quit;
				}

				if ( !verbose ) printf("Stopped.\n");

				zcnf_state_set_app( state, app->path, 0, allow_disable );
				break;
			case REMOVE:
				if ( app->pid && !application_kill( app->pid, force ) ) {
					retval = false;
					printf( "Failed: %s\n", strerror( errno ) );
					//print_errors( cnf_path );
					goto quit;
				}
				printf( "Zimr webapp removed.\n" );
				list_delete_at( &state->apps, i );
				zcnf_state_app_free( app );
				break;
			case STATUS:
				if ( app->stopped )
					printf( "[Disabled] %s\n", cnf_path );
				else if ( !app->pid || kill( app->pid, 0 ) == -1 ) // process still running, not dead
					printf( "[Not Running] %s\n", cnf_path );
				else {
					printf( "[Running] %s\n", cnf_path );

					if ( !verbose ) break;
					///////////////////// Proc Info /////////////////////////
					// TODO: calculate CPU usage: http://stackoverflow.com/questions/1420426/calculating-cpu-usage-of-a-process-in-linux
					FILE* kstat = fopen( "/proc/stat", "r" );
					char buf[256], btime_s[32], *ptr;
					time_t btime = 0;

					while ( kstat && fgets( buf, sizeof( buf ), kstat ) ) {
						if ( startswith( buf, "btime" ) ) {
							ptr = strchr( buf+6, '\n' );
							strncpy( btime_s, buf+6, ( ptr - buf+6 ) );
							btime_s[ ( ptr - buf+6 ) ] = 0;
							btime = strtoumax( btime_s, NULL, 0 );
							break;
						}
					}

					PROCTAB* ptab = openproc( PROC_FILLMEM, &app->pid );
					proc_t* proc = readproc( ptab, NULL );
					get_proc_stats( app->pid, proc );

					unsigned long long timediff = time( NULL ) - btime - proc->start_time / 100;
					int hours = timediff / 3600;
					int minutes = ( timediff - hours * 3600 ) / 60;
					int seconds = ( timediff - hours * 3600 - minutes * 60 );

					printf( "  pid: %d  mem: %li  run time: %02d:%02d:%02d  num threads: %d\n",
					  app->pid, proc->size /*+ proc->share*/, hours, minutes, seconds, proc->nlwp );

					freeproc( proc );
					closeproc( ptab );

					///////////////////// Webapp Info /////////////////////////
					zcnf_app_t* cnf = zcnf_app_load( cnf_path );
					assert( cnf );

					for ( i = 0; i < list_size( &cnf->websites ); i++ ) {
						zcnf_website_t* website_cnf = list_get_at( &cnf->websites, i );
						printf( "  Url: " );
						if ( !startswith( website_cnf->url, "https://" ) && !startswith( website_cnf->url, "http://" ) )
							printf( "http://" );

						printf( "%s", website_cnf->url );

						if ( website_cnf->redirect_url )
							printf( " -> %s", website_cnf->redirect_url );

						if ( list_size( &website_cnf->modules ) ) {
							printf( "  Modules:" );
							for ( j = 0; j < list_size( &website_cnf->modules ); j++ ) {
								zcnf_module_t* module_cnf = list_get_at( &website_cnf->modules, j );
								printf( " %s", module_cnf->name );
							}
						}
						puts("");
					}
				}

				break;
		}
	}

	zcnf_state_save( state );

quit:
	zcnf_state_free( state );
	return retval;
}

int state_apps_status_sort( const zcnf_state_app_t* a, const zcnf_state_app_t* b ) {
	int as, bs;

	if ( a->stopped ) as = 3; // stopped
	else if ( !a->pid || kill( a->pid, 0 ) == -1 ) as = 2; // not running
	else as = 1; // running

	if ( b->stopped ) bs = 3;
	else if ( !b->pid || kill( b->pid, 0 ) == -1 ) bs = 2;
	else bs = 1;

	return as > bs ? -1 : ( as == bs ? 0 : 1 );
}

bool state_function( uid_t uid, gid_t gid, char type, int optc, char* optv[] ) {
	zcnf_state_t* state = zcnf_state_load( uid );
	list_attributes_comparator( &state->apps, state_apps_status_sort );

	verbose = false;

	if ( !state ) {
		puts( "failed to load state" );
		return false;
	}

	if ( type == STATUS ) {
		if ( list_sort( &state->apps, 1 ) )
			puts("error sorting");
	}

	bool retval = true;

	zcnf_state_app_t* app;

	int i;
	bool allow_disable = true;
	bool ignore_disabled = false;

	for ( i = 0; i < optc; i++ ) {
		if ( strcmp( optv[i], "-ignore-disabled" ) == 0 )
			ignore_disabled = true;
		else if ( strcmp( optv[i], "-no-disable" ) == 0 )
			allow_disable = false;
		else if ( strcmp( optv[i], "-dont-wait-for-webapps" ) == 0 )
			wait_for_child = false;
	}

	for ( i = 0; i < list_size( &state->apps ); i++ ) {

		app = list_get_at( &state->apps, i );

		//if ( i ) printf( "\n" );

		if ( type == START ) {
			if ( optc && ignore_disabled && app->stopped ) continue;
			else if ( app->pid && kill( app->pid, 0 ) != -1 ) {
			//	printf( " * Starting %s\n * Skipping: Already started.\n", app->path );
				continue;
			}
		} else if ( type == STOP ) {
			if ( app->stopped ) continue;
		} else if ( type == RESTART ) {
			if ( app->stopped ) continue;
		}

		if ( !application_function( uid, gid, app->path, type, false, allow_disable ) ) {
			//retval = false;
			//goto quit;
		}
	}

//quit:
	zcnf_state_free( state );
	return retval;
}

bool state_all_function( char type, int optc, char* optv[] ) {
	if ( getuid() ) {
		puts( "Failed: Must have superuser privileges" );
		return false;
	}

	struct passwd *p;

	while ( ( p = getpwent() ) ) {
		char filepath[128];

		if ( !expand_tilde_with( ZM_USR_STATE_FILE, filepath, sizeof( filepath ), p->pw_dir ) )
			continue;

		int fd;
		if ( ( fd = open( filepath, O_RDONLY ) ) == -1 ) continue;
		close( fd );

		printf( "User: %s (%s)\n", p->pw_name, filepath );

		state_function( p->pw_uid, p->pw_gid, type, optc, optv );
	}

	endpwent();

	exit( EXIT_SUCCESS );
}

///////////////////////////////////////////////////////

void application_add_cmd( int optc, char* optv[] ) {
	char cnf_path[ PATH_MAX ] = "";
	getcwd( cnf_path, sizeof( cnf_path ) );
	strcat( cnf_path, "/" ZM_APP_CNF_FILE );


	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( !i ) {
			realpath( optv[i], cnf_path );
		} else {
			print_usage();
			return;
		}
	}

//	if ( nostate )
//		application_exec( getuid(), cnf_path, true );

	if ( !application_function( getuid(), getgid(), cnf_path, ADD, false, false ) )
		exit( EXIT_FAILURE );

}

void application_start_cmd( int optc, char* optv[] ) {
	char cnf_path[ PATH_MAX ] = "";
	getcwd( cnf_path, sizeof( cnf_path ) );
	strcat( cnf_path, "/" ZM_APP_CNF_FILE );

	bool nostate = false;
	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( strcmp( optv[i], "-in-foreground" ) == 0 ) {
			nostate = true;
		}
#ifdef DEBUG
		else if ( strcmp( optv[i], "-coredump" ) == 0 ) {
			on_shutdown_coredump = true;
		}
#endif
		else if ( !i ) {
			realpath( optv[i], cnf_path );
		}
		else {
			print_usage();
			return;
		}
	}

	if ( nostate )
		application_exec( getuid(), getgid(), cnf_path, true );

	else if ( !application_function( getuid(), getgid(), cnf_path, START, false, false ) )
		exit( EXIT_FAILURE );

}

void application_stop_cmd( int optc, char* optv[] ) {
	char cnf_path[ PATH_MAX ] = "";
	getcwd( cnf_path, sizeof( cnf_path ) );
	strcat( cnf_path, "/" ZM_APP_CNF_FILE );

	bool force = false;
	bool allow_disable = true;
	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( strcmp( optv[i], "-force" ) == 0 ) {
			force = true;
		} else if ( strcmp( optv[i], "-no-disable" ) == 0 ) {
			allow_disable = false;
		} else if ( !i )
			realpath( optv[i], cnf_path );
		else {
			print_usage();
			return;
		}
	}

	if ( !application_function( getuid(), getgid(), cnf_path, STOP, force, allow_disable ) )
		exit( EXIT_FAILURE );
}

void application_restart_cmd( int optc, char* optv[] ) {
	char cnf_path[ PATH_MAX ] = "";
	getcwd( cnf_path, sizeof( cnf_path ) );
	strcat( cnf_path, "/" ZM_APP_CNF_FILE );

	bool force = false;
	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( strcmp( optv[i], "-force" ) == 0 ) {
			force = true;
		}
#ifdef DEBUG
		else if ( strcmp( optv[i], "-coredump" ) == 0 ) {
			on_shutdown_coredump = true;
		}
#endif
		else if ( !i ) {
			realpath( optv[i], cnf_path );
		}
		else {
			print_usage();
			return;
		}
	}

	if ( !application_function( getuid(), getgid(), cnf_path, RESTART, force, false ) )
		exit( EXIT_FAILURE );
}

void application_remove_cmd( int optc, char* optv[] ) {
	char cnf_path[ PATH_MAX ] = "";
	getcwd( cnf_path, sizeof( cnf_path ) );
	strcat( cnf_path, "/" ZM_APP_CNF_FILE );

	bool force = false;
	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( strcmp( optv[i], "-force" ) == 0 ) {
			force = true;
		} else if ( !i ) {
			realpath( optv[i], cnf_path );
		} else {
			print_usage();
			return;
		}
	}

	if ( !application_function( getuid(), getgid(), cnf_path, REMOVE, force, false ) )
		exit( EXIT_FAILURE );
}

void application_status_cmd( int optc, char* optv[] ) {
	char cnf_path[ PATH_MAX ] = "";
	getcwd( cnf_path, sizeof( cnf_path ) );
	strcat( cnf_path, "/" ZM_APP_CNF_FILE );

	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( !i )
			realpath( optv[i], cnf_path );
		else {
			print_usage();
			return;
		}
	}

	if ( !application_function( getuid(), getgid(), cnf_path, STATUS, false, false ) )
		exit( EXIT_FAILURE );
}

void state_start_cmd( int optc, char* optv[] ) {
	if ( !state_function( getuid(), getgid(), START, optc, optv ) )
		exit( EXIT_FAILURE );
}

void state_stop_cmd( int optc, char* optv[] ) {
	if ( !state_function( getuid(), getgid(), STOP, optc, optv ) )
		exit( EXIT_FAILURE );
}

void state_restart_cmd( int optc, char* optv[] ) {
	if ( !state_function( getuid(), getgid(), RESTART, optc, optv ) )
		exit( EXIT_FAILURE );
}

void state_remove_cmd( int optc, char* optv[] ) {
	if ( !state_function( getuid(), getgid(), REMOVE, optc, optv ) )
		exit( EXIT_FAILURE );
}

void state_status_cmd( int optc, char* optv[] ) {
	if ( !state_function( getuid(), getgid(), STATUS, optc, optv ) )
		exit( EXIT_FAILURE );
}

void state_start_all_cmd( int optc, char* optv[] ) {
	if ( !state_all_function( START, optc, optv ) )
		exit( EXIT_FAILURE );
}

void state_restart_all_cmd( int optc, char* optv[] ) {
	if ( !state_all_function( RESTART, optc, optv ) )
		exit( EXIT_FAILURE );
}

void state_stop_all_cmd( int optc, char* optv[] ) {
	if ( !state_all_function( STOP, optc, optv ) )
		exit( EXIT_FAILURE );
}

void state_remove_all_cmd( int optc, char* optv[] ) {
	if ( !state_all_function( REMOVE, optc, optv ) )
		exit( EXIT_FAILURE );
}

void state_status_all_cmd( int optc, char* optv[] ) {
	if ( !state_all_function( STATUS, optc, optv ) )
		exit( EXIT_FAILURE );
}

void help_cmd( int optc, char* optv[] ) {
	print_usage();
}

// Proxy Functions ////////////////////////////

/*void proxy_status() {
	int command = ZM_CMD_STATUS;
	zsocket_init();
	zsocket_t* proxy = zsocket_connect( inet_addr( ZM_PROXY_ADDR ), ZM_PROXY_PORT );

	if ( !proxy ) {
		printf( "could not connect to proxy.\n" );
		return;
	}

	ztransport_t* transport;

	transport = ztransport_open( proxy->sockfd, ZT_WO, command );
	ztransport_close( transport );

	transport  = ztransport_open( proxy->sockfd, ZT_RO, 0 );
	ztransport_read( transport );
	ztransport_close( transport );

	puts( transport->message );

	zsocket_close( proxy );
}

void proxy_status_cmd( int optc, char* optv[] ) {
	proxy_status();
}*/

///////////////////////////////////////////////


int main( int argc, char* argv[] ) {

	cli_cmd_t stop_all_commands[] = {
		{ "users", "Stop all users' webapps. [ -no-disable, -dont-wait-for-webapps ]\n", NULL, &state_stop_all_cmd },
		{ NULL }
	};

	cli_cmd_t stop_commands[] = {
		{ "all", "Stop all of the current user's webapps. [ -no-disable ]", &stop_all_commands, &state_stop_cmd },
		{ NULL }
	};

	cli_cmd_t start_all_commands[] = {
		{ "users", "Start all users' stopped webapps. [ -ignore-disabled, -dont-wait-for-webapps ]\n", NULL, &state_start_all_cmd },
		{ NULL }
	};

	cli_cmd_t start_commands[] = {
		{ "all", "Start all of the current user's stopped webapps. [ -ignore-disabled ]", &start_all_commands, &state_start_cmd },
		{ NULL }
	};

	cli_cmd_t restart_all_commands[] = {
		{ "users", "Restart all users' running webapps. [ -dont-wait-for-webapps ]\n", NULL, &state_restart_all_cmd },
		{ NULL }
	};

	cli_cmd_t restart_commands[] = {
		{ "all", "Restart all of the current user's running webapps.", &restart_all_commands, &state_restart_cmd },
		{ NULL }
	};

	cli_cmd_t remove_all_commands[] = {
		{ "users", "Remove all users' webapps. [ -dont-wait-for-webapps ]\n", NULL, &state_remove_all_cmd },
		{ NULL }
	};

	cli_cmd_t remove_commands[] = {
		{ "all", "Remove all of the current user's webapps.", &remove_all_commands, &state_remove_cmd },
		{ NULL }
	};

	cli_cmd_t status_all_commands[] = {
		{ "users", "View the status of all users' webapps. [ -dont-wait-for-webapps ]\n", NULL, &state_status_all_cmd },
		{ NULL }
	};

	cli_cmd_t status_commands[] = {
		{ "all", "View the status of all of the current user's webapps.", &status_all_commands, &state_status_cmd },
		{ NULL }
	};

	cli_cmd_t zimr_commands[] = {
		{ "add",     "Add a webapp. [ <config path> ]\n", NULL, &application_add_cmd },
		{ "start",   "Start a stopped webapp. [ <config path>, -in-foreground ]", &start_commands, &application_start_cmd },
		{ "restart", "Restart a running webapp. [ <config path>, -force ]", &restart_commands, &application_restart_cmd },
		{ "stop",    "Stop a webapp. [ <config path>, -force, -no-disable ]", &stop_commands, &application_stop_cmd },
		{ "status",  "View the status of a webapp. [ <config path> ]", &status_commands, &application_status_cmd },
		{ "remove",  "Remove a webapp. [ <config path>, -force ]", &remove_commands, &application_remove_cmd },
		{ "help",    "List all commands.", NULL, &help_cmd },
		{ NULL }
	};

	root_cmd.name = "zimr";
	root_cmd.description = "";
	root_cmd.sub_commands = zimr_commands;
	root_cmd.func = NULL;

	char* last_slash = strrchr( argv[ 0 ], '/' );
	if ( last_slash ) argv[ 0 ] = last_slash + 1;

	if ( !cli_run( &root_cmd, argc, argv ) )
		print_usage();

	return EXIT_SUCCESS;
}
