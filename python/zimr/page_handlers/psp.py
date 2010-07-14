from mako.template  import Template
from mako.lookup    import TemplateLookup
from mako           import exceptions as mako_exceptions

def render( path, **kwargs ):

	#try
	lookup = TemplateLookup(
		directories=['.'],
		module_directory='psp_cache/',
		input_encoding='utf-8',
		encoding_errors='replace'
	)

	template = Template( filename=path, module_directory='psp_cache/', lookup=lookup )

	return template.render_unicode( **kwargs ).encode( 'utf-8', 'replace' )
	#except ( mako_exceptions.SyntaxException, mako_exceptions.CompileException ):
		#return mako_exceptions.text_error_template().render()
