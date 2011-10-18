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

#include "zcnf.h"

// zcnf_walk is more to show an example of how to walk through a yaml config file
/*int zcnf_walk( yaml_document_t* document, int index, int depth ) {
	int i;
	yaml_node_t *node = yaml_document_get_node( document, index );

	if ( !node )
		return 0;

	switch ( node->type ) {
		case YAML_SCALAR_NODE:
			printf( "%d: %s\n", depth, node->data.scalar.value );
			break;
		case YAML_SEQUENCE_NODE:
			depth++;
			for ( i = 0; i < node->data.sequence.items.top - node->data.sequence.items.start; i++ ) {
				zcnf_walk( document, node->data.sequence.items.start[ i ], depth );
			}
			break;
		case YAML_MAPPING_NODE:
			depth++;
			for ( i = 0; i < node->data.mapping.pairs.top - node->data.mapping.pairs.start; i++ ) {
				zcnf_walk( document, node->data.mapping.pairs.start[ i ].key, depth );
				zcnf_walk( document, node->data.mapping.pairs.start[ i ].value, depth );
			}
			break;
		default:
			return 0;
	}

	return 1;
}*/

void zcnf_parse_proxy( zcnf_proxy_t* proxy, char* proxy_str ) {
	memset( proxy->ip, 0, sizeof( proxy->ip ) );
	char* port = strchr( proxy_str, ':' );
	strncpy(
		proxy->ip, proxy_str,
		!port || strlen( proxy_str ) - strlen( port ) > sizeof( proxy->ip ) - 1
		? sizeof( proxy->ip ) - 1
		: strlen( proxy_str ) - strlen( port )
	);
	proxy->port = port ? atoi( port + 1 ) : ZM_PROXY_DEFAULT_PORT;
}

