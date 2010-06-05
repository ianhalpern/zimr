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

#include "dlog.h"

int dlog( FILE* f, char* message, ... ) {
	time_t now;
	char now_str[ 80 ];
	int ret;

	time( &now );
	strftime( now_str, 80, "%a %b %d %I:%M:%S %Z %Y: ", localtime( &now ) );
	if ( fwrite( now_str, strlen( now_str ), 1, f ) == -1 )
		return -1;

	va_list ap;
	va_start( ap, message );
	ret = vfprintf( f, message, ap );
	va_end( ap );

	fwrite( "\n", 1, 1, f );
	fflush( f );
	return ret;
}
