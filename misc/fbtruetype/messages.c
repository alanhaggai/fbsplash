/*
 *	(c) 2002-2003 by Stefan Reinauer, <stepan@suse.de>
 *  
 *	This program my be redistributed under the terms of the
 *	GNU General Public Licence, version 2, or at your preference,
 *	any later version.
 */

#include <stdio.h>
#include "fbtruetype.h"
#include "messages.h"

void usage(char *name) 
{
	fprintf(stderr, "\nusage: %s [ parameters ] text\n", name);
	fprintf(stderr, 
		"\n	-x: x coordinate\n"
		"	-y: y coordinate\n"
		"	-f, --font: font name (.ttf file)\n"
		"	-t, --textcolor: text color (hex)\n"
                "       -s, --size: font size (default 36)\n"
		"	-a, --alpha: default alpha channel 1..100\n"
		"	-v, --verbose: verbose mode\n"
		"	-V, --version: show version and exit\n"
		"	-?, -h, --help: print this help.\n"
		"	-S --start-console: output on start console only.\n"
		"       -c: start on specified console.\n\n");
}

void version(void)
{
	fprintf(stderr, 
		"fbtruetype v%s, Copyright (C) 2002, 2003 Stefan Reinauer <stepan@suse.de>\n\n"
		"fbtruetype comes with ABSOLUTELY NO WARRANTY;\n"
		"This is free software, and you are welcome to redistribute it\n"
		"under certain conditions; Check the GPL for details.\n", FBTRUETYPE_VERSION);
}
