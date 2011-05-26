from mako.template  import Template
from mako.lookup    import TemplateLookup

lookup = TemplateLookup(
	directories=['.'],
	module_directory='psp_cache/',
	input_encoding='utf-8',
	encoding_errors='replace'
)

def render( path, **kwargs ):
	template = lookup.get_template( path )
	return template.render_unicode( **kwargs ).encode( 'utf-8', 'replace' )

