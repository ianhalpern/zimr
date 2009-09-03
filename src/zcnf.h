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

TODO: EXAMPLE OF WHATS TO COME:

# zimr.cnf example

proxy: "127.0.0.1:8080"

websites:
- url: [ welive.net, www.welive.net ]
  ip: [ default, "10.10.1.0" ]
  public directory: welive.net/

- url: zimr.org
  www-redirect
  public directory: zimr.org/

- url: ian-halpern.com
  proxy: [ default, local-default, "127.0.1.1:1245" ]
  public directory: ian-halpern.com/

- url: impulse.ian-halpern.com
  public directory: impulse.ian-halpern.com/

- url: pookieweatherburn.com
  public directory: pookieweatherburn.com/


 */

#ifndef _ZM_PCNF_H
#define _ZM_PCNF_H

#include <stdbool.h>
#include <yaml.h>
#include <assert.h>
#include <sys/stat.h>

#include "simclist.h"
#include "general.h"

// Website Config structs
typedef struct zcnf_website {
	char* url;
	char* pubdir;
	char* redirect_url;
	struct zcnf_website* next;
} zcnf_website_t;

typedef struct {
	zcnf_website_t* website_node;
} zcnf_app_t;
///////////////////////////

// Proxy State structs
typedef struct zcnf_state_app {
	char* exec;
	char* dir;
	list_t args;
	pid_t pid;
} zcnf_state_app_t;

typedef struct {
	list_t apps;
	uid_t uid;
} zcnf_state_t;
///////////////////////////

zcnf_state_t* zcnf_state_load( uid_t uid );
zcnf_app_t* zcnf_app_load( char* path );

bool zcnf_state_app_is_running( zcnf_state_t* state, const char* exec, const char* cwd );
void zcnf_state_set_app( zcnf_state_t* state, const char* exec, const char* cwd, pid_t pid, list_t* args );
void zcnf_state_save( zcnf_state_t* state );

void zcnf_app_free( zcnf_app_t* cnf );
void zcnf_state_app_free( zcnf_state_app_t* app );
void zcnf_state_free( zcnf_state_t* state );

#endif
