/*
 * kernel.c - the core of splash_helper
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
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/tiocl.h>

#include "util.h"

/*
 * Opens /dev/console as stdout and stderr. This will not work if console
 * is set to a serial console, because ttySx are not initialized at the
 * point where this function is used.
 */
void prep_io()
{
	int fd = 0;

	fd = open(PATH_DEV "/console", O_WRONLY);
	if (fd >= 0) {
		dup2(fd,0);
		dup2(fd,1);
		dup2(fd,2);
	}
}

int handle_init(bool update)
{
	char sys[128];
	char fn_fb[32];
	char fn_vc[32];
	char buf[1024];
	char *t, *p;
	int fd, fd_vc, fd_fb, h, cnt;
	u8 created_dev = 0;
	stheme_t *theme;
#ifdef CONFIG_FBSPLASH
	bool fbsplash = true;
#endif

	/* If possible, make sure that the error messages don't go straight
	 * to /dev/null and are displayed on the screen instead. */
	if (!update)
		prep_io();

	splash_render_init(true);

	/* Parse the kernel command line to get splash settings. */
	mkdir(PATH_PROC,0700);
	h = mount("proc", PATH_PROC, "proc", 0, NULL);
	splash_parse_kcmdline(&config, true);
	if (h == 0)
		umount(PATH_PROC);

	if (config.reqmode == 'o')
		return 0;

	/* We don't want to use any effects if we're just updating the image.
	 * Nor do we want to mess with the verbose mode. */
	if (update) {
		config.effects = EFF_NONE;
#ifdef CONFIG_FBSPLASH
		fbsplash = false;
#endif
	}

	theme = splash_theme_load();
	if (!theme)
		return -1;

#ifdef CONFIG_FBSPLASH
	if (!update && (config.reqmode == 's' || config.reqmode == 'v')) {
		if (fbsplash_setcfg(theme, FB_SPLASH_IO_ORIG_USER))
			goto noverbose;

		if (fbsplash_setpic(theme, FB_SPLASH_IO_ORIG_USER))
			goto noverbose;
	} else {
noverbose:
		fbsplash = false;
	}
#endif
	/* Activate verbose mode if it was explicitly requested. If silent mode
	 * was requested, the verbose background image will be set after the
	 * switch to the silent tty is complete. */
	if (config.reqmode == 'v') {
#ifdef CONFIG_FBSPLASH
		/* Activate fbsplash on the first tty if the picture and
		 * the config file were successfully loaded. */
		if (fbsplash) {
			fbsplash_setstate(1, FB_SPLASH_IO_ORIG_USER);
			return 0;
		} else {
			fprintf(stderr, "Failed to get verbose splash image.\n");
			return -1;
		}
#else
		fprintf(stderr, "This version of splashutils was compiled without support for fbsplash\n"
						"Verbose mode will not be activated\n");
		return -1;
#endif
	}

	if (!(theme->modes & MODE_SILENT))
		return -1;

	fd_vc = open_tty(config.tty_s, true);

	/* Redirect all kernel messages to tty1 so that they don't get
	 * printed over our silent splash image. */
	buf[0] = TIOCL_SETKMSGREDIRECT;
	buf[1] = 1;
	ioctl(fd_vc, TIOCLINUX, buf);

	tty_silent_set(config.tty_s, fd_vc);

	if (config.kdmode == KD_GRAPHICS)
		ioctl(fd_vc, KDSETMODE, KD_GRAPHICS);

	if (theme->silent_img.cmap.red)
		ioctl(fd_fb, FBIOPUTCMAP, &theme->silent_img.cmap);

	splash_render_screen(theme, true, true, 's', config.effects);

#ifdef CONFIG_FBSPLASH
	if (fbsplash && config.reqmode == 's')
		fbsplash_setstate(1, FB_SPLASH_IO_ORIG_USER);
#endif

	/* We're the kernel helper. We try to do our task as efficiently
	 * as possible and don't care about any cleanup. */
	return 0;
}

int main(int argc, char **argv)
{
	char *tmpc;
	int err = 0, i = 5;
	u8 mounts = 0;

	if (argc < 3)
		goto out;

	splash_lib_init(undef);

	if (strcmp(argv[1],"2") && strcmp(argv[1], "1")) {
		fprintf(stderr, "Splash protocol mismatch: %s\n", argv[1]);
		fprintf(stderr, "This version of splashutils supports splash protocol v1 and v2.\n");
		err = -1;
		goto out;
	}

	arg_task = none;
	if (argc > 3 && argv[3])
		arg_vc = atoi(argv[3]);
	else
		arg_vc = 0;

	if (argc > 4 && argv[4])
		arg_fb = atoi(argv[4]);
	else
		arg_fb = 0;

	if (arg_vc < 0 || arg_fb < 0)
		goto out;

	if (!strcmp(argv[1],"1")) {
		i = 6;
	}

	/* On 'init' the theme isn't defined yet, and thus NULL is passed
	 * instead of any meaningful value. */
	if (argc > i && argv[i]) {
		if (config.theme)
			free(config.theme);
		config.theme = strdup(argv[i]);
	}
	mkdir(PATH_SYS, 0700);
	if (!mount("sysfs", PATH_SYS, "sysfs", 0, NULL))
		mounts = 1;


	if (!strcmp(argv[2],"init")) {
		err = handle_init(false);
	} else if (!strcmp(argv[2],"repaint")) {
		err = handle_init(true);
	}
#ifdef CONFIG_FBSPLASH
	else if (config.theme) {
		stheme_t *theme;

		splash_render_init(false);
		theme = splash_theme_load();
		if (!theme) {
			err = -1;
			goto out;
		}

		if (!strcmp(argv[2],"getpic")) {
			err = fbsplash_setpic(theme, FB_SPLASH_IO_ORIG_KERNEL);
		} else if (!strcmp(argv[2],"modechange")) {
			if ((err = fbsplash_setcfg(theme, FB_SPLASH_IO_ORIG_KERNEL)))
				goto out;

			if ((err = fbsplash_setpic(theme, FB_SPLASH_IO_ORIG_KERNEL)))
				goto out;

			err = fbsplash_setstate(1, FB_SPLASH_IO_ORIG_KERNEL);
		}
	}
#endif
	else {
		fprintf(stderr, "Unrecognized splash command: %s.\n", argv[2]);
	}

out:
	if (mounts)
		umount2(PATH_SYS, 0);

	return err;
}
