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

int pcnf_load_websites( pcnf_t* cnf, yaml_document_t* document, int index ) {
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
		website->next = cnf->website_node;
		cnf->website_node = website;

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
	}

	return 1;
}

pcnf_t* pcnf_load( ) {
	pcnf_t* cnf = NULL;
	int i;

	FILE* cnf_file = fopen( PD_WS_CONF_FILE, "rb" );
	if ( ! cnf_file ) {
		return NULL;
	}

	yaml_parser_t parser;
	if ( ! yaml_parser_initialize( &parser ) ) {
		fclose( cnf_file );
		return NULL;
	}

	yaml_parser_set_input_file( &parser, cnf_file );

	yaml_document_t document;
	if ( ! yaml_parser_load( &parser, &document ) ) {
		yaml_parser_delete( &parser );
		fclose( cnf_file );
		return NULL;
	}

	// we don't need the parser or file anymore
	yaml_parser_delete( &parser );
	fclose( cnf_file );

	if ( ! yaml_document_get_root_node( &document ) ) {
		goto quit;
	}

	// start parsing config settings
	yaml_node_t* root = yaml_document_get_node( &document, 1 );

	if ( ! root || root->type != YAML_MAPPING_NODE ) {
		goto quit;
	}

	cnf = (pcnf_t*) malloc( sizeof( pcnf_t ) );
	memset( cnf, 0, sizeof( pcnf_t ) );
	for ( i = 0; i < root->data.mapping.pairs.top - root->data.mapping.pairs.start; i++ ) {
		yaml_node_t* node = yaml_document_get_node( &document, root->data.mapping.pairs.start[ i ].key );

		if ( ! node || node->type != YAML_SCALAR_NODE ) {
			pcnf_free( cnf );
			cnf = NULL;
			break;
		}

		if ( strcmp( "websites", (char*) node->data.scalar.value ) == 0 ) {
			if ( ! pcnf_load_websites( cnf, &document, root->data.mapping.pairs.start[ i ].value ) ) {
				pcnf_free( cnf );
				cnf = NULL;
				break;
			}
		}
	}

quit:
	yaml_document_delete( &document );
	return cnf;
}

void pcnf_free( pcnf_t* cnf ) {
	free( cnf );
}
