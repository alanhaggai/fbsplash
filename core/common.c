/*
 * common.c - miscellaneous functions used by both the kernel helper and
 *            user utilities.
 *
 * Copyright (C) 2004-2005, Michal Januszewski <spock@gentoo.org>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "util.h"

struct fb_var_screeninfo   fb_var;
struct fb_fix_screeninfo   fb_fix;

char *config_file = NULL;

enum TASK	arg_task = none;
int			arg_fb = 0;
int			arg_vc = 0;
char		*arg_pidfile = NULL;
#ifndef TARGET_KERNEL
char		*arg_export = NULL;
#endif

int bytespp = 4;		/* bytes per pixel */
u8 fb_opt = 0;			/* can we use optimized 24/32bpp routines? */
u8 fb_ro, fb_go, fb_bo;	/* red, green, blue offset */
u8 fb_rlen, fb_glen, fb_blen;	/* red, green, blue length */

struct fb_image pic;
char *pic_file = NULL;

sendian_t	endianess;
scfg_t		*config;

void detect_endianess(sendian_t *end)
{
	u16 t = 0x1122;

	if (*(u8*)&t == 0x22) {
		*end = little;
	} else {
		*end = big;
	}
}

int get_fb_settings(int fb_num)
{
	int fb;
#ifdef TARGET_KERNEL
	bool create = true;
#else
	bool create = false;
#endif

	fb = open_fb(fb_num, create);
	if (fb == -1) {
		iprint(MSG_ERROR, "Failed to open fb%d.\n", fb_num);
		return -1;
	}

	if (ioctl(fb, FBIOGET_VSCREENINFO, &fb_var) == -1) {
		iprint(MSG_ERROR, "Failed to get fb_var info.\n");
		return 2;
	}

	if (ioctl(fb, FBIOGET_FSCREENINFO, &fb_fix) == -1) {
		iprint(MSG_ERROR, "Failed to get fb_fix info.\n");
		return 3;
	}
	close(fb);

	bytespp = (fb_var.bits_per_pixel + 7) >> 3;

	/* Check if optimized code can be used. We use special optimizations for
	 * 24/32bpp modes in which all color components have a length of 8 bits. */
	if (bytespp < 3 || fb_var.blue.length != 8 || fb_var.green.length != 8 || fb_var.red.length != 8) {
		fb_opt = 0;

		if (fb_fix.visual == FB_VISUAL_DIRECTCOLOR) {
			fb_blen = fb_glen = fb_rlen = min(min(fb_var.red.length,fb_var.green.length),fb_var.blue.length);
		} else {
			fb_rlen = fb_var.red.length;
			fb_glen = fb_var.green.length;
			fb_blen = fb_var.blue.length;
		}
	} else {
		fb_opt = 1;

		/* Compute component offsets (ie. indexes in an array of u8's) */
		fb_ro = (fb_var.red.offset >> 3);
		fb_go = (fb_var.green.offset >> 3);
		fb_bo = (fb_var.blue.offset >> 3);

		if (endianess == big) {
			fb_ro = bytespp - 1 - fb_ro;
			fb_go = bytespp - 1 - fb_go;
			fb_bo = bytespp - 1 - fb_bo;
		}
	}

	return 0;
}

char *get_filepath(char *path)
{
	char buf[512];

	if (path[0] == '/')
		return strdup(path);

	snprintf(buf, 512, "%s/%s/%s", THEME_DIR, config->theme, path);
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

#if (defined(CONFIG_TTF) && !defined(TARGET_KERNEL)) || (defined(CONFIG_TTF_KERNEL) && defined(TARGET_KERNEL))
	load_fonts();
#endif

	if (mode == 'v') {
		render_objs((u8*)verbose_img.data, NULL, mode, origin);
	} else {
		render_objs((u8*)silent_img.data, NULL, mode, origin);
	}

#ifdef CONFIG_FBSPLASH
	/* Setting the background picture only makes sense in the verbose mode. */
	if (do_cmds && mode == 'v') {
		cmd_setpic(&verbose_img, origin);
		free((u8*)verbose_img.data);
		if (verbose_img.cmap.red);
			free(verbose_img.cmap.red);
	}
#endif
	return 0;
}