bool zcnf_load( yaml_document_t* document, const char* filepath ) {

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

bool zcnf_load_websites( zcnf_app_t* cnf, yaml_document_t* document, int index ) {
	yaml_node_t* root = yaml_document_get_node( document, index );
	int i, j, k, l;

	if ( !root || root->type != YAML_SEQUENCE_NODE )
		return 0;

	for ( i = 0; i < root->data.sequence.items.top - root->data.sequence.items.start; i++ ) {
		yaml_node_t* website_node = yaml_document_get_node( document, root->data.sequence.items.start[ i ] );

		if ( !website_node || website_node->type != YAML_MAPPING_NODE )
			return 0;

		zcnf_website_t* website = (zcnf_website_t*) malloc( sizeof( zcnf_website_t ) );
		website->url = NULL;
		website->redirect_url = NULL;
		website->ip_address = NULL;
		list_init( &website->pubdirs );
		list_init( &website->modules );
		list_init( &website->ignore );

		for ( j = 0; j < website_node->data.mapping.pairs.top - website_node->data.mapping.pairs.start; j++ ) {
			yaml_node_t* attr_key = yaml_document_get_node( document, website_node->data.mapping.pairs.start[ j ].key );

			if ( !attr_key || attr_key->type != YAML_SCALAR_NODE )
				break;

			yaml_node_t* attr_val = yaml_document_get_node( document, website_node->data.mapping.pairs.start[ j ].value );
			if ( !attr_val )
				continue;

			// url
			if ( strcmp( "url", (char*) attr_key->data.scalar.value ) == 0 ) {
				if ( attr_val->type != YAML_SCALAR_NODE )
					continue;
				website->url = strdup( (char*) attr_val->data.scalar.value );
			}

			// public directory
			else if ( strcmp( "public directory", (char*) attr_key->data.scalar.value ) == 0 ) {
				if ( attr_val->type != YAML_SCALAR_NODE )
					continue;
				list_append( &website->pubdirs, strdup( (char*) attr_val->data.scalar.value ) );
			}

			// public directories
			else if ( strcmp( "public directories", (char*) attr_key->data.scalar.value ) == 0 ) {
				if ( !attr_val || attr_val->type != YAML_SEQUENCE_NODE )
					continue;

				for ( k = 0; k < attr_val->data.sequence.items.top - attr_val->data.sequence.items.start; k++ ) {
					yaml_node_t* pubdir_node = yaml_document_get_node( document, attr_val->data.sequence.items.start[ k ] );

					if ( pubdir_node->type != YAML_SCALAR_NODE )
						continue;

					list_append( &website->pubdirs, strdup( (char*) pubdir_node->data.scalar.value ) );
				}
			}

			// ip address
			else if ( strcmp( "ip address", (char*) attr_key->data.scalar.value ) == 0 ) {
				if ( attr_val->type != YAML_SCALAR_NODE )
					continue;
				website->ip_address = strdup( (char*) attr_val->data.scalar.value );
			}

			// redirect url
			else if ( strcmp( "redirect to", (char*) attr_key->data.scalar.value ) == 0 ) {
				if ( attr_val->type != YAML_SCALAR_NODE )
					continue;
				website->redirect_url = strdup( (char*) attr_val->data.scalar.value );
			}

			// ignore
			else if ( strcmp( "ignore", (char*) attr_key->data.scalar.value ) == 0 ) {
				if ( !attr_val || attr_val->type != YAML_SEQUENCE_NODE )
					continue;

				for ( k = 0; k < attr_val->data.sequence.items.top - attr_val->data.sequence.items.start; k++ ) {
					yaml_node_t* ignore_node = yaml_document_get_node( document, attr_val->data.sequence.items.start[ k ] );

					if ( ignore_node->type != YAML_SCALAR_NODE )
						continue;

					list_append( &website->ignore, strdup( (char*) ignore_node->data.scalar.value ) );
				}
			}

			// modules
			else if ( strcmp( "modules", (char*) attr_key->data.scalar.value ) == 0 ) {
				if ( !attr_val || attr_val->type != YAML_SEQUENCE_NODE )
					continue;

				for ( k = 0; k < attr_val->data.sequence.items.top - attr_val->data.sequence.items.start; k++ ) {
					yaml_node_t* module_node = yaml_document_get_node( document, attr_val->data.sequence.items.start[ k ] );

					if ( !module_node || module_node->type == YAML_SEQUENCE_NODE )
						continue;

					else if ( module_node->type == YAML_SCALAR_NODE ) {
						zcnf_module_t* module = (zcnf_module_t*) malloc( sizeof( zcnf_module_t ) );
						module->name = strdup( (char*) module_node->data.scalar.value );
						module->argc = 0;
						list_append( &website->modules, module );
					}

					else if ( module_node->type == YAML_MAPPING_NODE ) {
						if ( module_node->data.mapping.pairs.top - module_node->data.mapping.pairs.start != 1 )
							continue;

						yaml_node_t* module_name = yaml_document_get_node( document, module_node->data.mapping.pairs.start[ 0 ].key );
						if ( !module_name || module_name->type != YAML_SCALAR_NODE )
							continue;

						zcnf_module_t* module = (zcnf_module_t*) malloc( sizeof( zcnf_module_t ) );
						module->name = strdup( (char*) module_name->data.scalar.value );
						module->argc = 0;
						list_append( &website->modules, module );

						yaml_node_t* module_args = yaml_document_get_node( document, module_node->data.mapping.pairs.start[ 0 ].value );
						if ( !module_args || module_args->type != YAML_SEQUENCE_NODE )
							continue;

						for ( l = 0; l < module_args->data.sequence.items.top - module_args->data.sequence.items.start; l++ ) {
							yaml_node_t* module_arg = yaml_document_get_node( document, module_args->data.sequence.items.start[ l ] );

							if ( !module_arg || module_arg->type != YAML_SCALAR_NODE )
								continue;

							char arg[ strlen( (char*) module_arg->data.scalar.value ) + 1 ];

							memset( arg, 0, strlen( (char*) module_arg->data.scalar.value ) + 1 );
							strncpy( arg, (char*) module_arg->data.scalar.value,
							  strlen( (char*) module_arg->data.scalar.value ) );
							module->argv[ module->argc++ ] = strdup( arg );

							if ( module->argc == ZM_MODULE_MAX_ARGS )
								break;
						}
					}
				}
			}

		}

		list_append( &cnf->websites, website );
	}

	return 1;
}

bool zcnf_state_load_apps( zcnf_state_t* state, yaml_document_t* document, int index ) {
	yaml_node_t* root = yaml_document_get_node( document, index );
	int i, j;

	if ( ! root || root->type != YAML_SEQUENCE_NODE )
		return false;

	for ( i = 0; i < root->data.sequence.items.top - root->data.sequence.items.start; i++ ) {
		yaml_node_t* app_node = yaml_document_get_node( document, root->data.sequence.items.start[ i ] );

		if ( !app_node || app_node->type != YAML_MAPPING_NODE )
			return 0;

		zcnf_state_app_t* app = (zcnf_state_app_t*) malloc( sizeof( zcnf_state_app_t ) );
		list_append( &state->apps, app );
		app->path = NULL;
		app->pid = 0;
		app->stopped = false;

		for ( j = 0; j < app_node->data.mapping.pairs.top - app_node->data.mapping.pairs.start; j++ ) {
			yaml_node_t* attr_key = yaml_document_get_node( document, app_node->data.mapping.pairs.start[ j ].key );

			if ( !attr_key || attr_key->type != YAML_SCALAR_NODE )
				continue;

			yaml_node_t* attr_val = yaml_document_get_node( document, app_node->data.mapping.pairs.start[ j ].value );
			if ( ! attr_val )
				continue;

			// path
			if ( strcmp( "path", (char*) attr_key->data.scalar.value ) == 0 ) {
				if ( attr_val->type != YAML_SCALAR_NODE ) {
					printf( "Invalid state file: the path attribute's value must be a valid file path.\n" );
					return false;
				}
				app->path = strdup( (char*) attr_val->data.scalar.value );
			}

			// pid
			else if ( strcmp( "pid", (char*) attr_key->data.scalar.value ) == 0 ) {
				if ( attr_val->type != YAML_SCALAR_NODE )
					continue;
				app->pid = atoi( (char*) attr_val->data.scalar.value );
			}

			// stopped
			else if ( strcmp( "stopped", (char*) attr_key->data.scalar.value ) == 0 ) {
				if ( attr_val->type != YAML_SCALAR_NODE )
					continue;
				app->stopped = atoi( (char*) attr_val->data.scalar.value );
			}

		}

	}

	return 1;
}

zcnf_state_t* zcnf_state_load( uid_t uid ) {
	char filepath[ 128 ];
	zcnf_state_t* state = NULL;
	yaml_document_t document;
	int i;

	if ( !expand_tilde( ZM_USR_STATE_FILE, filepath, sizeof( filepath ), uid ) )
		return NULL;

	// create the file if it doesn't exist.
	FILE* state_file = fopen( filepath, "r" );
	if ( !state_file ) {
		state_file = fopen( filepath, "w" );
		if ( !state_file ) return NULL;
	}
	fclose( state_file );

	if ( !zcnf_load( &document, filepath ) ) {
		return NULL;
	}

	state = (zcnf_state_t*) malloc( sizeof( zcnf_state_t ) );
	list_init( &state->apps );
	state->uid = uid;

	/*struct stat state_stat_info;
	if ( !stat( filepath, &state_stat_info ) ) {
		state->uid = state_stat_info.st_uid;
	}*/

	yaml_node_t* root = yaml_document_get_root_node( &document );
	if ( !root || root->type != YAML_MAPPING_NODE ) {
		goto quit;
	}

	for ( i = 0; i < root->data.mapping.pairs.top - root->data.mapping.pairs.start; i++ ) {
		yaml_node_t* node = yaml_document_get_node( &document, root->data.mapping.pairs.start[ i ].key );

		if ( ! node || node->type != YAML_SCALAR_NODE ) {
			continue;
		}

		if ( strcmp( "applications", (char*) node->data.scalar.value ) == 0 ) {
			if ( !zcnf_state_load_apps( state, &document, root->data.mapping.pairs.start[ i ].value ) ) {
				continue;
			}
		}
	}

quit:
	yaml_document_delete( &document );
	return state;
}

zcnf_app_t* zcnf_app_load( char* cnf_path ) {
	char filepath[ 128 ];
	zcnf_app_t* cnf = NULL;
	yaml_document_t document;
	int i;

	if ( !cnf_path ) cnf_path = ZM_APP_CNF_FILE;

	if ( !expand_tilde( cnf_path, filepath, sizeof( filepath ), getuid() ) )
		return NULL;

	if ( !zcnf_load( &document, filepath ) )
		return NULL;

	// start parsing config settings
	yaml_node_t* root = yaml_document_get_node( &document, 1 );

	if ( ! root || root->type != YAML_MAPPING_NODE ) {
		goto quit;
	}

	cnf = (zcnf_app_t*) malloc( sizeof( zcnf_app_t ) );
	memset( cnf, 0, sizeof( zcnf_app_t ) );
	list_init( &cnf->websites );

	strcpy( cnf->proxy.ip, ZM_PROXY_DEFAULT_ADDR );
	cnf->proxy.port = ZM_PROXY_DEFAULT_PORT;

	for ( i = 0; i < root->data.mapping.pairs.top - root->data.mapping.pairs.start; i++ ) {
		yaml_node_t* node = yaml_document_get_node( &document, root->data.mapping.pairs.start[ i ].key );

		if ( ! node || node->type != YAML_SCALAR_NODE ) {
			zcnf_app_free( cnf );
			cnf = NULL;
			break;
		}

		if ( strcmp( "websites", (char*) node->data.scalar.value ) == 0 ) {
			if ( !zcnf_load_websites( cnf, &document, root->data.mapping.pairs.start[ i ].value ) ) {
				zcnf_app_free( cnf );
				cnf = NULL;
				break;
			}
		}

		else if ( strcmp( "proxy", (char*) node->data.scalar.value ) == 0 ) {
			yaml_node_t* proxy_node = yaml_document_get_node( &document, root->data.mapping.pairs.start[ i ].value );
			if ( !proxy_node || proxy_node->type != YAML_SCALAR_NODE )
				continue;
			zcnf_parse_proxy( &cnf->proxy, (char*) proxy_node->data.scalar.value );
		}
	}

quit:
	yaml_document_delete( &document );
	return cnf;
}

zcnf_daemon_t* zcnf_daemon_load() {
	char cnf_path[] = ZM_PROXY_CNF_FILE;
	zcnf_daemon_t* cnf = NULL;
	yaml_document_t document;
	int i, j;

	if ( !zcnf_load( &document, cnf_path ) )
		return NULL;

	// start parsing config settings
	yaml_node_t* root = yaml_document_get_node( &document, 1 );

	if ( ! root || root->type != YAML_MAPPING_NODE ) {
		goto quit;
	}

	cnf = (zcnf_daemon_t*) malloc( sizeof( zcnf_daemon_t ) );
	memset( cnf, 0, sizeof( zcnf_daemon_t ) );

	bool default_proxy = true;
	for ( i = 0; i < root->data.mapping.pairs.top - root->data.mapping.pairs.start; i++ ) {
		yaml_node_t* node = yaml_document_get_node( &document, root->data.mapping.pairs.start[ i ].key );

		if ( ! node || node->type != YAML_SCALAR_NODE ) {
			zcnf_daemon_free( cnf );
			cnf = NULL;
			break;
		}

		if ( strcmp( "default proxy", (char*) node->data.scalar.value ) == 0 ) {
			yaml_node_t* default_proxy_node = yaml_document_get_node( &document, root->data.mapping.pairs.start[ i ].value );
			if ( !default_proxy_node || default_proxy_node->type != YAML_SCALAR_NODE )
				continue;

			if ( strcmp( "off", (char*) default_proxy_node->data.scalar.value ) == 0 ) {
				default_proxy = false;
			}
		}

		else if ( strcmp( "other proxies", (char*) node->data.scalar.value ) == 0 ) {
			yaml_node_t* other_proxies_node = yaml_document_get_node( &document, root->data.mapping.pairs.start[ i ].value );

			if ( !other_proxies_node || other_proxies_node->type != YAML_SEQUENCE_NODE )
				continue;

			for ( j = 0; j < other_proxies_node->data.sequence.items.top - other_proxies_node->data.sequence.items.start; j++ ) {
				yaml_node_t* proxy_node = yaml_document_get_node( &document, other_proxies_node->data.sequence.items.start[ j ] );
				if ( !proxy_node || proxy_node->type != YAML_SCALAR_NODE )
					continue;

				zcnf_parse_proxy( &cnf->proxies[ cnf->n ], (char*) proxy_node->data.scalar.value );

				if ( ++cnf->n == ZM_PROXY_MAX_PROXIES - 1 )
					break;
			}
		}

		else if ( strcmp( "ssl cert path", (char*) node->data.scalar.value ) == 0 ) {
			yaml_node_t* ssl_cert_path = yaml_document_get_node( &document, root->data.mapping.pairs.start[ i ].value );

			if ( !ssl_cert_path || ssl_cert_path->type != YAML_SCALAR_NODE )
				continue;

			cnf->ssl_cert_path = strdup( ssl_cert_path );
		}

		else if ( strcmp( "ssl key path", (char*) node->data.scalar.value ) == 0 ) {
			yaml_node_t* ssl_key_path = yaml_document_get_node( &document, root->data.mapping.pairs.start[ i ].value );

			if ( !ssl_key_path || ssl_key_path->type != YAML_SCALAR_NODE )
				continue;

			cnf->ssl_key_path = strdup( ssl_key_path );
		}
	}

	if ( default_proxy ) {
		strncpy( cnf->proxies[ cnf->n ].ip, ZM_PROXY_DEFAULT_ADDR, sizeof( cnf->proxies[ cnf->n ].ip ) - 1 );
		cnf->proxies[ cnf->n ].port = ZM_PROXY_DEFAULT_PORT;
		cnf->n++;
	}

quit:
	yaml_document_delete( &document );
	return cnf;
}

void zcnf_state_set_app( zcnf_state_t* state, const char* path, pid_t pid, bool stopped ) {
	zcnf_state_app_t* app;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		if ( strcmp( app->path, path ) == 0 ) {
			break;
		}
	}

	if ( i == list_size( &state->apps ) ) {
		app = (zcnf_state_app_t*) malloc( sizeof( zcnf_state_app_t ) );
		app->path  = strdup( path );
		list_append( &state->apps, app );
	}

	app->pid = pid;
	app->stopped = stopped;

}

