/*
 * kernel.c - the core of splash_helper
 *
 * Copyright (C) 2004-2005, Michael Januszewski <spock@gentoo.org>
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
#include <linux/fb.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/kd.h>
#include <linux/tty.h>

#include "splash.h"

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

	if (TTF_Init() < 0) {
		fprintf(stderr, "Couldn't initialize TTF.\n");
	}
	
	if (!strcmp(argv[2],"getpic")) {

		err = do_getpic(FB_SPLASH_IO_ORIG_KERNEL, 1, arg_mode);
		
	} else if (!strcmp(argv[2],"init") || !strcmp(argv[2],"modechange")) {

		/* We have to act as if we were responding to a user request. This is 
		 * because the kernel will not hold the console sem while calling
		 * splash_helper init */
		if (!strcmp(argv[2], "init"))
			orig = FB_SPLASH_IO_ORIG_USER;

		create_dev(SPLASH_DEV, "/sys/class/misc/fbsplash/dev", 0x1);

		if (do_config(orig) || do_getpic(orig, 1, 'v')) {
			err = -1;
			goto out_init;
		}
		
		/* a list of things we should be doing on init:
		 *  - parse /proc/cmdline to get the silent tty
		 *  - switch to the silent tty
		 *  - disable the cursor
		 *  - disable echo and icanon
		 *  - paint the silent image
		 */
		if (arg_mode == 's') {
			
			int stty = TTY_SILENT;
			char sys[128]; 
			char fbfn[16];
			char vcfn[16];
			char buf[512];
			char *t;
			int fd, y, h;
			u8 created_dev = 0;
			u8 fadein = 0;
		
			h = mount("proc", "/proc", "proc", 0, NULL);
			fd = open("/proc/cmdline", O_RDONLY);
			if (fd != -1 && read(fd, buf, 512) > 0) {
			
				t = strstr(buf, "splash=");
				if (!t)
					goto next;

				t += 7;
				while (*t != ' ' && *t != 0) {
					if (!strncmp(t, "tty:", 4)) {
						stty = strtol(t+4, NULL, 0);
						t += 4;
					} else if (!strncmp(t, "fadein", 6)) {
						fadein = 1;
						t += 6;
					}
					t++;
				}
			}
			
next:			if (fd != -1)
				close(fd);

			if (h == 0)
				umount("/proc");
			
			if (stty < 0 || stty > MAX_NR_CONSOLES)
				stty = TTY_SILENT;

			sprintf(fbfn,"/dev/fb%d", arg_fb);
			sprintf(sys, "/sys/class/graphics/fb%d/dev", arg_fb);
	
			if (do_getpic(orig, 0, 's')) {
				err = -1;
				goto out_init;
			}
					
			open_cr(fb_fd, fbfn, sys, out, 0x4);
			cmd_setstate(1, orig);

			sprintf(vcfn,"/dev/tty%d", stty);
			sprintf(sys, "/sys/class/tty/tty%d/dev", stty);

			open_cr(vc_fd, vcfn, sys, out, 0x02);
			tty_set_silent(stty, vc_fd);
	
			t = mmap(NULL, fb_fix.line_length * fb_var.yres, PROT_WRITE | PROT_READ,
				MAP_SHARED, fb_fd, fb_var.yoffset * fb_fix.line_length); 
			
			if (t == MAP_FAILED) {
				goto init_cleanup;
			}

			if (silent_img.cmap.red)
				ioctl(fb_fd, FBIOPUTCMAP, &silent_img.cmap);
	
			if (fadein) {		
				fade_in(t, silent_img.data, silent_img.cmap, 1, fb_fd);
			} else {
				put_img(t, silent_img.data);
			}	
				
//			h = fb_var.xres * ((fb_var.bits_per_pixel + 7) >> 3);
//			for (y = 0; y < fb_var.yres; y++) {
//				memcpy(t + y * fb_fix.line_length, silent_img.data + y * h, h);
//			}

			munmap(t, fb_fix.line_length * fb_var.yres);
			
init_cleanup:		close_del(vc_fd, vcfn, 0x2);
//			close_del(fb_fd, fbfn, 0x4);
		
			free(silent_img.data);
			if (silent_img.cmap.red)
				free(silent_img.cmap.red);
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
