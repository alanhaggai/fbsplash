/*
 * util.c - The core of splash_util
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

#include "util.h"

struct option options[] = {
	{ "cmd",	required_argument, NULL, 0x102 },
	{ "mode",	required_argument, NULL, 0x100 },
#ifdef CONFIG_DEPRECATED
	{ "theme",	required_argument, NULL, 0x103 },
	{ "progress",required_argument, NULL, 0x105 },
	{ "type", required_argument, NULL, 0x10d },
#ifdef CONFIG_TTF
	{ "mesg",	required_argument, NULL, 0x109 },
#endif
#endif
	{ "help",	no_argument, NULL, 'h'},
	{ "verbose", no_argument, NULL, 'v'},
	{ "quiet",  no_argument, NULL, 'q'},
};

enum { none, getres, paint, setmode, getmode, repaint } arg_task;

struct cmd {
	char *name;
	int value;
};

struct cmd cmds[] = {
#ifdef CONFIG_DEPRECATED
	{ "paint",		paint },
	{ "repaint",	repaint },
#endif
	{ "setmode",	setmode },
	{ "getmode",	getmode },
	{ "getres",		getres },
};

void usage(void)
{
	printf(
"splash_util/splashutils-" PKG_VERSION "\n"
"Usage: splash_util [options] -c <cmd>\n\n"
"Commands:\n"
#ifdef CONFIG_DEPRECATED
"  paint    paint the background picture\n"
"  repaint  paint the whole background picture (full refresh)\n"
#endif
"  setmode  set global splash mode\n"
"  getmode  get global splash mode\n"
"  getres   get the resolution which the silent splash will use\n\n"
"Options:\n"
"  -c, --cmd=CMD       execute command CMD\n"
"  -v, --verbose       display verbose error messages\n"
"  -q, --quiet         don't display any messages\n"
"  -h, --help          show this help message\n"
"  -t, --theme=THEME   use theme THEME\n"
"  -m, --mode=(v|s)    set silent (s) or verbsose (v) mode\n"
#ifdef CONFIG_DEPRECATED
"  -p, --progress=NUM  set progress to NUM/65535 * 100%%\n"
#ifdef CONFIG_TTF
"      --mesg=TEXT     use TEXT as the main splash message\n"
#endif
"      --type=TYPE     TYPE can be: bootup, reboot, shutdown\n"
#endif
);
}

int main(int argc, char **argv)
{
	unsigned int c, i;
	int err = 0;
	int arg_vc = -1;
	stheme_t *theme = NULL;

	fbsplash_lib_init(fbspl_bootup);
	fbsplashr_init(false);

	arg_task = none;
	arg_vc = -1;

	config.reqmode = FBSPL_MODE_SILENT;

	while ((c = getopt_long(argc, argv, "c:t:m:p:e:hdvq", options, NULL)) != EOF) {

		switch (c) {

		case 'h':
			usage();
			return 0;

		case 0x100:
		case 'm':
			if (optarg[0] == 'v')
				config.reqmode = FBSPL_MODE_VERBOSE;
			break;

		case 0x103:
		case 't':
			fbsplash_acc_theme_set(optarg);
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

		case 'p':
		case 0x105:
			config.progress = atoi(optarg);
			break;

#ifdef CONFIG_TTF
		case 0x109:
			if (config.message)
				free(config.message);
			config.message = strdup(optarg);
			break;
#endif
		case 0x10d:
			if (!strcmp(optarg, "reboot"))
				config.type = fbspl_reboot;
			else if (!strcmp(optarg, "shutdown"))
				config.type = fbspl_shutdown;
			else
				config.type = fbspl_bootup;
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
#ifdef CONFIG_DEPRECATED
	case paint:
	case repaint:
#endif
		theme = fbsplashr_theme_load();
		if (!theme)
			exit(1);
	default:
		break;
	}

	if (arg_task != setmode && arg_vc == -1)
		arg_vc = 0;

	switch (arg_task) {

	case getres:
	{
		int xres = fbd.var.xres;
		int yres = fbd.var.yres;
		fbsplash_get_res(config.theme, &xres, &yres);
		printf("%dx%d\n", xres, yres);
		return 0;
	}

	case setmode:
		if (config.reqmode & FBSPL_MODE_SILENT) {
			fbsplashr_tty_silent_init(true);
			fbsplash_set_silent(NULL);
		} else {
			fbsplashr_tty_silent_cleanup();
			fbsplash_set_verbose(0);
		}
		break;

	case getmode:
		printf("%s\n", fbsplash_is_silent() ? "silent" : "verbose");
		break;

#ifdef CONFIG_DEPRECATED
	/* Deprecated. The daemon mode should be used instead. */
	case paint:
		fbsplashr_render_screen(theme, false, false, FBSPL_EFF_NONE);
		break;

	case repaint:
		fbsplashr_render_screen(theme, true, false, FBSPL_EFF_NONE);
		break;
#endif /* CONFIG_DEPRECATED */

	default:
		break;
	}

	fbsplashr_theme_free(theme);
	fbsplashr_cleanup();
	fbsplash_lib_cleanup();

	return err;
}
