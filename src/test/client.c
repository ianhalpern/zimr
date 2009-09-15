#include "zfildes.h"
#include "zsocket.h"
#include "msg_switch.h"
#include "config.h"

#define INREAD 0x5
#define INWRIT 0x6

ztswitcher_t* switcher;

void inread( int sockfd, void* udata ) {
	int type;
	read( sockfd, &type, sizeof( type ) );

	ztresp_t res;
	ztpacket_t packet;;
	int n;
	switch( type ) {
		case ZTTYPE_RESP:
			printf( "got response\n" );
			n = read( sockfd, &res, sizeof( res ) );
			ztswitcher_acceptres( switcher, res );
			break;
		case ZTTYPE_PACK:
			printf( "read packet\n" );
			read( sockfd, &packet, sizeof( packet ) );
			//write( fileno( stdout ), packet.message, packet.size );
			ztswitcher_sendresp( switcher, packet.msgid, ZTRES_OK );
			break;
	}
	if ( ztswitcher_pending_write( switcher ) ) {
		zfd_set( sockfd, INWRIT, NULL );
	}
}

void inwrit( int sockfd, void* udata ) {
	int type;
	int size;
	void* data;
	ztresp_t res;
	ztpacket_t packet;

	// check if waiting to send returns
	if ( ztswitcher_has_resps( switcher ) ) {
		printf( "sending read response\n" );
		type = ZTTYPE_RESP;
		res = ztswitcher_getresp( switcher );
		size = sizeof( res );
		data = &res;
	}

	// pop from queue and write
	else {
		printf( "writing data\n" );
		type = ZTTYPE_PACK;
		packet = ztswitcher_pop( switcher );
		size = sizeof( packet );
		data = &packet;
	}

	write( sockfd, &type, sizeof( type ) );
	write( sockfd, data, size );

	if ( !ztswitcher_pending_write( switcher ) ) {
		printf( "done writing\n" );
		zfd_clr( sockfd, INWRIT );
	}
}

int main( int argc, char* argv[ ] ) {
	zsocket_init( );

	zfd_register_type( INREAD, ZFD_R, ZFD_TYPE_HDLR inread );
	zfd_register_type( INWRIT, ZFD_W, ZFD_TYPE_HDLR inwrit );

	int sockfd = zsocket( inet_addr( ZM_PROXY_ADDR ), ZM_PROXY_PORT + 1, ZSOCK_CONNECT );

	switcher = ztswitcher_new( );
	zfd_set( sockfd, INREAD, NULL );

	while( zfd_select( 0 ) );

	return 0;
}
