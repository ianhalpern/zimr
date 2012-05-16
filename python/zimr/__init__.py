from czimr import *
import re

def connection_filter( url_filters=[ '.*' ], has_params=[], connection_handler=None ):
	def _connection_handler( connection ):
		for connection_handler, url_filters, has_params in connection_filter.connection_handlers:
			for url_filter in url_filters:
				if re.search( url_filter, connection.request.url ):
					param_match = 0
					for param in has_params:
						if param in connection.request.params.keys():
							param_match+=1
					if param_match == len( has_params ):
						if connection_handler( connection ):
							return

	def _add_connection_handler( connection_handler ):
		connection_filter.connection_handlers.append( ( connection_handler, url_filters, has_params ) )
		return _connection_handler

	if connection_handler:
		_add_connection_handler( connection_handler )
		return _connection_handler

	return _add_connection_handler

connection_filter.connection_handlers = []
