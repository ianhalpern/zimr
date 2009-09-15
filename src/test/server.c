#include "zfildes.h"
#include "zsocket.h"
#include "msg_switch.h"
#include "simclist.h"
#include "general.h"

#define EXLISN 0x1
#define EXREAD 0x2
#define EXWRIT 0x3

#define INLISN 0x4
#define INREAD 0x5
#define INWRIT 0x6

int clientfd;
ztswitcher_t* switcher;

void inlisn( int sockfd, void* udata ) {
	struct sockaddr_in cli_addr;
	unsigned int cli_len = sizeof( cli_addr );

	// Connection request on original socket.
	if ( ( clientfd = accept( sockfd, (struct sockaddr*) &cli_addr, &cli_len ) ) < 0 ) {
		return;
	}

	switcher = ztswitcher_new( );
	zfd_set( clientfd, INREAD, NULL );
}

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
			zfd_reset( res.msgid, EXREAD );
			break;
		case ZTTYPE_PACK:
			printf( "read packet\n" );
			read( sockfd, &packet, sizeof( packet ) );
			zfd_set( packet.msgid, EXWRIT, memdup( &packet, sizeof( packet ) ) );
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

void exlisn( int sockfd, void* udata ) {
	struct sockaddr_in cli_addr;
	unsigned int cli_len = sizeof( cli_addr );
	int newsockfd;

	// Connection request on original socket.
	if ( ( newsockfd = accept( sockfd, (struct sockaddr*) &cli_addr, &cli_len ) ) < 0 ) {
		return;
	}

	ztswitcher_msg_new( switcher, newsockfd );
	zfd_set( newsockfd, EXREAD, NULL );
}

void exread( int sockfd, void* udata ) {
	char buf[ 2048 ];
	int n = read( sockfd, buf, sizeof( buf ) );

	// push onto queue for sockfd
	ztswitcher_push( switcher, sockfd, buf, n );

	printf( "read %d bytes\n", n );

	// something to be written?
	if ( ztswitcher_msg_is_ready( switcher, sockfd ) ) {
		printf( "ready to write\n" );
		// add write to zfd and remove read
		zfd_set( clientfd, INWRIT, NULL );
		zfd_clr( sockfd, EXREAD );
	}
}

void exwrit( int sockfd, ztpacket_t* packet ) {
	// pop from queue and write, remove if none to write left
	write( sockfd, packet->message, packet->size );
	ztswitcher_sendresp( switcher, sockfd, ZTRES_OK );
	zfd_clr( sockfd, EXWRIT );
}

int main( int argc, char* argv[ ] ) {
	zsocket_init( );

	zfd_register_type( INLISN, ZFD_R, ZFD_TYPE_HDLR inlisn );
	zfd_register_type( INREAD, ZFD_R, ZFD_TYPE_HDLR inread );
	zfd_register_type( INWRIT, ZFD_W, ZFD_TYPE_HDLR inwrit );

	zfd_register_type( EXLISN, ZFD_R, ZFD_TYPE_HDLR exlisn );
	zfd_register_type( EXREAD, ZFD_R, ZFD_TYPE_HDLR exread );
	zfd_register_type( EXWRIT, ZFD_W, ZFD_TYPE_HDLR exwrit );

	int insockfd = zsocket( inet_addr( ZM_PROXY_ADDR ), ZM_PROXY_PORT + 1, ZSOCK_LISTEN );

	int exsockfd = zsocket( inet_addr( ZM_PROXY_ADDR ), 8080, ZSOCK_LISTEN );

	zfd_set( insockfd, INLISN, NULL );
	zfd_set( exsockfd, EXLISN, NULL );

	while( zfd_select( 0 ) );

	return 0;
}
