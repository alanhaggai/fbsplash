/*
 * splash.c - The core of splash_util
 *
 * Copyright (C) 2004, Michal Januszewski <spock@gentoo.org>
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * $Header: /srv/cvs/splash/utils/splash.c,v 1.18 2005/01/29 23:27:49 spock Exp $ 
 * 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <unistd.h>

#include "splash.h"

struct option options[] = {
	{ "fb", 	required_argument, NULL, 0x100 },
	{ "vc", 	required_argument, NULL, 0x101 },
	{ "cmd", 	required_argument, NULL, 0x102 },
	{ "theme", 	required_argument, NULL, 0x103 },
	{ "mode", 	required_argument, NULL, 0x104 },
	{ "progress", 	required_argument, NULL, 0x105 },
	{ "help",	no_argument, NULL, 'h'}	
};

struct cmd {
	char *name;
	int value;
};

struct cmd cmds[] = {
	{ "on",		on },
	{ "off",	off },
	{ "setcfg", 	setcfg },
	{ "getcfg", 	getcfg },
	{ "setpic", 	setpic },
	{ "paint", 	paint },
	{ "repaint",	repaint },
	{ "setmode", 	setmode },
	{ "getmode", 	getmode },
	{ "getstate", 	getstate },
};


void usage(void)
{

	printf(
"splash_util/splashutils-" PKG_VERSION "\n"
"Usage: splash_util [options] -c <cmd>\n\n"
"Commands:\n"
"  on       enable splash on a virtual console\n"
"  off      disable splash on a virtual console\n"
"  getstate get splash state on a virtual console\n"
"  setcfg   set splash configuration for a virtual console\n"
"  getcfg   get splash configuration for a virtual console\n"
"  setpic   set splash background picture for a virtual console;\n"
"           note that this command will only have any effect if\n"
"           it's called for the current console\n"
"  paint    paint background picture\n"
"  setmode  set global splash mode\n"
"  getmode  get global splash mode\n\n"
"Options:\n"
"  -c, --cmd=CMD       execute command CMD\n"
"  -h, --help          show this help message\n"
"  -t, --theme=THEME   use theme THEME\n"
"      --vc=NUM        use NUMth virtual console\n"
"      --fb=NUM        use NUMth framebuffer device\n"
"  -m, --mode=(v|s)    use either silent (s) or verbsose (v) mode\n"
"  -p, --progress=NUM  set progress to NUM/65535 * 100%%\n");
}

int main(int argc, char **argv)
{
	char dev[16];
	unsigned int c, i, y;
	int fp, err = 0;

	detect_endianess();
	arg_task = none;
	
	while ((c = getopt_long(argc, argv, "c:t:m:p:h", options, NULL)) != EOF) {
	
		switch (c) {
		
		case 'h':
			usage();
			return 0;
			
		case 0x100:
			arg_fb = atoi(optarg);
			break;

		case 0x101:
			arg_vc = atoi(optarg);
			break;

		case 0x103:
		case 't':
			arg_theme = optarg;
			break;
		
		case 0x102:
		case 'c':
			for (i = 0; i < sizeof(cmds) / sizeof(struct cmd); i++) {
			
				if (!strcmp(optarg, cmds[i].name)) {
					arg_task = cmds[i].value;
					break;
				}
			}
			break;
			
		case 'm':
		case 0x104:
			if (optarg[0] == 's')
				arg_mode = 's';
			else
				arg_mode = 'v';
			break;
			
		case 'p':
		case 0x105:
			arg_progress = atoi(optarg);
			break;
		}
	}

	if (arg_task == none) {
		usage();
		return 0;
	}
	
	get_fb_settings(arg_fb);
	
	if (arg_theme)
		config_file = get_cfg_file(arg_theme);
		
	if (config_file)
		parse_cfg(config_file);

	/* we've got to repaint the whole screen if we have icons to draw */
