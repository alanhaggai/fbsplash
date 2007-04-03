/*
 * splash.c - The core of splash_util
 *
 * Copyright (C) 2004-2006 Michal Januszewski <spock@gentoo.org>
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
	{ "export",	required_argument, NULL, 0x107 },
	{ "kdgraphics", no_argument, NULL, 0x108 },
#ifdef CONFIG_TTF
	{ "mesg",	required_argument, NULL, 0x109 },
#endif
	{ "pidfile",required_argument, NULL, 0x10a },
	{ "minstances", no_argument, NULL, 0x10b },
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
	{ "paint",		paint },
	{ "repaint",	repaint },
	{ "setmode",	setmode },
	{ "getmode",	getmode },
	{ "getstate",	getstate },
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
"  paint    paint the background picture\n"
"  repaint  paint the whole background picture (full refresh)\n"
"  setmode  set global splash mode\n"
"  getmode  get global splash mode\n\n"
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
"  -e, --export=FILE   export the silent background image to a file\n"
"                      this option is only used when splash_util is\n"
"                      running in daemon mode\n"
"      --kdgraphics    use KD_GRAPHICS mode for the silent splash\n"
"                      when splash_util is running in daemon mode\n"
"  -v, --verbose       display verbose error messages\n"
"  -q, --quiet         don't display any messages\n"
#ifdef CONFIG_TTF
"      --mesg=TEXT     use TEXT as the main splash message\n"
#endif
"      --pidfile=FILE  save the PID of the daemon to FILE\n"
"      --minstances    allow multiple instances of the splash daemon\n"
#ifndef CONFIG_FBSPLASH
"\nThis version of splashutils has been compiled without support for fbsplash.\n"
#endif
);
}

int main(int argc, char **argv)
{
	char dev[16];
	unsigned int c, i;
	int fp, err = 0;

	detect_endianess(&endianess);
	config = splash_lib_init(undef);

	arg_task = none;
	arg_vc = -1;

	verbose_img.cmap.red = silent_img.cmap.red = NULL;

#ifdef CONFIG_TTF
	if (TTF_Init() < 0) {
		fprintf(stderr, "Couldn't initialize TTF.\n");
		return -1;
	}
#endif

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
			if (config->theme)
				free(config->theme);
			config->theme = strdup(optarg);
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
				config->reqmode = 's';
			else
				config->reqmode = 'v';
			break;

		case 'p':
		case 0x105:
			config->progress = atoi(optarg);
			break;

		case 0x106:
			arg_vc = atoi(optarg)-1;
			if (arg_vc == -1)
				arg_vc = 0;
			break;

		case 'e':
		case 0x107:
			arg_export = optarg;
			break;

		case 0x108:
			config->kdmode = KD_GRAPHICS;
			break;
#ifdef CONFIG_TTF
		case 0x109:
			if (config->message)
				free(config->message);
			config->message = strdup(optarg);
			break;
#endif
		case 0x10a:
			arg_pidfile = strdup(optarg);
			break;

		case 0x10b:
			config->minstances = true;
			break;

		case 'd':
			arg_task = start_daemon;
			break;

		/* Verbosity level adjustment. */
		case 'q':
			config->verbosity = VERB_QUIET;
			break;

		case 'v':
			config->verbosity = VERB_HIGH;
			break;
		}
	}

	if (arg_task == none) {
		usage();
		return 0;
	}

#ifdef CONFIG_FBSPLASH
	fd_splash = open_fbsplash(false);
