/*
 * fbcon_decor_ctl.c -- Fbcon Decor control application
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
#include <linux/kd.h>
#include <linux/vt.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <getopt.h>
#include <unistd.h>

#include "common.h"
#include "render.h"
#include "fbcon_decor.h"

static struct option options[] = {
	{ "vc",		required_argument, NULL, 0x100 },
	{ "cmd",	required_argument, NULL, 0x101 },
	{ "theme",	required_argument, NULL, 0x102 },
	{ "tty",	required_argument, NULL, 0x103 },
	{ "help",	no_argument, NULL, 'h'},
	{ "verbose", no_argument, NULL, 'v'},
	{ "quiet",  no_argument, NULL, 'q'},
};

enum { none, setpic, on, off, setcfg, getcfg, getstate } arg_task;

struct cmd {
	char *name;
	int value;
};

static struct cmd cmds[] = {
	{ "on",			on },
	{ "off",		off },
	{ "setcfg",		setcfg },
	{ "getcfg",		getcfg },
	{ "setpic",		setpic },
	{ "getstate",	getstate },
};

static void usage()
{
	printf(
"fbcondecor_ctl/splashutils-" PACKAGE_VERSION "\n"
"Usage: fbcondecor_ctl [options] -c <cmd>\n\n"
"Commands:\n"
"  on       enable fbcondecor on a virtual console\n"
"  off      disable fbcondecor on a virtual console\n"
"  getstate get fbcondecor state on a virtual console\n"
"  setcfg   set fbcondecor configuration for a virtual console\n"
"  getcfg   get fbcondecor configuration for a virtual console\n"
"  setpic   set fbcondecor background picture for a virtual console;\n"
"           note that this command will only have any effect if\n"
"           it's called for the current console\n"
"Options:\n"
"  -c, --cmd=CMD       execute command CMD\n"
"  -h, --help          show this help message\n"
"  -t, --theme=THEME   use theme THEME\n"
"      --vc=NUM        use NUMth virtual console [0..n]\n"
"      --tty=NUM       use NUMth tty [1..n]\n"
"  -v, --verbose       display verbose error messages\n"
"  -q, --quiet         don't display any messages\n"
);
}

int fbcondecor_main(int argc, char **argv)
{
	unsigned int c, i;
	int err = 0;
	int arg_vc = -1;
	stheme_t *theme = NULL;

	fbsplash_lib_init(fbspl_bootup);
	fbsplashr_init(false);

	fd_fbcondecor = fbcon_decor_open(false);
	if (fd_fbcondecor == -1) {
		iprint(MSG_ERROR, "Failed to open the fbcon_decor control device.\n");
		exit(1);
	}

	arg_task = none;
	arg_vc = -1;

	while ((c = getopt_long(argc, argv, "c:t:hvq", options, NULL)) != EOF) {

		switch (c) {
		case 'h':
			usage();
			return 0;

		case 0x100:
			arg_vc = atoi(optarg);
			break;

		case 0x102:
		case 't':
			fbsplash_acc_theme_set(optarg);
			break;

		case 0x101:
		case 'c':
			for (i = 0; i < sizeof(cmds) / sizeof(struct cmd); i++) {
				if (!strcmp(optarg, cmds[i].name)) {
					arg_task = cmds[i].value;
					break;
				}
			}
			break;

		case 0x103:
			arg_vc = atoi(optarg)-1;
			if (arg_vc == -1)
				arg_vc = 0;
			break;

		/* Verbosity level adjustment. */
		case 'q':
			config.verbosity = FBSPL_VERB_QUIET;
			break;

		case 'v':
			config.verbosity = FBSPL_VERB_HIGH;
			break;
		}
	}

	if (arg_task == none) {
		usage();
		return 0;
	}

	switch (arg_task) {
	/* Only load the theme if it will actually be used. */
	case setpic:
	case setcfg:
		theme = fbsplashr_theme_load();
	default:
		break;
	}

	if (arg_vc == -1)
		arg_vc = 0;

	switch (arg_task) {

	case on:
		err = fbcon_decor_setstate(FBCON_DECOR_IO_ORIG_USER, arg_vc, 1);
		break;

	case off:
		err = fbcon_decor_setstate(FBCON_DECOR_IO_ORIG_USER, arg_vc, 0);
		break;

	case setpic:
	{
		struct vt_stat stat;

		if (ioctl(fd_tty0, VT_GETSTATE, &stat) != -1) {
			if (arg_vc != stat.v_active - 1)
				goto setpic_out;
		}

		err = fbcon_decor_setpic(FBCON_DECOR_IO_ORIG_USER, arg_vc, theme);
setpic_out:	break;
	}

	case getcfg:
		err = fbcon_decor_getcfg(arg_vc);
		break;

	case setcfg:
		err = fbcon_decor_setcfg(FBCON_DECOR_IO_ORIG_USER, arg_vc, theme);
		break;

	case getstate:
	{
		printf("Fbcon decorations state on console %d: %s\n", arg_vc,
				(fbcon_decor_getstate(FBCON_DECOR_IO_ORIG_USER, arg_vc)) ? "on" : "off");
		break;
	}
	default:
		break;
	}

	fbsplashr_theme_free(theme);

	close(fd_fbcondecor);
	fbsplashr_cleanup();
	fbsplash_lib_cleanup();

	return 0;
}

#ifndef UNIFIED_BUILD
int main(int argc, char **argv)
{
	return fbcondecor_main(argc, argv);
}
#endif /* UNIFIED_BUILD */
