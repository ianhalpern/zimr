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

#include "trace.h"

void trace( char* message, ... ) {
#ifdef DEBUG_TRACE_ENABLED
	struct timeval tv;
	time_t now;
	char now_str[ 80 ];
	int ret;

	gettimeofday( &tv, NULL );
	now = tv.tv_sec;
	strftime( now_str, 80, "%T.", localtime( &now ) );
	printf( "%s%ld: ", now_str, tv.tv_usec );

	va_list ap;
	va_start( ap, message );
	ret = vprintf( message, ap );
	va_end( ap );

	puts("");
#endif
}

