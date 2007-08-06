/*
 * splash.c - The core of splash_util
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
	{ "fb",		required_argument, NULL, 0x100 },
	{ "vc",		required_argument, NULL, 0x101 },
	{ "cmd",	required_argument, NULL, 0x102 },
	{ "theme",	required_argument, NULL, 0x103 },
	{ "mode",	required_argument, NULL, 0x104 },
	{ "progress",required_argument, NULL, 0x105 },
	{ "tty",	required_argument, NULL, 0x106 },
	{ "kdgraphics", no_argument, NULL, 0x108 },
#ifdef CONFIG_TTF
	{ "mesg",	required_argument, NULL, 0x109 },
#endif
	{ "pidfile",required_argument, NULL, 0x10a },
	{ "minstances", no_argument, NULL, 0x10b },
	{ "effects", required_argument, NULL, 0x10c },
	{ "type", required_argument, NULL, 0x10d },
	{ "daemon",	no_argument, NULL, 'd'},
	{ "help",	no_argument, NULL, 'h'},
	{ "verbose", no_argument, NULL, 'v'},
	{ "quiet",  no_argument, NULL, 'q'},
};

struct cmd {
	char *name;
	int value;
};

struct cmd cmds[] = {
	{ "on",			on },
	{ "off",		off },
	{ "setcfg",		setcfg },
	{ "getcfg",		getcfg },
	{ "setpic",		setpic },
#ifdef CONFIG_DEPRECATED
	{ "paint",		paint },
	{ "repaint",	repaint },
#endif
	{ "setmode",	setmode },
	{ "getmode",	getmode },
	{ "getstate",	getstate },
	{ "getres",		getres },
};

void usage(void)
{
	printf(
"splash_util/splashutils-" PKG_VERSION "\n"
"Usage: splash_util [options] -c <cmd>\n\n"
"Commands:\n"
#ifdef CONFIG_FBSPLASH
"  on       enable splash on a virtual console\n"
"  off      disable splash on a virtual console\n"
"  getstate get splash state on a virtual console\n"
"  setcfg   set splash configuration for a virtual console\n"
"  getcfg   get splash configuration for a virtual console\n"
"  setpic   set splash background picture for a virtual console;\n"
"           note that this command will only have any effect if\n"
"           it's called for the current console\n"
#endif
#ifdef CONFIG_DEPRECATED
"  paint    paint the background picture\n"
"  repaint  paint the whole background picture (full refresh)\n"
#endif
"  setmode  set global splash mode\n"
"  getmode  get global splash mode\n"
"  getres   get the resolution which the silent splash will use\n\n"
"Options:\n"
"  -c, --cmd=CMD       execute command CMD\n"
"  -d, --daemon        start the splash daemon\n"
"  -h, --help          show this help message\n"
"  -t, --theme=THEME   use theme THEME\n"
"      --vc=NUM        use NUMth virtual console [0..n]\n"
"      --tty=NUM       use NUMth tty [1..n]\n"
"      --fb=NUM        use NUMth framebuffer device\n"
"  -m, --mode=(v|s)    use either silent (s) or verbsose (v) mode\n"
"  -p, --progress=NUM  set progress to NUM/65535 * 100%%\n"
"      --kdgraphics    use KD_GRAPHICS mode for the silent splash\n"
"                      when splash_util is running in daemon mode\n"
"  -v, --verbose       display verbose error messages\n"
"  -q, --quiet         don't display any messages\n"
#ifdef CONFIG_TTF
"      --mesg=TEXT     use TEXT as the main splash message\n"
#endif
"      --pidfile=FILE  save the PID of the daemon to FILE\n"
"      --minstances    allow multiple instances of the splash daemon\n"
"      --effects=LIST  a comma-separated list of effects to use;\n"
"                      supported effects: fadein, fadeout\n"
"      --type=TYPE     TYPE can be: bootup, reboot, shutdown\n"
#ifndef CONFIG_FBCON_DECOR
"\nThis version of splashutils has been compiled without support for fbcondecor.\n"
#endif
);
}

int main(int argc, char **argv)
{
	unsigned int c, i;
	int err = 0;
	int arg_vc = -1;
	stheme_t *theme = NULL;

	splash_lib_init(spl_bootup);
	splashr_init(false);

	arg_task = none;
	arg_vc = -1;

	while ((c = getopt_long(argc, argv, "c:t:m:p:e:hdvq", options, NULL)) != EOF) {

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
			splash_acc_theme_set(optarg);
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
				config.reqmode = 's';
			else
				config.reqmode = 'v';
			break;

		case 'p':
		case 0x105:
			config.progress = atoi(optarg);
			break;

		case 0x106:
			arg_vc = atoi(optarg)-1;
			if (arg_vc == -1)
				arg_vc = 0;
			break;

		case 0x108:
			config.kdmode = KD_GRAPHICS;
			break;
#ifdef CONFIG_TTF
		case 0x109:
			if (config.message)
				free(config.message);
			config.message = strdup(optarg);
			break;
#endif
		case 0x10a:
			arg_pidfile = strdup(optarg);
			break;

		case 0x10b:
			config.minstances = true;
			break;

		case 0x10c:
		{
			char *topt;

			while ((topt = strsep(&optarg, ",")) != NULL) {
				if (!strcmp(topt, "fadein"))
					config.effects |= SPL_EFF_FADEIN;
				else if (!strcmp(topt, "fadeout"))
					config.effects |= SPL_EFF_FADEOUT;
			}
			break;
		}

		case 0x10d:
			if (!strcmp(optarg, "reboot"))
				config.type = spl_reboot;
			else if (!strcmp(optarg, "shutdown"))
				config.type = spl_shutdown;
			else
				config.type = spl_bootup;
			break;

		case 'd':
			arg_task = start_daemon;
			break;

		/* Verbosity level adjustment. */
		case 'q':
			config.verbosity = VERB_QUIET;
			break;

		case 'v':
			config.verbosity = VERB_HIGH;
			break;
		}
	}

	if (arg_task == none) {
		usage();
		return 0;
	}

	switch (arg_task) {
	case getres:
	{
		int xres = fbd.var.xres;
		int yres = fbd.var.yres;
		splash_get_res(config.theme, &xres, &yres);
		printf("%dx%d\n", xres, yres);
		return 0;
	}

	/* Only load the theme if it will actually be used. */
	case setpic:
	case setcfg:
	case start_daemon:
#ifdef CONFIG_DEPRECATED
	case paint:
	case repaint:
#endif
		theme = splashr_theme_load();
		if (arg_task != start_daemon && !theme)
			exit(1);
	default:
		break;
	}

	if (arg_task == start_daemon) {
		daemon_start(theme);
		/* we never get here */
	}

	if (arg_task != setmode && arg_vc == -1)
		arg_vc = 0;

	switch (arg_task) {

	case setmode:
		if (config.reqmode == 's') {
			splashr_tty_silent_init();
			splash_set_silent(NULL);
		} else {
			splashr_tty_silent_cleanup();
			splash_set_verbose(0);
		}
		break;

	case getmode:
		printf("Splash mode: %s\n", splash_is_silent() ? "silent" : "verbose");
		break;

#ifdef CONFIG_FBCON_DECOR
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
#endif /* CONFIG_FBCON_DECOR */
#ifdef CONFIG_DEPRECATED
	/* Deprecated. The daemon mode should be used instead. */
	case paint:
		splashr_render_screen(theme, false, false, 's', SPL_EFF_NONE);
		break;

	case repaint:
		splashr_render_screen(theme, true, false, 's', SPL_EFF_NONE);
		break;
#endif /* CONFIG_DEPRECATED */

	default:
		break;
	}

	splashr_theme_free(theme);
	splashr_cleanup();
	splash_lib_cleanup();

	return err;
}