bool zcnf_state_app_is_running( zcnf_state_t* state, const char* path ) {
	zcnf_state_app_t* app;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		if ( strcmp( app->path, path ) == 0 ) {
			if ( !app->pid ) return false;
			return true;
		}
	}

	return false;
}

void zcnf_state_save( zcnf_state_t* state ) {
	char filepath[ 128 ];
	yaml_document_t document;
	int root, apps, name;
	char number[64];

	if ( !expand_tilde( ZM_USR_STATE_FILE, filepath, sizeof( filepath ), state->uid ) )
		return;

	memset( &document, 0, sizeof( yaml_document_t ) );
	assert( yaml_document_initialize( &document, NULL, NULL, NULL, 0, 0 ) );

	assert( ( root = yaml_document_add_mapping( &document, NULL, YAML_BLOCK_MAPPING_STYLE ) ) );
	assert( ( apps = yaml_document_add_sequence( &document, NULL, YAML_BLOCK_SEQUENCE_STYLE ) ) );
	assert( ( name = yaml_document_add_scalar( &document, NULL, (unsigned char*) "applications", 12, YAML_PLAIN_SCALAR_STYLE ) ) );

	assert( yaml_document_append_mapping_pair( &document, root, name, apps ) );

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		zcnf_state_app_t* app = list_get_at( &state->apps, i );
		int app_map, name, value;

		assert( ( app_map = yaml_document_add_mapping( &document, NULL, YAML_BLOCK_SEQUENCE_STYLE ) ) );

		assert( ( name = yaml_document_add_scalar( &document, NULL,
		  (unsigned char*) "path", 4, YAML_PLAIN_SCALAR_STYLE ) ) );
		assert( ( value = yaml_document_add_scalar( &document, NULL,
		  (unsigned char*) app->path, strlen( app->path ), YAML_PLAIN_SCALAR_STYLE ) ) );
		assert( yaml_document_append_mapping_pair( &document, app_map, name, value ) );

		sprintf( number, "%d", app->pid );
		assert( ( name = yaml_document_add_scalar( &document, NULL,
		  (unsigned char*) "pid", 3, YAML_PLAIN_SCALAR_STYLE ) ) );
		assert( ( value = yaml_document_add_scalar( &document, NULL,
		  (unsigned char*) number, -1, YAML_PLAIN_SCALAR_STYLE ) ) );
		assert( yaml_document_append_mapping_pair( &document, app_map, name, value ) );

		sprintf( number, "%d", app->stopped );
		assert( ( name = yaml_document_add_scalar( &document, NULL,
		  (unsigned char*) "stopped", 7, YAML_PLAIN_SCALAR_STYLE ) ) );
		assert( ( value = yaml_document_add_scalar( &document, NULL,
		  (unsigned char*) number, -1, YAML_PLAIN_SCALAR_STYLE ) ) );
		assert( yaml_document_append_mapping_pair( &document, app_map, name, value ) );

		assert( yaml_document_append_sequence_item( &document, apps, app_map ) );

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

void zcnf_app_free( zcnf_app_t* cnf ) {
	// TODO: free strdup'd website data
	int i = 0;
	for ( ; i < list_size( &cnf->websites ); i++ ) {
		zcnf_website_t* website = list_get_at( &cnf->websites, i );
		free( website->url );
		free( website->redirect_url );
		free( website->ip_address );
		while( list_size( &website->pubdirs ) ) {
			free( list_fetch( &website->pubdirs ) );
		}
		list_destroy( &website->pubdirs );
		while( list_size( &website->modules ) ) {
			zcnf_module_t* module = list_fetch( &website->modules );
			int i;
			for ( i = 0; i < module->argc; i++ ) {
				free( module->argv[ i ] );
			}
			free( module->name );
			free( module );
		}
		list_destroy( &website->modules );
		while( list_size( &website->ignore ) ) {
			free( list_fetch( &website->ignore ) );
		}
		list_destroy( &website->ignore );
		free( website );
	}
	list_destroy( &cnf->websites );
	free( cnf );
}

void zcnf_state_app_free( zcnf_state_app_t* app ) {
	free( app->path );
	free( app );
}

void zcnf_state_free( zcnf_state_t* state ) {
	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		zcnf_state_app_t* app = list_get_at( &state->apps, i );
		zcnf_state_app_free( app );
	}
	list_destroy( &state->apps );
	free( state );
}

void zcnf_daemon_free( zcnf_daemon_t* daemon ) {
	if ( daemon->ssl_cert_path ) free( daemon->ssl_cert_path );
	if ( daemon->ssl_key_path ) free( daemon->ssl_key_path );
	free( daemon );
}
