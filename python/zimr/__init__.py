from czimr import *
import re

def connection_filter( url_filters=[ '.*' ], connection_handler=None ):
	def _connection_handler( connection ):
		for connection_handler, url_filters in connection_filter.connection_handlers:
			for url_filter in url_filters:
				if re.search( url_filter, connection.request.url ):
					if connection_handler( connection ):
						return

	def _add_connection_handler( connection_handler ):
		connection_filter.connection_handlers.append( ( connection_handler, url_filters ) )
		return _connection_handler

	if connection_handler:
		_add_connection_handler( connection_handler )
		return _connection_handler

	return _add_connection_handler

connection_filter.connection_handlers = []
