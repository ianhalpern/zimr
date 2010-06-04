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

#ifndef _ZM_CLI_H
#define _ZM_CLI_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct command {
	char* name;
	char* description;
	void* sub_commands;
	void (*func)( int optc, char* optv[ ] );
} cli_cmd_t;

int cli_to_string( char* commands[ 50 ][ 2 ], char* top_cmd, cli_cmd_t* command, int* n );
void cli_print( cli_cmd_t* root_cmd );
bool cli_run( cli_cmd_t* command, int argc, char* argv[] );

#endif