/*
	if (arg_task == paint && cf_icons_cnt > 0)
		arg_task = repaint;
*/
	
	switch (arg_task) {

	case on:
		cmd_setstate(1, FB_SPLASH_IO_ORIG_USER);
		break;
		
	case off:
		cmd_setstate(0, FB_SPLASH_IO_ORIG_USER);
		break;

	case setpic:
	{
		struct vt_stat stat;
		
		if ((fp = open("/dev/tty", O_NOCTTY)) != -1) {
			if (ioctl(fp, VT_GETSTATE, &stat) != -1) {
				if (arg_vc != stat.v_active - 1)
					goto setpic_out;				
			}	
			close(fp);
		}

		do_getpic(FB_SPLASH_IO_ORIG_USER, 1, arg_mode);
setpic_out:	break;
	}

	case setcfg:
		do_config(FB_SPLASH_IO_ORIG_USER);
		break;

	case getstate:
	{
		struct fb_splash_iowrapper wrapper = {
			.vc = arg_vc,
			.origin = FB_SPLASH_IO_ORIG_USER,
			.data = &i,
		};
		
		fp = open(SPLASH_DEV, O_WRONLY);
		if (fp == -1) {
			fprintf(stderr, "Can't open %s\n", SPLASH_DEV);
			break;
		}
		ioctl(fp, FBIOSPLASH_GETSTATE, &wrapper);
		close(fp);
		
		printf("Splash state on console %d: %s\n", arg_vc, (i != 0) ? "on" : "off");
		break;
	}
		
	case setmode:
		fp = open(SPLASH_DEV, O_WRONLY);
		if (fp == -1) {
			fprintf(stderr, "Can't open %s\n", SPLASH_DEV);
			break;
		}
		ioctl(fp, FBIOSPLASH_SETMODE, (arg_mode == 's') ? FB_SPLASH_MODE_SILENT : FB_SPLASH_MODE_VERBOSE);
		close(fp);
		
		if (arg_mode == 's') {
			fp = open("/dev/vc/0", O_WRONLY);
			if (fp == -1) {
				fprintf(stderr, "Can't open /dev/vc/0\n");
				break;
			}		
			ioctl(fp, KDSETMODE, KD_GRAPHICS);
			close(fp);
		}
		
		break;
		
	case getmode:
		fp = open(SPLASH_DEV, O_WRONLY);
		if (fp == -1) {
			fprintf(stderr, "Can't open %s\n", SPLASH_DEV);
			break;
		}
		ioctl(fp, FBIOSPLASH_GETMODE, &i);
		close(fp);

		printf("Splash mode: %s\n", (i == FB_SPLASH_MODE_SILENT) ? "silent" : "verbose");
		break;
		
	case getcfg:
		cmd_getcfg(FB_SPLASH_IO_ORIG_USER);
		break;
		
	case paint:
	case repaint:
		if (do_getpic(FB_SPLASH_IO_ORIG_USER, 0, arg_mode)) {
			err = -1;
			break;
		}

		if (arg_progress > 0 && arg_task == paint && !cf_rects_cnt)
			goto paint_out;

		sprintf(dev, "/dev/fb%d", arg_fb);
		if ((c = open(dev, O_WRONLY)) == -1) {
			sprintf(dev, "/dev/fb/%d", arg_fb);
			if ((c = open(dev, O_WRONLY)) == -1) {
				printerr("Failed to open /dev/fb%d or /dev/fb/%d.\n", arg_fb, arg_fb);
				break;
			}
		}
		
		if (pic.cmap.red)
			ioctl(c, FBIOPUTCMAP, &pic.cmap);

		if (arg_task == repaint) {
		
			i = fb_var.xres * ((fb_var.bits_per_pixel + 7) >> 3);

			for (y = 0; y < fb_var.yres; y++) {
				if (i != fb_fix.line_length || fb_var.yoffset != 0) {
					lseek(c, (fb_var.yoffset + y) * fb_fix.line_length, SEEK_SET);
				}

				write(c, pic.data + i*y, i);
			}
	
		} else {
			int j;
			int bytespp = ((fb_var.bits_per_pixel + 7) >> 3);
			
			for (i = 0; i < cf_rects_cnt; i++) {
				j = (cf_rects[i].x2 - cf_rects[i].x1) * bytespp;
				
				for (y = cf_rects[i].y1; y <= cf_rects[i].y2; y++) {
					lseek(c, (fb_var.yoffset + y) * fb_fix.line_length + cf_rects[i].x1 * bytespp, SEEK_SET);
					write(c, pic.data + (y * fb_var.xres + cf_rects[i].x1) * bytespp, j); 
				}			
			}
		}

		close(c);

paint_out:	free((u8*)pic.data);
		if (pic.cmap.red)
			free(pic.cmap.red);
		break;

	default:
		break;
	}

	if (config_file)
		free(config_file);

	return err;
}
