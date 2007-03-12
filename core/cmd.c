/*
 * splash_cmd.c - Functions for handling communication with the kernel
 *
 * Copyright (C) 2004-2005 Michal Januszewski <spock@gentoo.org>
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
#include "splash.h"

#ifdef CONFIG_FBSPLASH

int cmd_setstate(unsigned int state, unsigned char origin)
{
	int fd, err = 0;
	struct fb_splash_iowrapper wrapper = {
		.vc = arg_vc,
		.origin = origin,
		.data = &state,
	};

	fd = open(SPLASH_DEV, O_WRONLY);
	if (fd == -1) {
		iprint(MSG_ERROR, "Can't open %s\n", SPLASH_DEV);
		return -1;
	}

	if (ioctl(fd, FBIOSPLASH_SETSTATE, &wrapper)) {
		iprint(MSG_ERROR, "FBIOSPLASH_SETSTATE failed, error code %d.\n", errno);
		err = -2;
		goto out;
	}

out:
	close(fd);
	return err;
}

int cmd_setpic(struct fb_image *img, unsigned char origin)
{
	int fd, err = 0;
	struct fb_splash_iowrapper wrapper = {
		.vc = arg_vc,
		.origin = origin,
		.data = img,
	};

	fd = open(SPLASH_DEV, O_WRONLY);
	if (fd == -1) {
		iprint(MSG_ERROR, "Can't open %s\n", SPLASH_DEV);
		return -1;
	}

	if (ioctl(fd, FBIOSPLASH_SETPIC, &wrapper)) {
		iprint(MSG_ERROR, "FBIOSPLASH_SETPIC failed, error code %d.\n", errno);
		iprint(MSG_ERROR, "Hint: are you calling 'setpic' for the current virtual console?\n");
		err = -2;
		goto out;
	}
out:
	close(fd);
	return err;
}

int cmd_setcfg(unsigned char origin)
{
	int fd, err = 0;
	struct vc_splash vc_cfg;
	struct fb_splash_iowrapper wrapper = {
		.vc = arg_vc,
		.origin = origin,
		.data = &vc_cfg,
	};

	fd = open(SPLASH_DEV, O_WRONLY);
	if (fd == -1) {
		iprint(MSG_ERROR, "Can't open %s\n", SPLASH_DEV);
		return -1;
	}

	vc_cfg.tx = cf.tx;
	vc_cfg.ty = cf.ty;
	vc_cfg.twidth = cf.tw;
	vc_cfg.theight = cf.th;
	vc_cfg.bg_color = cf.bg_color;
	vc_cfg.theme = arg_theme;

	if (ioctl(fd, FBIOSPLASH_SETCFG, &wrapper)) {
		iprint(MSG_ERROR, "FBIOSPLASH_SETCFG failed, error code %d.\n", errno);
		err = -2;
		goto out;
	}
out:
	close(fd);
	return err;
}

int cmd_getcfg()
{
	int fd, err = 0;
	struct vc_splash vc_cfg;
	struct fb_splash_iowrapper wrapper = {
		.vc = arg_vc,
		.origin = FB_SPLASH_IO_ORIG_USER,
		.data = &vc_cfg,
	};

	vc_cfg.theme = malloc(FB_SPLASH_THEME_LEN);
	if (!vc_cfg.theme)
		return -1;

	fd = open(SPLASH_DEV, O_WRONLY);
	if (fd == -1) {
		iprint(MSG_ERROR, "Can't open %s\n", SPLASH_DEV);
		err = -1;
		goto out;
	}

	if (ioctl(fd, FBIOSPLASH_GETCFG, &wrapper)) {
		iprint(MSG_ERROR, "FBIOSPLASH_GETCFG failed, error code %d.\n", errno);
		err = -2;
		close(fd);
		goto out;
	}
	close(fd);

	if (vc_cfg.theme[0] == 0) {
		strcpy(vc_cfg.theme, "<none>");
	}

	printf("Splash config on console %d:\n", arg_vc);
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

#endif /* CONFIG_FBSPLASH */

