/*
 * splash_common.c - Miscellaneous functions used by both the kernel helper and
 *                   user utilities.
 * 
 * Copyright (C) 2004, Michal Januszewski <spock@gentoo.org>
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * $Header: /srv/cvs/splash/utils/splash_common.c,v 1.8 2005/01/29 23:27:49 spock Exp $
 * 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <linux/fb.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "splash.h"

struct fb_var_screeninfo   fb_var;
struct fb_fix_screeninfo   fb_fix;

enum ENDIANESS endianess;
char *config_file = NULL;

enum TASK arg_task = none; 
int arg_fb = 0;
int arg_vc = 0;
char arg_mode = 'v';
char *arg_theme = NULL;
u16 arg_progress = 0;

struct fb_image pic;
char *pic_file = NULL;

void detect_endianess(void)
{
	u16 t = 0x1122;

	if (*(u8*)&t == 0x22) {
		endianess = little;
	} else {
		endianess = big;
	}

	DEBUG("This system is %s-endian.\n", (endianess == little) ? "little" : "big");
}

int get_fb_settings(int fb_num)
{
	char fn[11];
	int fb;
#ifdef TARGET_KERNEL
	char sys[128];
#endif

	sprintf(fn, "/dev/fb/%d", fb_num);	
	fb = open(fn, O_WRONLY, 0);

	if (fb == -1) {
		sprintf(fn, "/dev/fb%d", fb_num);
		fb = open(fn, O_WRONLY, 0);
	}
	
	if (fb == -1) {
	
#ifdef TARGET_KERNEL
		sprintf(sys, "/sys/class/graphics/fb%d/dev", fb_num);
		create_dev(fn, sys, 0x1);
		fb = open(fn, O_WRONLY, 0);
		if (fb == -1)
			remove_dev(fn, 0x1);
		if (fb == -1)
#endif
		{
			printerr("Failed to open /dev/fb%d or /dev/fb%d for reading.\n", fb_num, fb_num);
			return 1;
		}
	}
		
	if (ioctl(fb,FBIOGET_VSCREENINFO,&fb_var) == -1) {
		printerr("Failed to get fb_var info.\n");
		return 2;
	}

	if (ioctl(fb,FBIOGET_FSCREENINFO,&fb_fix) == -1) {
		printerr("Failed to get fb_fix info.\n");
		return 3;
	}

	close(fb);

#ifdef TARGET_KERNEL
	remove_dev(fn, 0x1);
#endif

	return 0;
}

char *get_filepath(char *path) 
{
	char buf[512];
	
	if (path[0] == '/')
		return strdup(path);

	snprintf(buf, 512, "%s/%s/%s", THEME_DIR, arg_theme, path);
	return strdup(buf);	
}

char *get_cfg_file(char *theme) 
{
	char buf[512];

	snprintf(buf, 512, "%s/%s/%dx%d.cfg", THEME_DIR, theme, fb_var.xres, fb_var.yres);
	return strdup(buf);	
}

int do_getpic(unsigned char origin, unsigned char do_cmds, char mode)
{	
	int res;
	
	if (!config_file) {
		printerr("No config file.\n");
		return -1;
	}

	pic.width = fb_var.xres;
	pic.height = fb_var.yres;
	pic.depth = fb_var.bits_per_pixel;

	/* if we have a 8bpp pixel mode to deal with, we need to use pic256
	 * and silentpic256, which can currently only be PNGs */
	if (fb_var.bits_per_pixel == 8) {

		if ((!cf_pic256 && mode == 'v') || (!cf_silentpic256 && mode == 's')) {
			printerr("No 8bpp picture for the current splash mode (%c) specified in the theme config.\n", mode);
			return -2;
		}
	
		pic_file = (mode == 'v') ? cf_pic256 : cf_silentpic256;
#ifdef CONFIG_PNG
		if (load_png(pic_file, &pic, mode)) {
			printerr("Failed to load PNG file %s.\n", pic_file);
			return -2;
		}
		
		if (do_cmds) {
			cmd_setpic(&pic, origin);	
			free((void*)pic.data);
			if (pic.cmap.red)
				free(pic.cmap.red);
		}
#endif
	} else {

		/* here we handle 15bpp+ modes, the pics can be either jpgs or
		 * pngs, so we have to check it out first */

		pic_file = (mode == 'v') ? cf_pic : cf_silentpic;
#ifdef CONFIG_PNG		
		if (is_png(pic_file))
			res = load_png(pic_file, &pic, mode);
		else 
#endif
			res = decompress_jpeg(pic_file, &pic);
	
		if (res) {
			printerr("Failed to load image %s.\n", pic_file);
			return -2;
		}
		
		draw_boxes((u8*)pic.data, mode, origin);

		if (mode == 's') {
			draw_icons((u8*)pic.data);
		}
			
		if (do_cmds) {
			cmd_setpic(&pic, origin);
			free((void*)pic.data);
			if (pic.cmap.red)
				free(pic.cmap.red);
		}
	}

	return 0;
}

int do_config(unsigned char origin)
{
	if (!config_file) {
		printerr("No config file.\n");
		return -1;
	}
		
	/* if the user specified invalid values for the text field - correct it.
	 * also setup default values (text field coverting the whole screen) */

	if (cf.tx > fb_var.xres)
		cf.tx = 0;

	if (cf.ty > fb_var.yres)
		cf.ty = 0;

	if (cf.tw > fb_var.xres || cf.tw == 0)
		cf.tw = fb_var.xres;

	if (cf.th > fb_var.yres || cf.th == 0)
		cf.th = fb_var.yres;

	cmd_setcfg(origin);
	return 0;
}

