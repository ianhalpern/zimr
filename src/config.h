/*   Podora - Next Generation Web Server
 *
 *+  Copyright (c) 2009 Ian Halpern
 *@  http://Podora.org
 *
 *   This file is part of Podora.
 *
 *   Podora is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Podora is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Podora.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef _PD_CONFIG_H
#define _PD_CONFIG_H

#define FLAG_ISSET( flag, flags ) ( (flag) & (flags) )

#define D_LOCKFILE_PATH "/tmp/podorapd.pid" // used by daemon.c
#define PD_WS_CONF_FILE "podora.cnf"

#define PROXY_ADDR "127.0.0.1"
#define PROXY_PORT 8888

/****** ptransmission config *******/

#define PT_MSG_SIZE ( PT_BUF_SIZE - PT_HDR_SIZE )
#define PT_HDR_SIZE ( sizeof( ptransport_header_t ) )
#define PT_BUF_SIZE 4096

/**************************/

/****** psocket config ********/

#define PSOCK_N_PENDING 25

/*************************/

/****** http config *******/

#define HTTP_POST "POST"
#define HTTP_GET  "GET"

#define HTTP_GET_TYPE  0x01
#define HTTP_POST_TYPE 0x02

#define HTTP_HDR_ENDL "\r\n"

#define HTTP_DEFAULT_PORT  80

/*************************/

/******* headers config *******/

#define HEADERS_MAX_NUM 32

#define HEADER_NAME_MAX_LEN  128
#define HEADER_VALUE_MAX_LEN 512

/*************************/

/******* params config *******/

#define PARAMS_MAX_NUM 32

#define PARAM_NAME_MAX_LEN  128
#define PARAM_VALUE_MAX_LEN 512

/*************************/

#endif
