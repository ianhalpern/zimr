#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "simclist.h"

#define MSG_STAT_NEW      0x1
#define MSG_STAT_WRITE    0x2
#define MSG_STAT_READ     0x3
#define MSG_STAT_RECVRESP 0x4

#define PACK_DATA_SIZE 4048

#define PACK_FL_FIRST 0x1
#define PACK_FL_LAST  0x2

#define PACK_RESP_OK 0x1

#define MSG_TYPE_RESP 0x1
#define MSG_TYPE_PACK 0x2

typedef struct {
	int msgid;
	char status;
} msg_packet_resp_t;

typedef struct {
	int msgid;
	int size;
	int flags;
	char data[ PACK_DATA_SIZE ];
} msg_packet_t;

typedef struct {
	int msgid;
	int status;
	list_t queue;
} msg_t;

typedef struct {
	list_t pending_resps;
	list_t pending_msgs;
	msg_t* msgs[ FD_SETSIZE ];
} msg_switch_t;

msg_switch_t* msg_switch_new( );
void msg_new( msg_switch_t* msg_switch, int msgid );
bool msg_switch_has_pending_msgs( msg_switch_t* msg_switch );
bool msg_switch_has_pending_resps( msg_switch_t* msg_switch );
msg_packet_resp_t msg_switch_get_resp( msg_switch_t* msg_switch );
void msg_accept_resp( msg_switch_t* msg_switch, msg_packet_resp_t resp );
void msg_switch_send_resp( msg_switch_t* msg_switch, int msgid, char status );
msg_packet_t msg_switch_pop_packet( msg_switch_t* msg_switch );
bool msg_switch_is_pending( msg_switch_t* msg_switch );
void msg_push_data( msg_switch_t* msg_switch, int msgid, void* data, int size );
bool msg_is_pending( msg_switch_t* msg_switch, int msgid );


