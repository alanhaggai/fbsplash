/*
 * fbsplashctl.c - The unified fbsplash/fbcondecor control app.
 *
 * Copyright (C) 2008 Michal Januszewski <spock@gentoo.org>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "common.h"
#include "render.h"

int fbsplashd_main(int argc, char **argv);
int util_main(int argc, char **argv);
int fbcondecor_main(int argc, char **argv);

int main(int argc, char **argv)
{
	if (strstr(argv[0], "fbsplashd.static")) {
		return fbsplashd_main(argc, argv);
	} else if (strstr(argv[0], "splash_util.static")) {
		return util_main(argc, argv);
	}
#ifdef CONFIG_FBCON_DECOR
	else if (strstr(argv[0], "fbcondecor_ctl.static")) {
		return fbcondecor_main(argc, argv);
	}
#endif

	return 0;
}

