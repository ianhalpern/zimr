#!/usr/bin/python
import sys
version = sys.argv.pop(1)

from distutils.core import setup

setup(
	name         = 'zimr',
	version      = version,
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
		('/usr/local/lib/python2.6/dist-packages/zimr/', ['zimr/__init__.so']),
	)
)