int cfg_check_sanity(u8 mode)
{
	char *pic;

	if (!config_file) {
		iprint(MSG_CRITICAL, "No config file.\n");
		return -1;
	}

	/* If the user specified invalid values for the text field - correct it.
	 * Also setup default values (text field covering the whole screen). */
	if (cf.tx > fb_var.xres)
		cf.tx = 0;

	if (cf.ty > fb_var.yres)
		cf.ty = 0;

	if (cf.tw > fb_var.xres || cf.tw == 0)
		cf.tw = fb_var.xres;

	if (cf.th > fb_var.yres || cf.th == 0)
		cf.th = fb_var.yres;

	if (fb_var.bits_per_pixel == 8) {
		pic = (mode == 'v') ? cf_pic256 : cf_silentpic256;

		if (!pic) {
			iprint(MSG_ERROR, "No 8bpp %s picture specified in the theme.\n", (mode == 'v') ? "verbose" : "silent");
			return -2;
		}
	} else {
		pic = (mode == 'v') ? cf_pic : cf_silentpic;

		if (!pic) {
			iprint(MSG_ERROR, "No %s picture specified in the theme.\n", (mode == 'v') ? "verbose" : "silent");
			return -2;
		}
	}

	return 0;
}

static int dev_create(char *fn, char *sys)
{
	char buf[256];
	unsigned int major = 0, minor = 0;
	int fd;
	int res;

//	if (!access(fn, W_OK | R_OK))
//		return 0;

	fd = open(sys, O_RDONLY);

	if (fd == -1) {
		return 1;
	}

	read(fd, buf, 256);
	close(fd);

	buf[255] = 0;

	sscanf(buf, "%u:%u", &major, &minor);

	if (major == 0) {
		return 2;
	}

	res = mknod(fn, 0600 | S_IFCHR, makedev(major, minor));

	return res;
}

#ifdef CONFIG_FBSPLASH
int open_fbsplash(bool create)
{
	int c;
	c = open(SPLASH_DEV, O_RDWR);

	if (c == -1 && create) {
		if (!dev_create(SPLASH_DEV, PATH_SYS "/class/misc/fbsplash/dev"))
			c = open(SPLASH_DEV, O_RDWR);
	}

	return c;
}
#endif

int open_fb(int fb, bool create)
{
	char dev[64];
	char sys[128];
	int c;
	bool first = true;

tryopen:
	snprintf(dev, 64, PATH_DEV "/fb%d", fb);
	if ((c = open(dev, O_RDWR)) == -1) {
		snprintf(dev, 64, PATH_DEV "/fb/%d", fb);
		c = open(dev, O_RDWR);
	}

	if (c == -1 && create && first) {
		first = false;
		snprintf(dev, 64, PATH_DEV "/fb%d", fb);
		snprintf(sys, 128, PATH_SYS "/class/graphics/fb%d/dev", fb);
		if (!dev_create(dev, sys))
			goto tryopen;
	}

	return c;
}

int open_tty(int tty, bool create)
{
	char dev[64];
	char sys[128];
	int c;
	bool first = true;

tryopen:
	snprintf(dev, 64, PATH_DEV "/tty%d", tty);
	if ((c = open(dev, O_RDWR)) == -1) {
		snprintf(dev, 64, PATH_DEV "/vc/%d", tty);
		c = open(dev, O_RDWR);
	}

	if (c == -1 && create && first) {
		first = false;
		snprintf(dev, 64, PATH_DEV "/tty%d", tty);
		snprintf(sys, 128, PATH_SYS "/class/tty/tty%d/dev", tty);
		if (!dev_create(dev, sys))
			goto tryopen;
	}

	return c;
}

void vt_cursor_disable(int fd)
{
	write(fd, "\e[?25l\e[?1c",11);
}

void vt_cursor_enable(int fd)
{
	write(fd, "\e[?25h\e[?0c",11);
}

/* FIXME: merge this with the functions from the daemon code. */

int tty_silent_set(int tty, int fd)
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

int tty_silent_unset(int fd)
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


