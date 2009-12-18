/*   Zimr - Next Generation Web Server
 *
 *+  Copyright (c) 2009 Ian Halpern
 *@  http://Zimr.org
 *
 *   This file is part of Zimr.
 *
 *   Zimr is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Zimr is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Zimr.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#include "urldecoder.h"

char* urlcodes[] = {
"",   // %00
"",   // %01
"",   // %02
"",   // %03
"",   // %04
"",   // %05
"",   // %06
"",   // %07
"",   // %08
"",   // %09
"",   // %0A
"",   // %0B
"",   // %0C
"",   // %0D
"",   // %0E
"",   // %0F
"",   // %10
"",   // %11
"",   // %12
"",   // %13
"",   // %14
"",   // %15
"",   // %16
"",   // %17
"",   // %18
"",   // %19
"",   // %1A
"",   // %1B
"",   // %1C
"",   // %1D
"",   // %1E
"",   // %1F
" ",  // %20
"!",  // %21
"\"", // %22
"#",  // %23
"$",  // %24
"%",  // %25
"&",  // %26
"'",  // %27
"(",  // %28
")",  // %29
"*",  // %2A
"+",  // %2B
",",  // %2C
"-",  // %2D
".",  // %2E
"/",  // %2F
"0",  // %30
"1",  // %31
"2",  // %32
"3",  // %33
"4",  // %34
"5",  // %35
"6",  // %36
"7",  // %37
"8",  // %38
"9",  // %39
":",  // %3A
";",  // %3B
"<",  // %3C
"=",  // %3D
">",  // %3E
"?",  // %3F
"@",  // %40
"A",  // %41
"B",  // %42
"C",  // %43
"D",  // %44
"E",  // %45
"F",  // %46
"G",  // %47
"H",  // %48
"I",  // %49
"J",  // %4A
"K",  // %4B
"L",  // %4C
"M",  // %4D
"N",  // %4E
"O",  // %4F
"P",  // %50
"Q",  // %51
"R",  // %52
"S",  // %53
"T",  // %54
"U",  // %55
"V",  // %56
"W",  // %57
"X",  // %58
"Y",  // %59
"Z",  // %5A
"[",  // %5B
"\\", // %5C
"]",  // %5D
"^",  // %5E
"_",  // %5F
"`",  // %60
"a",  // %61
"b",  // %62
"c",  // %63
"d",  // %64
"e",  // %65
"f",  // %66
"g",  // %67
"h",  // %68
"i",  // %69
"j",  // %6A
"k",  // %6B
"l",  // %6C
"m",  // %6D
"n",  // %6E
"o",  // %6F
"p",  // %70
"q",  // %71
"r",  // %72
"s",  // %73
"t",  // %74
"u",  // %75
"v",  // %76
"w",  // %77
"x",  // %78
"y",  // %79
"z",  // %7A
"{",  // %7B
"|",  // %7C
"}",  // %7D
"~",  // %7E
" ",  // %7F
"€",  // %80
" ",  // %81
"‚",  // %82
"ƒ",  // %83
"„",  // %84
"…",  // %85
"†",  // %86
"‡",  // %87
"ˆ",  // %88
"‰",  // %89
"Š",  // %8A
"‹",  // %8B
"Œ",  // %8C
" ",  // %8D
"Ž",  // %8E
" ",  // %8F
" ",  // %90
"‘",  // %91
"’",  // %92
"“",  // %93
"”",  // %94
"•",  // %95
"–",  // %96
"—",  // %97
"˜",  // %98
"™",  // %99
"š",  // %9A
"›",  // %9B
"œ",  // %9C
" ",  // %9D
"ž",  // %9E
"Ÿ",  // %9F
" ",  // %A0
"¡",  // %A1
"¢",  // %A2
"£",  // %A3
" ",  // %A4
"¥",  // %A5
"|",  // %A6
"§",  // %A7
"¨",  // %A8
"©",  // %A9
"ª",  // %AA
"«",  // %AB
"¬",  // %AC
"¯",  // %AD
"®",  // %AE
"¯",  // %AF
"°",  // %B0
"±",  // %B1
"²",  // %B2
"³",  // %B3
"´",  // %B4
"µ",  // %B5
"¶",  // %B6
"·",  // %B7
"¸",  // %B8
"¹",  // %B9
"º",  // %BA
"»",  // %BB
"¼",  // %BC
"½",  // %BD
"¾",  // %BE
"¿",  // %BF
"À",  // %C0
"Á",  // %C1
"Â",  // %C2
"Ã",  // %C3
"Ä",  // %C4
"Å",  // %C5
"Æ",  // %C6
"Ç",  // %C7
"È",  // %C8
"É",  // %C9
"Ê",  // %CA
"Ë",  // %CB
"Ì",  // %CC
"Í",  // %CD
"Î",  // %CE
"Ï",  // %CF
"Ð",  // %D0
"Ñ",  // %D1
"Ò",  // %D2
"Ó",  // %D3
"Ô",  // %D4
"Õ",  // %D5
"Ö",  // %D6
" ",  // %D7
"Ø",  // %D8
"Ù",  // %D9
"Ú",  // %DA
"Û",  // %DB
"Ü",  // %DC
"Ý",  // %DD
"Þ",  // %DE
"ß",  // %DF
"à",  // %E0
"á",  // %E1
"â",  // %E2
"ã",  // %E3
"ä",  // %E4
"å",  // %E5
"æ",  // %E6
"ç",  // %E7
"è",  // %E8
"é",  // %E9
"ê",  // %EA
"ë",  // %EB
"ì",  // %EC
"í",  // %ED
"î",  // %EE
"ï",  // %EF
"ð",  // %F0
"ñ",  // %F1
"ò",  // %F2
"ó",  // %F3
"ô",  // %F4
"õ",  // %F5
"ö",  // %F6
"÷",  // %F7
"ø",  // %F8
"ù",  // %F9
"ú",  // %FA
"û",  // %FB
"ü",  // %FC
"ý",  // %FD
"þ",  // %FE
"ÿ",  // %FF
};

char* url_decode( char* raw_url, char* buffer, int url_len ) {
	char url[ url_len + 1 ],* url_ptr;
	memset( url, 0, sizeof( url ) );
	strncpy( url, raw_url, url_len );
	url_ptr = url;

	char* ptr;
	char hex[ 3 ];
	unsigned int i;

	*buffer = 0;

	while ( ( ptr = strchr( url_ptr, '%' ) ) ) {
		strncat( buffer, url_ptr, ptr - url_ptr );
		ptr++;

		if ( *ptr == '\0' || *( ptr + 1 ) == '\0' )
			break;

		strncpy( hex, ptr, 2 );
		hex[ 2 ] = '\0';
		xtoi( hex, &i );
		strcat( buffer, urlcodes[ i ] );
		url_ptr = ptr + 2;
	}

	strcat( buffer, url_ptr );
	return buffer;
}
