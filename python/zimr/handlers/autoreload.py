import os
import sys
import time
import zimr

_interval = 1.0
_times = {}
_files = []

_needs_restart = False


def _modified(path):
    try:
        # If path doesn't denote a file and were previously
        # tracking it, then it has been removed or the file type
        # has changed so force a restart. If not previously
        # tracking the file then we can ignore it as probably
        # pseudo reference such as when file extracted from a
        # collection of modules contained in a zip file.

        if not os.path.isfile(path):
            return path in _times

        # Check for when file last modified.

        mtime = os.stat(path).st_mtime
        if path not in _times:
            _times[path] = mtime

        # Force restart when modification time has changed, even
        # if time now older, as that could indicate older file
        # has been restored.

        if mtime != _times[path]:
            return True
    except:
        # If any exception occured, likely that file has been
        # been removed just before stat(), so force a restart.

        return True

    return False

def check():
	global _needs_restart

	for module in sys.modules.values():
		if not hasattr(module, '__file__'):
			continue
		path = getattr(module, '__file__')
		if not path:
			continue
		if os.path.splitext(path)[1] in ['.pyc', '.pyo', '.pyd']:
			path = path[:-1]
		if _modified(path):
			_needs_restart = True
			return True

	# Check modification times on files which have
	# specifically been registered for monitoring.

	for path in _files:
		if _modified(path):
			_needs_restart = True
			return True
	return False

def track(path):
    if not path in _files:
        _files.append(path)

def wrapper( func ):
	def connection_handler( connection ):
		if check():
			zimr.modpythonReload()
		else:
			func( connection )
	return connection_handler

@wrapper
def connection_handler( connection ):
	connection.sendFile()
