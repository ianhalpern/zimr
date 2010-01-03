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

#include "userdir.h"

static bool checkdir( const char* path ) {

	struct stat dir_stat;
	if ( stat( path, &dir_stat ) == 0 ) {
		if ( !S_ISDIR( dir_stat.st_mode ) ) {
			return false;
		}
	} else if ( mkdir( path, S_IRWXU ) != 0 )
		return false;

	return true;
}

bool userdir_init( int uid ) {
	char buf[ 256 ];

	expand_tilde( ZM_USR_DIR, buf, sizeof( buf ), uid );
	if ( !checkdir( buf ) ) return false;

	expand_tilde( ZM_USR_MODULE_DIR, buf, sizeof( buf ), uid );
	if ( !checkdir( buf ) ) return false;

	return true;
}
