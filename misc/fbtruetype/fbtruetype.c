/*
 *	fbmngplay - fb console MNG player.
 *	(c) 2001-2002 by Stefan Reinauer, <stepan@suse.de>
 *  
 *	This program is based on mngplay, written and (C) by
 *	Ralph Giles <giles@ashlu.bc.ca>
 *
 *	This program my be redistributed under the terms of the
 *	GNU General Public Licence, version 2, or at your preference,
 *	any later version.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <signal.h>


#include "fbtruetype.h"
#include "messages.h"
#include "console.h"

volatile int run = 1;
int verbose = 0;
int buffered = 0;
int waitsignal = 0;
int delta = 16;
int sconly=0;

unsigned int fbbytes, fbx, fby;
unsigned int fbypos=100, fbxpos=100;
unsigned int fblinelen, alpha=100;
unsigned char *framebuffer, *font=DEFAULT_FONTNAME;
unsigned int fgcolor=0xff0000;
unsigned int fontsize=36;
int strict_font=0;
int rendertext(char *text, char *fontname, unsigned int size, unsigned int forecol);

int main(int argc, char *argv[])
{
	int fbdev,c,option_index;
	unsigned int alpha;
	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;

	/* Check which console we're running on */
	init_consoles();
		
	alpha = 100;
	while (1) {
		static struct option long_options[] = {
			{"help", 0, 0, 'h'},
			{"verbose", 0, 0, 'v'},
			{"alpha", 1, 0, 'a'},
			{"version", 0, 0, 'V'},
			{"start-console",0,0,'S'},
			{"size",1,0,'s'},
			{"console",1,0,'c'}, 
			{"font",1,0,'f'}, 
			{"textcolor",1,0,'t'}, 
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "a:x:y:h?vVSc:f:t:s:",
				long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
		case 'a':
			alpha = atoi(optarg);
			if (alpha > 100)
				alpha = 100;
			break;
		case 'x':
			fbxpos = atoi(optarg);
			break;
		case 'y':
			fbypos = atoi(optarg);
			break;
		case '?':
		case 'h':
			usage(argv[0]);
			exit(0);
		case 'v':
			verbose = 1;
			break;
		case 'V':
			version();
			exit(0);
		case 'c':
			start_console=atoi(optarg)-1;
		case 'S':
			sconly=1;
			break;
		case 'f':
			strict_font=1;
			font=strdup(optarg);
			break;
		case 't':
			fgcolor=strtol(optarg, NULL, 16);
			break;
		case 's':
			fontsize=strtol(optarg, NULL, 10);
			break;

		default:
			break;
		}
	}
	if (optind >= argc) {
		printf("No text\n");
		exit(0);
	}
	
	/* Initialize framebuffer */
        fbdev = open("/dev/fb/0", O_RDWR);
        if (fbdev < 0) {
 		fbdev = open("/dev/fb0", O_RDWR);
		if (fbdev < 0) {
			fprintf(stderr, "error while opening framebuffer.\n");
			exit(fbdev);
		}
	}

	ioctl(fbdev, FBIOGET_VSCREENINFO, &var);
	fbbytes = var.bits_per_pixel>>3;
	fbx=var.xres; 
	fby=var.yres;
	ioctl(fbdev, FBIOGET_FSCREENINFO, &fix);
	fblinelen = fix.line_length;

	framebuffer = mmap(NULL,  fblinelen* fby ,
		 PROT_WRITE | PROT_READ, MAP_SHARED, fbdev, var.yoffset * fblinelen);

	rendertext (argv[optind], font, fontsize, fgcolor);
	return 0;
}
