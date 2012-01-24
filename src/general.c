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

#include "general.h"

static int srand_called = 0;

int randkey() {
	int key;

	if ( !srand_called ) {
		srand( time( 0 ) );
		srand_called = 1;
	}

	key = rand();
	//TODO: bitshift only temporary, remember to change it back
	return key >> 24;
}

int url2int( const char* url ) {
	int h = 0, i = 0;

	for ( i = 0; i < strlen( url ); i++ ) {
		h = 31 * h + url[ i ];
	}

	return h;
}

void wait_for_kill( ) {
	sigset_t set;
	int sig;

	sigemptyset( &set );
	sigaddset( &set, SIGINT );
	sigprocmask( SIG_BLOCK, &set, NULL );

	if ( sigwait( &set, &sig ) == -1 )
		perror( "[error] wait_for_kill: sigwait() failed" );
	else if ( sig != 2 )
		fprintf( stderr, "[warning] wait_for_kill: sigwait() returned with sig: %d\n", sig );
}

int startswith( const char* s1, const char* s2 ) {
	int i;

	//if( size > strlen( s1 ) ) return 0;
	for ( i = 0; *(s2 + i); i++ )
		if ( *(s1 + i) != *(s2 + i) || !*(s1 + i) )
			return 0;

	return 1;
}

#define MAX_PATH_INDECIES 100
char* normalize_path( char* normpath, const char* path ) {
	char* ptr;
	int i = 0, pathindcies[ MAX_PATH_INDECIES ];

	normpath[0]  = 0;
	pathindcies[0] = 0;

	while ( ( ptr = strchr( path, '/' ) ) || ( ptr = (char*) path + strlen( path ) ) ) {
		bool is_part = true;
		if ( ptr - path == 1 && path[0] == '.' )
			is_part = false;
		else if ( ptr - path == 2 && path[0] == '.' && path[1] == '.' ) {
			if (i) i--;
			is_part = false;
		} else if ( ptr - path == 0 )
			is_part = false;

		if ( ptr[0] == '/' ) ptr++;

		if ( is_part ) {
			strncpy( normpath + pathindcies[i], path, ptr - path );
			pathindcies[++i] = pathindcies[i-1] + ( ptr - path );
		}

		*(normpath + pathindcies[i]) = 0;
		if ( !ptr[0] || i == MAX_PATH_INDECIES ) break;

		path = ptr;
	}

	return normpath;
}

// Converts a hexadecimal string to integer
// code from http://devpinoy.org/blogs/cvega/archive/2006/06/19/xtoi-hex-to-integer-c-function.aspx
int xtoi( const char* xs, unsigned int* result ) {
	size_t szlen = strlen(xs);
	int i, xv, fact;

	if (szlen > 0)
	{
	 // Converting more than 32bit hexadecimal value?
	 if (szlen>8) return 2; // exit

	 // Begin conversion here
	 *result = 0;
	 fact = 1;

	 // Run until no more character to convert
	 for(i=szlen-1; i>=0 ;i--)
	 {
	  if (isxdigit(*(xs+i)))
	  {
		if (*(xs+i)>=97)
		{
		 xv = ( *(xs+i) - 97) + 10;
		}
		else if ( *(xs+i) >= 65)
		{
		 xv = (*(xs+i) - 65) + 10;
		}
		else
		{
		 xv = *(xs+i) - 48;
		}
		*result += (xv * fact);
		fact *= 16;
	   }
	   else
	   {
		// Conversion was abnormally terminated
		// by non hexadecimal digit, hence
		// returning only the converted with
		// an error value 4 (illegal hex character)
		return 4;
	   }
	  }
	 }

	 // Nothing to convert
	 return 1;
}

char* expand_tilde_with( char* path, char* buffer, int size, char* home ) {
	*buffer = 0;
	if ( *path != '~' ) {
		if ( strlen( path ) > size ) return NULL;
		strcpy( buffer, path );
		return buffer;
	}

	if ( strlen( home ) + strlen( path + 1 ) > size ) return NULL;

	strcat( buffer, home );
	strcat( buffer, path + 1 );

	return buffer;
}

char* expand_tilde( char* path, char* buffer, int size, uid_t uid ) {
	*buffer = 0;
	if ( *path != '~' ) {
		if ( strlen( path ) > size ) return NULL;
		strcpy( buffer, path );
		return buffer;
	}

	struct passwd* pwd;
	pwd = getpwuid( uid );
	char* home = pwd->pw_dir;

	if ( strlen( home ) + strlen( path + 1 ) > size ) return NULL;

	strcat( buffer, home );
	strcat( buffer, path + 1 );

	return buffer;
}

bool stopproc( pid_t pid ) {
	if ( kill( pid, SIGTERM ) != 0 ) { // process not running
		return false;
	}

	int time = 0;
	while ( kill( pid, 0 ) != -1 && ( time += 250000 ) <= 3 * 1000000 )
		usleep( time );

	if ( kill( pid, 0 ) != -1 ) { // process still running, not dead
		errno = EBUSY;
		return false;
	}

	if ( errno != ESRCH ) {// failed on error other than nonexistant pid
		return false;
	}

	return true;
}

bool killproc( pid_t pid ) {

	if ( kill( pid, SIGKILL ) != 0 ) { // process not running
		return false;
	}


	int time = 0;
	while ( kill( pid, 0 ) != -1 && ( time += 250000 ) <= 3 * 1000000 )
		usleep( time );

	if ( kill( pid, 0 ) != -1 ) { // process still running, not dead
		errno = EBUSY;
		return false;
	}

	if ( errno != ESRCH ) {// failed on error other than nonexistant pid
		return false;
	}

	return true;
}

char* strnstr( const char* s, const char* find, size_t slen) {

	int success = 0, len = strlen( find );

	while ( slen >= len && *s != '\0' && ( success = !strncmp( s, find, len ) ) != 1 ) {
		slen--;
		s++;
	}

	if ( !success )
		return NULL;

	return (char*) s;
}

char* strtolower( char* s ) {
	char* ptr = s;
	while( *ptr ) {
		*ptr = tolower( *ptr );
		ptr++;
	}
	return s;
}

void* memdup( const void* src, size_t len ) {
	void* dst = malloc( len );
	if ( dst ) memcpy( dst, src, len );
	return dst;
}

char* chomp(char *s) {
	char* p = s;
	while(*p && *p != '\n' && *p != '\r') p++;

	*p = 0;
	return s;
}


/*
bool mkdir_p(const char *dir) {

	char tmp[256];
	char *p = NULL;
	size_t len;

	snprintf(tmp, sizeof(tmp),"%s",dir);
	len = strlen(tmp);

	if(tmp[len - 1] == '/')
		tmp[len - 1] = 0;

	for(p = tmp + 1; *p; p++)
		if(*p == '/') {
			*p = 0;
			if ( mkdir(tmp, S_IRWXU) ) return false;
			*p = '/';
		}
	if ( mkdir(tmp, S_IRWXU) ) return false;

}*/
