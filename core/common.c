/*
 * splash_common.c - Miscellaneous functions used by both the kernel helper and
 *                   user utilities.
 * 
 * Copyright (C) 2004-2005, Michael Januszewski <spock@gentoo.org>
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/vt.h>

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

#ifndef TARGET_KERNEL
char *arg_export = NULL;
#endif

int bytespp = 4;

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

	bytespp = (fb_var.bits_per_pixel + 7) >> 3;
	
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
	if (load_images(mode))
		return -1;

	load_fonts();
	
	if (mode == 'v') {
		render_objs(mode, (u8*)verbose_img.data);
	} else {
		render_objs(mode, (u8*)silent_img.data);
	}

	if (do_cmds) {
		cmd_setpic(&verbose_img, origin);
		free((u8*)verbose_img.data);
		if (verbose_img.cmap.red);
			free(verbose_img.cmap.red);
	}	
	return 0;
}

int do_config(unsigned char origin)
{
	if (!config_file) {
		printerr("No config file.\n");
		return -1;
	}
		
	/* If the user specified invalid values for the text field - correct it.
	 * Also setup default values (text field coverting the whole screen). */
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

void vt_cursor_disable(int fd)
{
	write(fd, "\e[?25l\e[?1c",11);
}

void vt_cursor_enable(int fd)
{
	write(fd, "\e[?25h\e[?0c",11);
}

int open_fb()
{
	char dev[16];
	int c;
	
	sprintf(dev, "/dev/fb%d", arg_fb);
	if ((c = open(dev, O_RDWR)) == -1) {
		sprintf(dev, "/dev/fb/%d", arg_fb);
		if ((c = open(dev, O_RDWR)) == -1) {
			printerr("Failed to open /dev/fb%d or /dev/fb/%d.\n", arg_fb, arg_fb);
			return 0;
		}
	}

	return c;
}

int open_tty(int tty)
{
	char dev[16];
	int c;

	sprintf(dev, "/dev/tty%d", tty);
	if ((c = open(dev, O_RDWR)) == -1) {
		sprintf(dev, "/dev/vc/%d", tty);
		if ((c = open(dev, O_RDWR)) == -1) {
			printerr("Failed to open /dev/tty%d or /dev/vc/%d\n", tty, tty);
			return 0;
		}
	}

	return c;
}

int tty_set_silent(int tty, int fd)
{
	struct termios w;
	
	tcgetattr(fd, &w);
	w.c_lflag &= ~(ICANON|ECHO);
	w.c_cc[VTIME] = 0;
	w.c_cc[VMIN] = 1;
	tcsetattr(fd, TCSANOW, &w);
	vt_cursor_disable(fd);
	ioctl(fd, VT_ACTIVATE, tty);
	ioctl(fd, VT_WAITACTIVE, tty);

	return 0;
}

int tty_unset_silent(int fd)
{
	struct termios w;
	
	tcgetattr(fd, &w);
	w.c_lflag &= (ICANON|ECHO);
	w.c_cc[VTIME] = 0;
	w.c_cc[VMIN] = 1;
	tcsetattr(fd, TCSANOW, &w);
	vt_cursor_enable(fd);

	return 0;
}
