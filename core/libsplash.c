/*
 * libsplash.c
 *
 * Copyright (c) 2007, Michal Januszewski <spock@gentoo.org>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2.  See the file COPYING in the main directory of this archive for
 * more details.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <linux/kd.h>
#include <linux/vt.h>

#include "splash.h"

static int fd_tty0 = -1;

int splash_config_init(scfg_t *cfg, stype_t type)
{
	cfg->tty_s = TTY_SILENT;
	cfg->tty_v = TTY_VERBOSE;
	cfg->theme = strdup(DEFAULT_THEME);
	cfg->kdmode = KD_TEXT;
	cfg->insane = false;
	cfg->profile = false;
	cfg->vonerr = false;
	cfg->reqmode = 'o';

	switch (type) {
		case reboot:
			cfg->message = strdup(SYSMSG_REBOOT);
			break;

		case shutdown:
			cfg->message = strdup(SYSMSG_SHUTDOWN);
			break;

		case bootup:
		default:
			cfg->message = strdup(SYSMSG_BOOTUP);
			break;
	}

	if (fd_tty0 == -1) {
		fd_tty0 = open("/dev/tty0", O_RDWR);
	}

	return 0;
}

/* Parse the kernel command line to get splash settings. */
int splash_parse_kcmdline(scfg_t *cfg)
{
	FILE *fp;
	char *p, *t, *opt;
	char *buf = malloc(1024);
	int err = 0;

	if (!buf)
		return -1;

	fp = fopen("/proc/cmdline", "r");
	if (!fp)
		goto fail;

	fgets(buf, 1024, fp);
	fclose(fp);

	/* Remove the newline character so that it doesn't get
	 * included in the theme name. */
	if (buf[strlen(buf)-1] == '\n') {
		buf[strlen(buf)-1] = 0;
	}

	/* FIXME: add support for multiple splash= settings */
	t = strstr(buf, "splash=");
	if (!t)
		goto fail;

	t += 7; p = t;
	for (p = t; *p != ' ' && *p != 0; p++);
	*p = 0;

	while ((opt = strsep(&t, ",")) != NULL) {

		if (!strncmp(opt, "tty:", 4)) {
			cfg->tty_v = strtol(opt+4, NULL, 0);
//		} else if (!strncmp(opt, "fadein", 6)) {
//			arg_effects |= EFF_FADEIN;
		} else if (!strncmp(opt, "verbose", 7)) {
			cfg->reqmode = 'v';
		} else if (!strncmp(opt, "silent", 6)) {
			cfg->reqmode = 's';
		} else if (!strncmp(opt, "off", 3)) {
			cfg->reqmode = 'o';
		} else if (!strncmp(opt, "insane", 6)) {
			cfg->insane = true;
		} else if (!strncmp(opt, "theme:", 6)) {
			if (cfg->theme)
				free(cfg->theme);
			cfg->theme = strdup(opt+6);
		} else if (!strncmp(opt, "kdgraphics", 10)) {
			cfg->kdmode = KD_GRAPHICS;
		} else if (!strncmp(opt, "profile", 7)) {
			cfg->profile = 1;
		}
	}

out:
	free(buf);
	return err;

fail:
	err = -1;
	goto out;
}

/* Switch to verbose mode */
int splash_verbose(scfg_t *cfg)
{
	if (fd_tty0 == -1)
		return -1;
	return ioctl(fd_tty0, VT_ACTIVATE, cfg->tty_v);
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

