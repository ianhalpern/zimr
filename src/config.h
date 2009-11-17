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

#ifndef _ZM_CONFIG_H
#define _ZM_CONFIG_H

#define BUILD_DATE __DATE__ " " __TIME__

#define ZIMR_WEBSITE "http://Zimr.org"

#define FL_ISSET( flags, flag ) ( (flags) & (flag) )
#define FL_SET( flags, flag ) ( (flags) |= (flag) )
#define FL_CLR( flags, flag ) ( (flags) ^= (flags)&(flag) )

#define CLR 0
#define SET 1

#define SMALLEST( X, Y ) ( (X)<=(Y) ? (X) : (Y) )

#define D_LOCKFILE_PATH "/tmp/zimr-proxy.pid" // used by daemon.c
#define ZM_APP_CNF_FILE "zimr.cnf"
#define ZM_USR_STATE_FILE "~/.zimr.state"
#define ZM_REQ_LOGFILE "zimr.log"
#define ZM_APP_EXEC "zimr-app"

#define ZM_PROXY_ADDR "127.0.0.1"
#define ZM_PROXY_PORT 7672

#define ZM_NUM_PROXY_DEATH_RETRIES 1000 // set to 0(ZERO) for infinite retries
#define ZM_PROXY_DEATH_RETRY_DELAY 2

#define ZM_MODULE_NAME_MAX_LEN 100

/******* ZM_CMD's *********/

// Commands MUST be NEGATIVE
#define ZM_CMD_WS_START  -1
#define ZM_CMD_WS_STOP   -2
#define ZM_CMD_STATUS    -3

/**************************/

/****** ztransport config *******/

#define ZT_MSG_SIZE 4084

/**************************/

/****** zsocket config ********/

#define ZSOCK_N_PENDING 25

/*************************/

/****** http config *******/

#define HTTP_POST "POST"
#define HTTP_GET  "GET"

#define HTTP_GET_TYPE  0x01
#define HTTP_POST_TYPE 0x02

#define HTTP_TYPE(X) ( (X) == HTTP_GET_TYPE ? HTTP_GET : HTTP_POST )

#define HTTP_HDR_ENDL "\r\n"

#define HTTP_DEFAULT_PORT  80

/*************************/

/******* headers config *******/

#define HEADERS_MAX_NUM 32
#define HEADER_NAME_MAX_LEN  128
#define HEADER_VALUE_MAX_LEN 512

/*************************/

/******* cookies config *******/

#define COOKIES_MAX_NUM        32
#define COOKIE_NAME_MAX_LEN    128
#define COOKIE_VALUE_MAX_LEN   512
#define COOKIE_DOMAIN_MAX_LEN  128
#define COOKIE_PATH_MAX_LEN    256
#define COOKIE_EXPIRES_MAX_LEN 30

/*************************/

#endif
