/*   Poroda - Next Generation Web Server
 *
 *+  Copyright (c) 2009 Ian Halpern
 *@  http://Poroda.org
 *
 *   This file is part of Poroda.
 *
 *   Poroda is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Poroda is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Poroda.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#include "pcnf.h"

int pcnf_walk( yaml_document_t* document, int index, int depth ) {
	int i;
	yaml_node_t *node = yaml_document_get_node( document, index );

	if ( ! node )
		return 0;

	switch ( node->type ) {
		case YAML_SCALAR_NODE:
			printf( "%d: %s\n", depth, node->data.scalar.value );
			break;
		case YAML_SEQUENCE_NODE:
			depth++;
			for ( i = 0; i < node->data.sequence.items.top - node->data.sequence.items.start; i++ ) {
				pcnf_walk( document, node->data.sequence.items.start[ i ], depth );
			}
			break;
		case YAML_MAPPING_NODE:
			depth++;
			for ( i = 0; i < node->data.mapping.pairs.top - node->data.mapping.pairs.start; i++ ) {
				pcnf_walk( document, node->data.mapping.pairs.start[ i ].key, depth );
				pcnf_walk( document, node->data.mapping.pairs.start[ i ].value, depth );
			}
			break;
		default:
			return 0;
	}

	return 1;
}

bool pcnf_load( yaml_document_t* document, const char* filepath ) {

	FILE* cnf_file = fopen( filepath, "rb" );
	if ( ! cnf_file )
		return false;

	yaml_parser_t parser;
	if ( ! yaml_parser_initialize( &parser ) ) {
		fclose( cnf_file );
		return false;
	}

	yaml_parser_set_input_file( &parser, cnf_file );

	if ( ! yaml_parser_load( &parser, document ) ) {
		yaml_parser_delete( &parser );
		fclose( cnf_file );
		return false;
	}

	// we don't need the parser or file anymore
	yaml_parser_delete( &parser );
	fclose( cnf_file );

	return true;
}

bool pcnf_load_websites( pcnf_app_t* cnf, yaml_document_t* document, int index ) {
	yaml_node_t* root = yaml_document_get_node( document, index );
	int i, j;

	if ( ! root || root->type != YAML_SEQUENCE_NODE )
		return 0;

	for ( i = 0; i < root->data.sequence.items.top - root->data.sequence.items.start; i++ ) {
		yaml_node_t* website_node = yaml_document_get_node( document, root->data.sequence.items.start[ i ] );

		if ( !website_node || website_node->type != YAML_MAPPING_NODE )
			return 0;

		pcnf_website_t* website = (pcnf_website_t*) malloc( sizeof( pcnf_website_t ) );
		website->url = NULL;
		website->pubdir = NULL;

		for ( j = 0; j < website_node->data.mapping.pairs.top - website_node->data.mapping.pairs.start; j++ ) {
			yaml_node_t* attr_key = yaml_document_get_node( document, website_node->data.mapping.pairs.start[ j ].key );

			if ( !attr_key || attr_key->type != YAML_SCALAR_NODE )
				break;

			yaml_node_t* attr_val = yaml_document_get_node( document, website_node->data.mapping.pairs.start[ j ].value );
			if ( ! attr_val )
				break;

			// url
			if ( strcmp( "url", (char*) attr_key->data.scalar.value ) == 0 ) {
				if ( attr_val->type != YAML_SCALAR_NODE )
					break;
				website->url = strdup( (char*) attr_val->data.scalar.value );
			}

			// public directory
			else if ( strcmp( "public directory", (char*) attr_key->data.scalar.value ) == 0 ) {
				if ( attr_val->type != YAML_SCALAR_NODE )
					break;
				website->pubdir = strdup( (char*) attr_val->data.scalar.value );
			}

		}

		website->next = cnf->website_node;
		cnf->website_node = website;
	}

	return 1;
}

bool pcnf_state_load_apps( pcnf_state_t* state, yaml_document_t* document, int index ) {
	yaml_node_t* root = yaml_document_get_node( document, index );
	int i, j;

	if ( ! root || root->type != YAML_SEQUENCE_NODE )
		return false;

	for ( i = 0; i < root->data.sequence.items.top - root->data.sequence.items.start; i++ ) {
		yaml_node_t* app_node = yaml_document_get_node( document, root->data.sequence.items.start[ i ] );

		if ( !app_node || app_node->type != YAML_MAPPING_NODE )
			return 0;

		pcnf_state_app_t* app = (pcnf_state_app_t*) malloc( sizeof( pcnf_state_app_t ) );
		list_append( &state->apps, app );
		app->exec = NULL;
		app->cwd = NULL;
		app->pid = 0;

		for ( j = 0; j < app_node->data.mapping.pairs.top - app_node->data.mapping.pairs.start; j++ ) {
			yaml_node_t* attr_key = yaml_document_get_node( document, app_node->data.mapping.pairs.start[ j ].key );

			if ( !attr_key || attr_key->type != YAML_SCALAR_NODE )
				continue;

			yaml_node_t* attr_val = yaml_document_get_node( document, app_node->data.mapping.pairs.start[ j ].value );
			if ( ! attr_val )
				continue;

			// exec
			if ( strcmp( "exec", (char*) attr_key->data.scalar.value ) == 0 ) {
				if ( attr_val->type != YAML_SCALAR_NODE )
					continue;
				app->exec = strdup( (char*) attr_val->data.scalar.value );
			}

			// cwd
			else if ( strcmp( "cwd", (char*) attr_key->data.scalar.value ) == 0 ) {
				if ( attr_val->type != YAML_SCALAR_NODE )
					continue;
				app->cwd = strdup( (char*) attr_val->data.scalar.value );
			}

			// pid
			else if ( strcmp( "pid", (char*) attr_key->data.scalar.value ) == 0 ) {
				if ( attr_val->type != YAML_SCALAR_NODE )
					continue;
				app->pid = atoi( (char*) attr_val->data.scalar.value );
			}

		}

	}

	return 1;
}

pcnf_state_t* pcnf_state_load( ) {
	char filepath[ 128 ];
	pcnf_state_t* state = NULL;
	yaml_document_t document;
	int i;

	if ( !expand_tilde( PD_USR_STATE_FILE, filepath, sizeof( filepath ) ) )
		return NULL;

	// create the file if it doesn't exist.
	FILE* state_file = fopen( filepath, "r" );
	if ( !state_file ) {
		state_file = fopen( filepath, "w" );
		if ( !state_file ) return NULL;
	}
	fclose( state_file );


	if ( !pcnf_load( &document, filepath ) ) {
		return NULL;
	}

	state = (pcnf_state_t*) malloc( sizeof( pcnf_state_t ) );
	list_init( &state->apps );

	yaml_node_t* root = yaml_document_get_root_node( &document );
	if ( !root || root->type != YAML_MAPPING_NODE ) {
		goto quit;
	}

	for ( i = 0; i < root->data.mapping.pairs.top - root->data.mapping.pairs.start; i++ ) {
		yaml_node_t* node = yaml_document_get_node( &document, root->data.mapping.pairs.start[ i ].key );

		if ( ! node || node->type != YAML_SCALAR_NODE ) {
			break;
		}

		if ( strcmp( "applications", (char*) node->data.scalar.value ) == 0 ) {
			if ( ! pcnf_state_load_apps( state, &document, root->data.mapping.pairs.start[ i ].value ) ) {
				break;
			}
		}
	}

quit:
	yaml_document_delete( &document );
	return state;
}

pcnf_app_t* pcnf_app_load( ) {
	char filepath[ 128 ];
	pcnf_app_t* cnf = NULL;
	yaml_document_t document;
	int i;

	if ( !expand_tilde( PD_APP_CNF_FILE, filepath, sizeof( filepath ) ) )
		return NULL;

	if ( !pcnf_load( &document, filepath ) )
		return NULL;

	// start parsing config settings
	yaml_node_t* root = yaml_document_get_node( &document, 1 );

	if ( ! root || root->type != YAML_MAPPING_NODE ) {
		goto quit;
	}

	cnf = (pcnf_app_t*) malloc( sizeof( pcnf_app_t ) );
	memset( cnf, 0, sizeof( pcnf_app_t ) );
	for ( i = 0; i < root->data.mapping.pairs.top - root->data.mapping.pairs.start; i++ ) {
		yaml_node_t* node = yaml_document_get_node( &document, root->data.mapping.pairs.start[ i ].key );

		if ( ! node || node->type != YAML_SCALAR_NODE ) {
			pcnf_app_free( cnf );
			cnf = NULL;
			break;
		}

		if ( strcmp( "websites", (char*) node->data.scalar.value ) == 0 ) {
			if ( ! pcnf_load_websites( cnf, &document, root->data.mapping.pairs.start[ i ].value ) ) {
				pcnf_app_free( cnf );
				cnf = NULL;
				break;
			}
		}
	}

quit:
	yaml_document_delete( &document );
	return cnf;
}

void pcnf_state_set_app( pcnf_state_t* state, const char* exec, const char* cwd, pid_t pid ) {
	pcnf_state_app_t* app;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		if ( strcmp( app->exec, exec ) == 0 && strcmp( app->cwd, cwd ) == 0 ) {
			app->pid = pid;
			return;
		}
	}

	app = (pcnf_state_app_t*) malloc( sizeof( pcnf_state_app_t ) );
	app->exec = strdup( exec );
	app->cwd  = strdup( cwd );
	app->pid  = pid;

	list_append( &state->apps, app );
}

bool pcnf_state_app_is_running( pcnf_state_t* state, const char* exec, const char* cwd ) {
	pcnf_state_app_t* app;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		if ( strcmp( app->exec, exec ) == 0 && strcmp( app->cwd, cwd ) == 0 ) {
			if ( !app->pid ) return false;
			return true;
		}
	}

	return false;
}

void pcnf_state_save( pcnf_state_t* state ) {
	char filepath[ 128 ];
	yaml_document_t document;
	int root, apps, name;
	char number[64];

	if ( !expand_tilde( PD_USR_STATE_FILE, filepath, sizeof( filepath ) ) )
		return;

	memset( &document, 0, sizeof( yaml_document_t ) );
	assert( yaml_document_initialize( &document, NULL, NULL, NULL, 0, 0 ) );

	assert( ( root = yaml_document_add_mapping( &document, NULL, YAML_BLOCK_SEQUENCE_STYLE ) ) );
	assert( ( apps = yaml_document_add_sequence( &document, NULL, YAML_BLOCK_MAPPING_STYLE ) ) );
	assert( ( name = yaml_document_add_scalar( &document, NULL, (unsigned char*) "applications", 12, YAML_PLAIN_SCALAR_STYLE ) ) );

	assert( yaml_document_append_mapping_pair( &document, root, name, apps ) );

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		pcnf_state_app_t* app = list_get_at( &state->apps, i );
		int app_map, exec_name, exec_value, cwd_name, cwd_value, pid_name, pid_value;

		assert( ( exec_name = yaml_document_add_scalar( &document, NULL,
		  (unsigned char*) "exec", 4, YAML_PLAIN_SCALAR_STYLE ) ) );
		assert( ( exec_value = yaml_document_add_scalar( &document, NULL,
		  (unsigned char*) app->exec, strlen( app->exec ), YAML_PLAIN_SCALAR_STYLE ) ) );

		assert( ( cwd_name = yaml_document_add_scalar( &document, NULL,
		  (unsigned char*) "cwd", 3, YAML_PLAIN_SCALAR_STYLE ) ) );
		assert( ( cwd_value = yaml_document_add_scalar( &document, NULL,
		  (unsigned char*) app->cwd, strlen( app->cwd ), YAML_PLAIN_SCALAR_STYLE ) ) );

		sprintf( number, "%d", app->pid );
		assert( ( pid_name = yaml_document_add_scalar( &document, NULL,
		  (unsigned char*) "pid", 3, YAML_PLAIN_SCALAR_STYLE ) ) );
		assert( ( pid_value = yaml_document_add_scalar( &document, NULL,
		  (unsigned char*) number, -1, YAML_PLAIN_SCALAR_STYLE ) ) );

		assert( ( app_map = yaml_document_add_mapping( &document, NULL, YAML_BLOCK_SEQUENCE_STYLE ) ) );
		assert( yaml_document_append_sequence_item( &document, apps, app_map ) );

		assert( yaml_document_append_mapping_pair( &document, app_map, exec_name, exec_value ) );
		assert( yaml_document_append_mapping_pair( &document, app_map, cwd_name, cwd_value ) );
		assert( yaml_document_append_mapping_pair( &document, app_map, pid_name, pid_value ) );

	}

	// save document
	yaml_emitter_t emitter;
	FILE* state_file = fopen( filepath, "wb" );

	assert( yaml_emitter_initialize( &emitter ) );
	yaml_emitter_set_output_file( &emitter, state_file );
//	yaml_emitter_set_canonical(&emitter, canonical);
//	yaml_emitter_set_unicode(&emitter, unicode);
	assert( yaml_emitter_open( &emitter ) );
	yaml_emitter_dump( &emitter, &document );

	yaml_emitter_close( &emitter );
	yaml_emitter_delete( &emitter );
	yaml_document_delete( &document );
	fclose( state_file );
}

void pcnf_app_free( pcnf_app_t* cnf ) {
	free( cnf );
}

void pcnf_state_app_free( pcnf_state_app_t* app ) {
	free( app->exec );
	free( app->cwd );
	free( app );
}

void pcnf_state_free( pcnf_state_t* state ) {
	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		pcnf_state_app_t* app = list_get_at( &state->apps, i );
		pcnf_state_app_free( app );
	}
	list_destroy( &state->apps );
	free( state );
}
