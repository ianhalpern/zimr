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

#define PD_INFO_FILE "podora.info"

/****** pcom config *******/

#define PCOM_MSG_SIZE ( PCOM_BUF_SIZE - PCOM_HDR_SIZE )
#define PCOM_HDR_SIZE ( sizeof( pcom_header_t ) )
#define PCOM_BUF_SIZE 4096

#define PCOM_RO 0x0001
#define PCOM_WO 0x0002

#define PCOM_MSG_FIRST 0x0001
#define PCOM_MSG_LAST  0x0002

#define PCOM_MSG_IS_FIRST(X) ( ((pcom_transport_t *)(X))->header->flags & PCOM_MSG_FIRST )
#define PCOM_MSG_IS_LAST(X)  ( ((pcom_transport_t *)(X))->header->flags & PCOM_MSG_LAST  )

#define PCOM_NO_SID 0
#define PCOM_NO_TID 0

/**************************/

/****** website Config *******/

#define WS_START_CMD -1
#define WS_STOP_CMD  -2

#define WS_STATUS_STOPPED  0x01
#define WS_STATUS_STARTED  0x02
#define WS_STATUS_STOPPING 0x03

#define WS_TYPE_SERVER 0x01
#define WS_TYPE_CLIENT 0x02

/*************************/

/****** socket config ********/

#define SOCK_N_PENDING 5

/*************************/

/****** http Config *******/

#define HTTP_DEFAULT_PORT  80
#define HTTPS_DEFAULT_PORT 443

/*************************/

#endif
