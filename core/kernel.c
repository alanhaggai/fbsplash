/*
 * kernel.c - the core of fbcondecor_helper
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
#include "fbcon_decor.h"

int arg_vc;

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
	char buf[8];
	int h;
	stheme_t *theme;
#ifdef CONFIG_FBCON_DECOR
	bool fbcon_decor = true;
#endif

	/* If possible, make sure that the error messages don't go straight
	 * to /dev/null and are displayed on the screen instead. */
	if (!update)
		prep_io();

	splashr_init(true);

	/* Parse the kernel command line to get splash settings. */
	mkdir(PATH_PROC,0700);
	h = mount("proc", PATH_PROC, "proc", 0, NULL);
	splash_parse_kcmdline(true);
	if (h == 0)
		umount(PATH_PROC);

	if (config.reqmode == 'o')
		return 0;

	/* We don't want to use any effects if we're just updating the image.
	 * Nor do we want to mess with the verbose mode. */
	if (update) {
		config.effects = SPL_EFF_NONE;
#ifdef CONFIG_FBCON_DECOR
		fbcon_decor = false;
#endif
	}

	theme = splashr_theme_load();
	if (!theme)
		return -1;

#ifdef CONFIG_FBCON_DECOR
	fd_fbcondecor = fbcon_decor_open(true);
	if (fd_fbcondecor == -1) {
		fprintf(stderr, "Failed to open the fbcon_decor control device.\n");
		fbcon_decor = false;
	}

	if (!update && (config.reqmode == 's' || config.reqmode == 'v') && fbcon_decor) {
		if (fbcon_decor_setcfg(FBCON_DECOR_IO_ORIG_USER, arg_vc, theme))
			goto noverbose;

		if (fbcon_decor_setpic(FBCON_DECOR_IO_ORIG_USER, arg_vc, theme))
			goto noverbose;
	} else {
noverbose:
		fbcon_decor = false;
	}
#endif
	/* Activate verbose mode if it was explicitly requested. If silent mode
	 * was requested, the verbose background image will be set after the
	 * switch to the silent tty is complete. */
	if (config.reqmode == 'v') {
#ifdef CONFIG_FBCON_DECOR
		/* Activate fbcon_decor on the first tty if the picture and
		 * the config file were successfully loaded. */
		if (fbcon_decor) {
			fbcon_decor_setstate(FBCON_DECOR_IO_ORIG_USER, arg_vc, 1);
			return 0;
		} else {
			fprintf(stderr, "Failed to get background decoration image.\n");
			return -1;
		}
#else
		fprintf(stderr, "This version of splashutils was compiled without support for fbcondecor.\n"
						"Verbose mode will not be activated\n");
		return -1;
#endif
	}

	if (!(theme->modes & MODE_SILENT))
		return -1;

	splash_set_silent(NULL);
	splashr_tty_silent_init();
	splashr_tty_silent_update();

	/* Redirect all kernel messages to tty1 so that they don't get
	 * printed over our silent splash image. */
	buf[0] = TIOCL_SETKMSGREDIRECT;
	buf[1] = 1;
	ioctl(fd_tty[config.tty_s], TIOCLINUX, buf);

	if (config.kdmode == KD_GRAPHICS)
		ioctl(fd_tty[config.tty_s], KDSETMODE, KD_GRAPHICS);

	splashr_render_screen(theme, true, true, 's', config.effects);

#ifdef CONFIG_FBCON_DECOR
	if (fbcon_decor && config.reqmode == 's')
		fbcon_decor_setstate(FBCON_DECOR_IO_ORIG_USER, arg_vc, 1);
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

	splash_lib_init(spl_undef);

	if (strcmp(argv[1],"2") && strcmp(argv[1], "1")) {
		fprintf(stderr, "Splash protocol mismatch: %s\n", argv[1]);
		fprintf(stderr, "This version of splashutils supports splash protocol v1 and v2.\n");
		err = -1;
		goto out;
	}

	if (argc > 3 && argv[3])
		arg_vc = atoi(argv[3]);
	else
		arg_vc = 0;

	if (arg_vc < 0)
		goto out;

	if (!strcmp(argv[1],"1")) {
		i = 6;
	}

	/* On 'init' the theme isn't defined yet, and thus NULL is passed
	 * instead of any meaningful value. */
	if (argc > i && argv[i]) {
		splash_acc_theme_set(argv[i]);
	}
	mkdir(PATH_SYS, 0700);
	if (!mount("sysfs", PATH_SYS, "sysfs", 0, NULL))
		mounts = 1;


	if (!strcmp(argv[2],"init")) {
		err = handle_init(false);
	} else if (!strcmp(argv[2],"repaint")) {
		err = handle_init(true);
	}
#ifdef CONFIG_FBCON_DECOR
	else if (config.theme) {
		stheme_t *theme;

		splashr_init(false);

		fd_fbcondecor = fbcon_decor_open(true);
		if (fd_fbcondecor == -1) {
			fprintf(stderr, "Failed to open the fbcon_decor control device.\n");
			err = -1;
			goto out;
		}

		theme = splashr_theme_load();
		if (!theme) {
			err = -1;
			goto out;
		}

		if (!strcmp(argv[2],"getpic")) {
			err = fbcon_decor_setpic(FBCON_DECOR_IO_ORIG_KERNEL, arg_vc, theme);
		} else if (!strcmp(argv[2],"modechange")) {
			if ((err = fbcon_decor_setcfg(FBCON_DECOR_IO_ORIG_KERNEL, arg_vc, theme)))
				goto out;

			if ((err = fbcon_decor_setpic(FBCON_DECOR_IO_ORIG_KERNEL, arg_vc, theme)))
				goto out;

			err = fbcon_decor_setstate(FBCON_DECOR_IO_ORIG_KERNEL, arg_vc, 1);
		}
	}
#endif
	else {
		fprintf(stderr, "Unrecognized splash command: %s.\n", argv[2]);
	}

out:
	if (mounts)
		umount2(PATH_SYS, 0);

#ifdef CONFIG_FBCON_DECOR
	if (fd_fbcondecor != -1)
		close(fd_fbcondecor);
#endif

	return err;
}
