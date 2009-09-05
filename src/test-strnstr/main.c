#include "general.h"

int main( int argc, char* argv[ ] ) {
	char* s;
	puts( "hi" );
	s = strnstr( "12345", "1", 5 );
	puts( s ? s : "-1" );
	s = strnstr( "12345", "2", 5 );
	puts( s ? s : "-1" );
	s = strnstr( "12345", "3", 5 );
	puts( s ? s : "-1" );
	s = strnstr( "12345", "4", 5 );
	puts( s ? s : "-1" );
	s = strnstr( "12345", "5", 5 );
	puts( s ? s : "-1" );
	s = strnstr( "12345", "5", 4 );
	puts( s ? s : "-1" );
	s = strnstr( "", "5", 4 );
	puts( s ? s : "-1" );
	s = strnstr( "\0 12345", "5", 4 );
	puts( s ? s : "-1" );
	s = strnstr( "12345", "", 4 );
	puts( s ? s : "-1" );
	s = strnstr( "12", "12345", 4 );
	puts( s ? s : "-1" );
	return 0;
}
