from mako.template  import Template
from mako.lookup    import TemplateLookup
from mako           import exceptions as mako_exceptions

def render( path, connection=None ):
	result = "";

	try:
		lookup = TemplateLookup(
			directories=['.'],
			module_directory='psp_cache/',
			input_encoding='utf-8',
			encoding_errors='replace'
		)
		template = Template( filename=path, module_directory='psp_cache/', lookup=lookup )
	except OSError:
		return "error 1"
	except ( mako_exceptions.SyntaxException, mako_exceptions.CompileException ):
		connection.response.setStatus( 500 )
		if connection: connection.response.setHeader( "Content-Type", "text/plain" )
		return mako_exceptions.text_error_template().render()

	try:
		#loadme and unload me are so the procs modules are in scope
		return template.render_unicode( connection=connection ).encode( 'utf-8', 'replace' )
	except:
		connection.response.setStatus( 500 )
		if connection: connection.response.setHeader( "Content-Type", "text/plain" )
		return mako_exceptions.text_error_template().render()
