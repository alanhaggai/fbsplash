/*
 * common.c - miscellaneous functions used by both the kernel helper and
 *            user utilities.
 *
 * Copyright (C) 2004-2007, Michal Januszewski <spock@gentoo.org>
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
#include <sys/types.h>
#include <sys/mman.h>
#include <dirent.h>
#include <errno.h>

#include "util.h"

enum TASK	arg_task = none;
int			arg_fb = 0;
char		*arg_pidfile = NULL;
scfg_t		*cfg;

int dev_create(char *fn, char *sys)
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

int fb_open(int fb, bool create)
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

void* fb_mmap(int fb)
{
	return mmap(NULL, fbd.fix.line_length * fbd.var.yres,
			PROT_WRITE | PROT_READ, MAP_SHARED, fb,
			fbd.var.yoffset * fbd.fix.line_length);
}

void fb_unmap(u8 *fb)
{
	munmap(fb, fbd.fix.line_length * fbd.var.yres);
}

int fb_get_settings(int fb)
{
	if (ioctl(fb, FBIOGET_VSCREENINFO, &fbd.var) == -1) {
		iprint(MSG_ERROR, "Failed to get fb_var info. (errno=%d)\n", errno);
		return 2;
	}

	if (ioctl(fb, FBIOGET_FSCREENINFO, &fbd.fix) == -1) {
		iprint(MSG_ERROR, "Failed to get fb_fix info. (errno=%d)\n", errno);
		return 3;
	}

	fbd.bytespp = (fbd.var.bits_per_pixel + 7) >> 3;

	/* Check if optimized code can be used. We use special optimizations for
	 * 24/32bpp modes in which all color components have a length of 8 bits. */
	if (fbd.bytespp < 3 || fbd.var.blue.length != 8 || fbd.var.green.length != 8 || fbd.var.red.length != 8) {
		fbd.opt = false;

		if (fbd.fix.visual == FB_VISUAL_DIRECTCOLOR) {
			fbd.blen = fbd.glen = fbd.rlen = min(min(fbd.var.red.length, fbd.var.green.length),fbd.var.blue.length);
		} else {
			fbd.rlen = fbd.var.red.length;
			fbd.glen = fbd.var.green.length;
			fbd.blen = fbd.var.blue.length;
		}
	} else {
		fbd.opt = true;

		/* Compute component offsets (ie. indexes in an array of u8's) */
		fbd.ro = (fbd.var.red.offset >> 3);
		fbd.go = (fbd.var.green.offset >> 3);
		fbd.bo = (fbd.var.blue.offset >> 3);

		if (endianess == big) {
			fbd.ro = fbd.bytespp - 1 - fbd.ro;
			fbd.go = fbd.bytespp - 1 - fbd.go;
			fbd.bo = fbd.bytespp - 1 - fbd.bo;
		}
	}

	return 0;
}

int cfg_check_sanity(stheme_t *theme, u8 mode)
{
	char *pic;

	/* Verbose mode needs a config file for the exact video mode that is
	 * currently in use. */
	if (mode == 'v' && (fbd.var.xres != theme->xres || fbd.var.yres != theme->yres))
		return -1;

	/* If the user specified invalid values for the text field - correct it.
	 * Also setup default values (text field covering the whole screen). */
	if (theme->tx > fbd.var.xres)
		theme->tx = 0;

	if (theme->ty > fbd.var.yres)
		theme->ty = 0;

	if (theme->tw > fbd.var.xres || theme->tw == 0)
		theme->tw = fbd.var.xres;

	if (theme->th > fbd.var.yres || theme->th == 0)
		theme->th = fbd.var.yres;

	if (fbd.var.bits_per_pixel == 8) {
		pic = (mode == 'v') ? theme->pic256 : theme->silentpic256;

		if (!pic) {
			iprint(MSG_ERROR, "No 8bpp %s picture specified in the theme.\n", (mode == 'v') ? "verbose" : "silent");
			return -2;
		}
	} else {
		pic = (mode == 'v') ? theme->pic : theme->silentpic;

		if (!pic) {
			iprint(MSG_ERROR, "No %s picture specified in the theme.\n", (mode == 'v') ? "verbose" : "silent");
			return -2;
		}
	}

	return 0;
}

int tty_open(int tty)
{
	char dev[64];
	int c;
#ifdef TARGET_KERNEL
	char sys[128];
	bool first = true;
tryopen:
#endif

	snprintf(dev, 64, PATH_DEV "/tty%d", tty);
	if ((c = open(dev, O_RDWR | O_NOCTTY)) == -1) {
		snprintf(dev, 64, PATH_DEV "/vc/%d", tty);
		c = open(dev, O_RDWR | O_NOCTTY);
	}

#ifdef TARGET_KERNEL
	if (c == -1 && first) {
		first = false;
		snprintf(dev, 64, PATH_DEV "/tty%d", tty);
		snprintf(sys, 128, PATH_SYS "/class/tty/tty%d/dev", tty);
		if (!dev_create(dev, sys))
			goto tryopen;
	}
#endif

	return c;
}


