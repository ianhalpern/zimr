#include "general.h"

int main( int argc, char* argv[] ) {
	char normpath[PATH_MAX] = "";
	puts( normalize_path( normpath, "/" ) );
	puts( normalize_path( normpath, "a/" ) );
	puts( normalize_path( normpath, "/a/" ) );
	puts( normalize_path( normpath, "a" ) );
	puts( normalize_path( normpath, "a/b" ) );
	puts( normalize_path( normpath, "a/b/c" ) );
	puts( normalize_path( normpath, "a/d/../b/./c/d" ) );
	puts( normalize_path( normpath, "../a/d/../b/./c/d/c/h/../../e/" ) );
	puts( normalize_path( normpath, "../../a/d/../b/./c/d/c/h/../../e/f/." ) );
	puts( normalize_path( normpath, "./../../a/d/../b/./c/d/c/h/../../e/f/g/d/.." ) );
	puts( normalize_path( normpath, "./../../aa/dd//////////../bb/./cc/dd/cc/hh/../../ee/ff/gg/dd/.." ) );
	return 0;
}

