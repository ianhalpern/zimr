#!/usr/bin/python
import urllib
c = []
c.append( urllib.urlopen( "http://localhost:1111/largefile.dat.3" ) )
c.append( urllib.urlopen( "http://localhost:1111/largefile.dat.3" ) )


#for i in range( 0, 2 ):
#	c.append( urllib.urlopen( "http://localhost:1111/largefile.dat.3" ) )

#for i in range( 0, len( c ) ):
#	c[ i ].close( )
