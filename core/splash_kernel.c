/*
 * splash_kernel.c - The core of splash_helper
 *
 * Copyright (C) 2004, Michal Januszewski <spock@gentoo.org>
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * $Header: /srv/cvs/splash/utils/splash_kernel.c,v 1.8 2004/09/27 14:31:56 spock Exp $
 * 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <linux/fb.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <linux/kd.h>

#include "splash.h"

/* $Header: /srv/cvs/splash/utils/splash_kernel.c,v 1.8 2004/09/27 14:31:56 spock Exp $ */

int main(int argc, char **argv)
{
	int vc_fd, fb_fd, err = 0;
	unsigned char orig = FB_SPLASH_IO_ORIG_KERNEL;
	unsigned char mounts = 0;

	detect_endianess();

	if (argc < 3)
		goto out;

	if (strcmp(argv[1],"1")) {
		fprintf(stderr, "Splash protocol mismatch: %s\n", argv[1]);
		goto out;
	}

	arg_task = none;
	arg_vc = atoi(argv[3]);
	arg_fb = atoi(argv[4]);

	if (arg_vc < 0 || arg_fb < 0)
		goto out;

	arg_mode = argv[5][0];
	arg_theme = strdup(argv[6]);

	if (!mount("sysfs", "/sys", "sysfs", 0, NULL))
		mounts = 1;

	get_fb_settings(arg_fb);
	config_file = get_cfg_file(arg_theme);

	if (!config_file)
		goto out;

	parse_cfg(config_file);

	if (!strcmp(argv[2],"getpic")) {

		err = do_getpic(FB_SPLASH_IO_ORIG_KERNEL, 1, arg_mode);
		
	} else if (!strcmp(argv[2],"init") || !strcmp(argv[2],"modechange")) {

		/* We have to act if we were responding to a user request. This is 
		 * because the kernel will not hold the console sem while calling
		 * splash_helper init */
		if (!strcmp(argv[2], "init"))
			orig = FB_SPLASH_IO_ORIG_USER;

		create_dev(SPLASH_DEV, "/sys/class/misc/fbsplash/dev", 0x1);

		if (do_config(orig) || do_getpic(orig, 1, 'v')) {
			err = -1;
			goto out_init;
		}
	
		if (arg_mode == 's') {
			
			u8 created_dev = 0;
			char sys[128]; 
			char fbfn[16];
			int y, t;
	
			sprintf(fbfn,"/dev/fb%d", arg_fb);
			sprintf(sys, "/sys/class/graphics/fb%d/dev", arg_fb);
	
			if (do_getpic(orig, 0, 's')) {
				err = -1;
				goto out_init;
			}
			
			open_cr(fb_fd, fbfn, sys, out, 0x4);
			cmd_setstate(1, orig);

			if (!strcmp(argv[2], "init")) {
				open_cr(vc_fd, "/dev/vc/0", "/sys/class/tty/tty0/dev", out, 0x2);
				ioctl(vc_fd, KDSETMODE, KD_GRAPHICS);
				close_del(vc_fd, "/dev/vc/0", 0x2);
			}
		
			t = fb_var.xres * ((fb_var.bits_per_pixel + 7) >> 3);

			for (y = 0; y < fb_var.yres; y++) {
				if (t != fb_fix.line_length || fb_var.yoffset != 0)
					lseek(fb_fd, (fb_var.yoffset + y) * fb_fix.line_length, SEEK_SET);
				write(fb_fd, pic.data + t * y, t);
			}

			if (pic.cmap.red)
				ioctl(fb_fd, FBIOPUTCMAP, &pic.cmap);

//			close_del(fb_fd, fbfn, 0x4);
		
			free(pic.data);
			if (pic.cmap.red)
				free(pic.cmap.red);
		} else {
			cmd_setstate(1, orig);	
		}
	
out_init:	//remove_dev(SPLASH_DEV, 0x1);
		;
	} else {
		fprintf(stderr, "Unrecognized splash command: %s.\n", argv[2]);
	}

out:	
	if (mounts)
		umount2("/sys",0);

	if (config_file)
		free(config_file);
		
	return err;
}
