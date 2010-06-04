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

#include "cli.h"

int cli_to_string( char* commands[ 50 ][ 2 ], char* top_cmd, cli_cmd_t* command, int* n ) {
	int max_len;
	char* tmp = (char*) malloc( strlen( top_cmd ) + strlen( command->name ) + 2 );
	sprintf( tmp, "%s %s", top_cmd, command->name );
	top_cmd = tmp;

	if ( !command->func ) {
		(*n)--;
	}

	else {
		commands[ *n ][ 0 ] = top_cmd;
		commands[ *n ][ 1 ] = command->description;
	}

	max_len = strlen( top_cmd );
	if ( command->sub_commands ) {
		int i = 0, tmp_max_len;
		cli_cmd_t* tmp;
		while ( ( tmp = command->sub_commands + i )->name ) {
			(*n)++;
			tmp_max_len = cli_to_string( commands, top_cmd, tmp, n );
			if ( tmp_max_len > max_len ) max_len = tmp_max_len;
			i += sizeof( cli_cmd_t );
		}
	}

	if ( !command->func )
		free( top_cmd );

	return max_len;
}

void cli_print( cli_cmd_t* root_cmd ) {
	int n = 0;
	char* commands[ 50 ][ 2 ];
	int len = cli_to_string( commands, "", root_cmd, &n );

	int i;
	for ( i = 0; i <= n; i++ ) {
		char buf[ 16 ];
		sprintf( buf, " %%-%ds %%s\n", len );
		printf( buf, commands[ i ][ 0 ], commands[ i ][ 1 ] );
		free( commands[ i ][ 0 ] );
	}
}

bool cli_run( cli_cmd_t* command, int argc, char* argv[ ] ) {
	if ( strcmp( command->name, argv[ 0 ] ) == 0 ) {
		if ( command->sub_commands && argc - 1 ) {
			int i = 0;
			cli_cmd_t* tmp;
			while ( ( tmp = command->sub_commands + i )->name ) {
				if ( cli_run( tmp, argc - 1, argv + 1 ) )
					return true;
				i += sizeof( cli_cmd_t );
			}
		}

		if ( command->func && argc ) {
			command->func( argc - 1, argv + 1 );
			return true;
		}
	}

	return false;
}