#endif

	if (get_fb_settings(arg_fb))
		return -1;

	if (config->theme)
		config_file = get_cfg_file(config->theme);

	if (config_file)
		parse_cfg(config_file);

	if (arg_task == start_daemon) {
		if (load_images('s'))
			return -1;
		daemon_start();
		/* we never get here */
	}

	if (arg_task != setmode && arg_vc == -1)
		arg_vc = 0;

	switch (arg_task) {

#ifdef CONFIG_FBSPLASH
	case on:
		err = cmd_setstate(1, FB_SPLASH_IO_ORIG_USER);
		break;

	case off:
		err = cmd_setstate(0, FB_SPLASH_IO_ORIG_USER);
		break;

	case setpic:
	{
		struct vt_stat stat;

		/* Setpic only makes sense in verbose mode. */
		if (config->reqmode != 'v')
			break;

		if ((fp = open(PATH_DEV "/tty", O_NOCTTY)) != -1) {
			if (ioctl(fp, VT_GETSTATE, &stat) != -1) {
				if (arg_vc != stat.v_active - 1)
					goto setpic_out;
			}
			close(fp);
		}

		err = cfg_check_sanity('v');
		if (err)
			break;

		err = do_getpic(FB_SPLASH_IO_ORIG_USER, 1, config->reqmode);
setpic_out:	break;
	}

	case getcfg:
		err = cmd_getcfg(FB_SPLASH_IO_ORIG_USER);
		break;

	case setcfg:
		err = cfg_check_sanity('v');
		if (err)
			break;
		err = cmd_setcfg(FB_SPLASH_IO_ORIG_USER);
		break;

	case getstate:
	{
		struct fb_splash_iowrapper wrapper = {
			.vc = arg_vc,
			.origin = FB_SPLASH_IO_ORIG_USER,
			.data = &i,
		};

		ioctl(fd_splash, FBIOSPLASH_GETSTATE, &wrapper);

		printf("Splash state on console %d: %s\n", arg_vc, (i != 0) ? "on" : "off");
		break;
	}
#endif /* CONFIG_FBSPLASH */

	case setmode:
	{
		int t;

		if (arg_vc > -1) {
			fp = open_tty(arg_vc+1, false);
			t = arg_vc+1;
		} else {
			t = (config->reqmode == 's') ? TTY_SILENT : TTY_VERBOSE;
			fp = open_tty(t, false);
		}

		if (fp < 0)
			break;

		if (config->reqmode == 's') {
			tty_silent_set(t, fp);
		} else {
			ioctl(fp, VT_ACTIVATE, t);
			ioctl(fp, VT_WAITACTIVE, t);
			close(fp);
			fp = open_tty(TTY_SILENT, false);
			if (fp < 0)
				break;
			tty_silent_unset(fp);
		}

		close(fp);
		break;
	}

	case getmode:
	{
		struct vt_stat stat;
		i = 0;

		if ((fp = open(PATH_DEV "/tty", O_NOCTTY)) != -1) {
			if (ioctl(fp, VT_GETSTATE, &stat) != -1) {
				if (stat.v_active == TTY_SILENT)
					i = 1;
			}
			close(fp);
		}

		printf("Splash mode: %s\n", (i) ? "silent" : "verbose");
		break;
	}

	/* Deprecated. The daemon mode should be used instead. */
	case paint:
	case repaint:
	{
		struct fb_image pic;
		u8 *out;

		sprintf(dev, PATH_DEV "/fb%d", arg_fb);
		if ((c = open(dev, O_RDWR)) == -1) {
			sprintf(dev, PATH_DEV "/fb/%d", arg_fb);
			if ((c = open(dev, O_RDWR)) == -1) {
				fprintf(stderr, "Failed to open " PATH_DEV "/fb%d or "
					 PATH_DEV "/fb/%d.\n", arg_fb, arg_fb);
				break;
			}
		}

		out = mmap(NULL, fb_fix.line_length * fb_var.yres, PROT_WRITE | PROT_READ,
				MAP_SHARED, c, fb_var.yoffset * fb_fix.line_length);

		if (out == MAP_FAILED) {
			fprintf(stderr, "mmap() " PATH_DEV "/fb%d failed.\n", arg_fb);
			close(c);
			err = -1;
			break;
		}

		/* Make sure the config file contains sane settings. */
		err = cfg_check_sanity(config->reqmode);
		if (err)
			break;

		if (do_getpic(FB_SPLASH_IO_ORIG_USER, 0, config->reqmode)) {
			err = -1;
			break;
		}

		if (config->reqmode == 's') {
			pic = silent_img;
		} else {
			pic = verbose_img;
		}

		if (pic.cmap.red)
			ioctl(c, FBIOPUTCMAP, &pic.cmap);

		if (arg_task == repaint || config->reqmode == 'v') {
			put_img(out, (u8*)pic.data);
		} else {
			do_paint(out, (u8*)pic.data);
		}

		munmap(out, fb_fix.line_length * fb_var.yres);
		close(c);

		free((u8*)pic.data);
		if (pic.cmap.red)
			free(pic.cmap.red);

		break;
	}

	default:
		break;
	}

#ifdef CONFIG_FBSPLASH
	close(fd_splash);
#endif

	if (config_file)
		free(config_file);

	return err;
}
