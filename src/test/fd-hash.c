#include "fd_hash.h"

int main( int argc, char* argv[] ) {
	fd_hash_t hash;
	fd_hash_init( &hash );

	// add/remove one element
	fd_hash_add( &hash, 1 );
	fd_hash_remove( &hash, 1 );

	// add two elements, remove first
	fd_hash_add( &hash, 0 );
	fd_hash_add( &hash, 2 );
	fd_hash_remove( &hash, 0 );

	// add trailing element, remove
	fd_hash_add( &hash, 3 );
	fd_hash_remove( &hash, 3 );

	// add elements, remove from middle 
	fd_hash_add( &hash, 4 );
	fd_hash_add( &hash, 5 );
	fd_hash_add( &hash, 6 );
	fd_hash_remove( &hash, 4 );

	// add exists elements at different positions
	fd_hash_add( &hash, 6 );
	fd_hash_add( &hash, 2 );
	fd_hash_add( &hash, 5 );

	int fd = fd_hash_head( &hash );
	while ( fd != -1 ) {
		printf( "node: %d\n", fd );
		fd = fd_hash_next( &hash, fd );
	}
	return EXIT_SUCCESS;
}
