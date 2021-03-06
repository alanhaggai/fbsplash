/*
 * fbcon_decor.c - Functions for handling communication with the kernel
 *
 * Copyright (C) 2004-2007 Michal Januszewski <spock@gentoo.org>
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
#include <errno.h>

#include "common.h"
#include "render.h"
#include "fbcon_decor.h"

#ifdef CONFIG_FBCON_DECOR
int fd_fbcondecor = -1;

int fbcon_decor_open(bool create)
{
	int c;
	c = open(FBCON_DECOR_DEV, O_RDWR);

	if (c == -1 && create) {
		if (!dev_create(FBCON_DECOR_DEV, PATH_SYS "/class/misc/fbcondecor/dev"))
			c = open(FBCON_DECOR_DEV, O_RDWR);
	}

	if (c == -1) {
		c = open(PATH_DEV"/fbsplash", O_RDWR);
		if (c == -1 && create) {
			if (!dev_create(PATH_DEV"/fbsplash", PATH_SYS "/class/misc/fbsplash/dev"))
				c = open(PATH_DEV"/fbsplash", O_RDWR);
		}
	}

	return c;
}

int fbcon_decor_setstate(unsigned char origin, int vc, unsigned int state)
{
	struct fbcon_decor_iowrapper wrapper = {
		.vc = vc,
		.origin = origin,
		.data = &state,
	};

	if (ioctl(fd_fbcondecor, FBIOCONDECOR_SETSTATE, &wrapper)) {
		iprint(MSG_ERROR, "FBIOCONDECOR_SETSTATE failed, error code %d.\n", errno);
		return -1;
	}
	return 0;
}

int fbcon_decor_getstate(unsigned char origin, int vc)
{
	int i;

	struct fbcon_decor_iowrapper wrapper = {
		.vc = vc,
		.origin = FBCON_DECOR_IO_ORIG_USER,
		.data = &i,
	};

	ioctl(fd_fbcondecor, FBIOCONDECOR_GETSTATE, &wrapper);
	return i;
}

int fbcon_decor_setpic(unsigned char origin, int vc, stheme_t *theme)
{
	struct fbcon_decor_iowrapper wrapper = {
		.vc = vc,
		.origin = origin,
		.data = &theme->verbose_img,
	};

	if (!(theme->modes & FBSPL_MODE_VERBOSE))
		return -1;

	invalidate_all(theme);
	render_objs(theme, (u8*)theme->verbose_img.data, FBSPL_MODE_VERBOSE, true);

	if (ioctl(fd_fbcondecor, FBIOCONDECOR_SETPIC, &wrapper)) {
		iprint(MSG_ERROR, "FBIOCONDECOR_SETPIC failed, error code %d.\n", errno);
		iprint(MSG_ERROR, "Hint: are you calling 'setpic' for the current virtual console?\n");
		return -1;
	}
	return 0;
}

int fbcon_decor_setcfg(unsigned char origin, int vc, stheme_t *theme)
{
	struct vc_decor vc_cfg;
	struct fbcon_decor_iowrapper wrapper = {
		.vc = vc,
		.origin = origin,
		.data = &vc_cfg,
	};

	int err = cfg_check_sanity(theme, 'v');
	if (err)
		return err;

	vc_cfg.tx = theme->tx;
	vc_cfg.ty = theme->ty;
	vc_cfg.twidth = theme->tw;
	vc_cfg.theight = theme->th;
	vc_cfg.bg_color = theme->bg_color;
	vc_cfg.theme = config.theme;

	if (ioctl(fd_fbcondecor, FBIOCONDECOR_SETCFG, &wrapper)) {
		iprint(MSG_ERROR, "FBIOCONDECOR_SETCFG failed, error code %d.\n", errno);
		return -1;
	}
	return 0;
}

int fbcon_decor_getcfg(int vc)
{
	int err = 0;
	struct vc_decor vc_cfg;
	struct fbcon_decor_iowrapper wrapper = {
		.vc = vc,
		.origin = FBCON_DECOR_IO_ORIG_USER,
		.data = &vc_cfg,
	};

	vc_cfg.theme = malloc(FBCON_DECOR_THEME_LEN);
	if (!vc_cfg.theme)
		return -1;

	if (ioctl(fd_fbcondecor, FBIOCONDECOR_GETCFG, &wrapper)) {
		iprint(MSG_ERROR, "FBIOCONDECOR_GETCFG failed, error code %d.\n", errno);
		err = -2;
		goto out;
	}

	if (vc_cfg.theme[0] == 0) {
		strcpy(vc_cfg.theme, "<none>");
	}

	printf("Fbcon decorations config on console %d:\n", vc);
	printf("tx:       %d\n", vc_cfg.tx);
	printf("ty:       %d\n", vc_cfg.ty);
	printf("twidth:	  %d\n", vc_cfg.twidth);
	printf("theight:  %d\n", vc_cfg.theight);
	printf("bg_color: %d\n", vc_cfg.bg_color);
	printf("theme:    %s\n", vc_cfg.theme);

out:
	free(vc_cfg.theme);
	return err;
}

#endif /* CONFIG_FBCON_DECOR */

