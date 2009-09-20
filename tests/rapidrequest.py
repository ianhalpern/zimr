#!/usr/bin/python
import urllib
c = []


for i in range( 0, 50 ):
	c.append( urllib.urlopen( "http://zimr:8080/hello-world.html" ) )

#for i in range( 0, len( c ) ):
#	c[ i ].close( )
