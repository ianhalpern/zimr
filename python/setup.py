#!/usr/bin/python
import sys
zimr_version = sys.argv.pop(1)
python_version = sys.argv.pop(1)
from distutils.core import setup

setup(
	name         = 'zimr',
	version      = zimr_version,
	description  = 'Zimr python bindings',
	author       = 'Ian Halpern',
	author_email = 'ian@ian-halpern.com',
	url          = 'https://zimr.org',
	download_url = 'https://zimr.org',
	packages     = (
		'zimr',
		'zimr.page_handlers',
		'zimr.handlers',
	),
	data_files   = (
		# TODO: this is a hack!
		('/usr/local/lib/python%s/dist-packages/zimr/' % python_version, ['zimr/__init__.so']),
	)
)
