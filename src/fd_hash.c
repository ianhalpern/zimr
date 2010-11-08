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

#include "fd_hash.h"

void fd_hash_init( fd_hash_t* hash ) {
	memset( hash, 0, sizeof( fd_hash_t ) );
}

void fd_hash_add( fd_hash_t* hash, int fd ) {
	if ( hash->table[fd].next || hash->table[fd].previous || &hash->table[fd] == hash->head )
		return; // Already in the list, return

	if ( hash->tail != NULL ) {
		hash->table[fd].previous = hash->tail;
		hash->tail->next = &hash->table[fd];
	}
	hash->tail = &hash->table[fd];

	if ( hash->head == NULL )
		hash->head = hash->tail;
}

void fd_hash_remove( fd_hash_t* hash, int fd ) {
	if ( !hash->table[fd].next && !hash->table[fd].previous && &hash->table[fd] != hash->head )
		return; // Not in the list, return

	if ( hash->table[fd].previous )
		hash->table[fd].previous->next = hash->table[fd].next;

	if ( hash->table[fd].next )
		hash->table[fd].next->previous = hash->table[fd].previous;

	if ( &hash->table[fd] == hash->head )
		hash->head = hash->table[fd].next;

	if ( &hash->table[fd] == hash->tail )
		hash->tail = hash->table[fd].previous;

	hash->table[fd].next = NULL;
	hash->table[fd].previous = NULL;
}

int fd_hash_head( fd_hash_t* hash ) {
	if ( !hash->head ) return -1;
	return hash->head - &hash->table[0];
}

int fd_hash_next( fd_hash_t* hash, int fd ) {
	if ( !hash->table[fd].next ) return -1;
	return hash->table[fd].next - &hash->table[0];
}
