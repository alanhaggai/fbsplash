/*
 * kernel.c - the core of splash_helper
 *
 * Copyright (C) 2004-2005, Michal Januszewski <spock@gentoo.org>
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

#define EFF_FADEIN 1

/* Opens /dev/console as stdout and stderr. */
void prep_io()
{
	int fd = 0;

	fd = open("/dev/console", O_WRONLY);
	if (fd >= 0) {
		dup2(fd,0);
		dup2(fd,1);
		dup2(fd,2);
	}
}

int handle_init(u8 update)
{
	int stty = TTY_SILENT;
	char sys[128]; 
	char fn_fb[16];
	char fn_vc[16];
	char buf[512];
	char *t, *p;
	int fd, fd_vc, fd_fb, h, cnt;
	u8 created_dev = 0;
	u8 effects = 0;
#ifdef CONFIG_FBSPLASH
	u8 fbsplash = 1;
#endif
	arg_mode = ' ';

	if (!update) {
		/* If possible, make sure that the error messages that we print don't
		 * go straight to /dev/null and are displayed on the screen instead. */
		prep_io();
	}
		
	/* Mount the proc filesystem */
	h = mount("proc", "/proc", "proc", 0, NULL);
	fd = open("/proc/cmdline", O_RDONLY);
	if (fd != -1 && (cnt = read(fd, buf, 512)) > 0) {
		char *opt;
		buf[cnt-1] = 0;
		
		t = strstr(buf, "splash=");
		if (!t)
			goto parse_failure;

		t += 7; p = t;
		for (p = t; *p != ' ' && *p != 0; p++);
		*p = 0;
				
		while ((opt = strsep(&t, ",")) != NULL) {
			if (!strncmp(opt, "tty:", 4)) {
				stty = strtol(opt+4, NULL, 0);
			} else if (!strncmp(opt, "fadein", 6)) {
				effects |= EFF_FADEIN;
			} else if (!strncmp(opt, "verbose", 7)) {
				arg_mode = 'v';
			} else if (!strncmp(opt, "silent", 6)) {
				arg_mode = 's';
			} else if (!strncmp(opt, "theme:", 6)) {
				arg_theme = strdup(opt+6);
			} else if (!strncmp(opt, "kdgraphics", 10)) {
				arg_kdmode = KD_GRAPHICS;
			}
		}
	} else {
		/* If we can't parse the command line, we can't
		 * make any assuptions as to in which mode splash
		 * is to be started -- so we just quit. */
parse_failure:	if (h == 0)
			umount("/proc");

		return -1;
	}

	close(fd);
	if (h == 0)
		umount("/proc");

	/* We don't want to use any effects if we're just updating the image. 
	 * Nor do we want to mess with the verbose mode. */
	if (update) {
		effects = 0;
		fbsplash = 0;
	}
		
	/* If no mode was specified, we can't make any decisions
	 * by ourselves. */
	if (arg_mode == ' ')
		return 0;	
	
#ifdef CONFIG_FBSPLASH
	if (!update) {
		create_dev(SPLASH_DEV, "/sys/class/misc/fbsplash/dev", 0x1);
	}
#endif
	if (!arg_theme) {
		arg_theme = strdup(DEFAULT_THEME);
	}
	
	config_file = get_cfg_file(arg_theme);
	if (!config_file)
		return -1;
	parse_cfg(config_file);

#ifdef CONFIG_FBSPLASH
	if (!update) {
		/* Load the configuartion and the verbose background picture
		 * but don't activate fbsplash just yet. We'll enable it
		 * after the silent screen is displayed. */
		if (do_config(FB_SPLASH_IO_ORIG_USER) || do_getpic(FB_SPLASH_IO_ORIG_USER, 1, 'v')) {
			fbsplash = 0;
		}
	}	
#endif
	if (arg_mode != 's') {
#ifdef CONFIG_FBSPLASH
		/* Activate fbsplash on the first tty is the picture and 
		 * config file were successfully loaded. */
		if (fbsplash) {
			cmd_setstate(1, FB_SPLASH_IO_ORIG_USER);
			return 0;
		} else {
			printerr("Failed to get verbose splash image.\n");
			return -1;
		}
#else
		printerr("This version of splashutils was compiled without support for fbsplash\n"
			 "Verbose mode will not be activated\n");
		return -1;
#endif
	}
	
	/* If the user supplied a silent tty number, check whether
	 * it is valid. */
	if (stty < 0 || stty > MAX_NR_CONSOLES)
		stty = TTY_SILENT;

	sprintf(fn_fb,"/dev/fb%d", arg_fb);
	sprintf(sys, "/sys/class/graphics/fb%d/dev", arg_fb);

	if (do_getpic(FB_SPLASH_IO_ORIG_USER, 0, 's')) {
		printerr("Failed to get silent splash image.\n");
#ifdef CONFIG_FBSPLASH
		if (fbsplash)
			cmd_setstate(1, FB_SPLASH_IO_ORIG_USER);
#endif
		return -1;
	}
				
	open_cr(fd_fb, fn_fb, sys, out, 0x4);

	sprintf(fn_vc,"/dev/tty%d", stty);
	sprintf(sys, "/sys/class/tty/tty%d/dev", stty);

	open_cr(fd_vc, fn_vc, sys, out, 0x2);
	t = mmap(NULL, fb_fix.line_length * fb_var.yres, PROT_WRITE | PROT_READ,
		 MAP_SHARED, fd_fb, fb_var.yoffset * fb_fix.line_length); 
		
	if (t == MAP_FAILED) {
		goto clean;
	}

	tty_set_silent(stty, fd_vc);
		
	if (arg_kdmode == KD_GRAPHICS) 
		ioctl(fd_vc, KDSETMODE, KD_GRAPHICS);	
	
	if (silent_img.cmap.red)
		ioctl(fd_fb, FBIOPUTCMAP, &silent_img.cmap);

	if (effects & EFF_FADEIN) {		
		fade_in(t, silent_img.data, silent_img.cmap, 1, fd_fb);
	} else {
		if (fb_fix.visual == FB_VISUAL_DIRECTCOLOR)
			set_directcolor_cmap(fd_fb);

		put_img(t, silent_img.data);
	}	
			
	munmap(t, fb_fix.line_length * fb_var.yres);

#ifdef CONFIG_FBSPLASH
	if (fbsplash)
		cmd_setstate(1, FB_SPLASH_IO_ORIG_USER);
#endif
	
clean:	close_del(fd_vc, fn_vc, 0x2);
//	close_del(fd_fb, fn_fb, 0x4);
	
out:	free(silent_img.data);
	if (silent_img.cmap.red)
		free(silent_img.cmap.red);

//	remove_dev(SPLASH_DEV, 0x1);
}

int main(int argc, char **argv)
{
	char *tmpc;
	int err = 0, i = 5;
	u8 mounts = 0;

	detect_endianess();

	if (argc < 3)
		goto out;

	if (strcmp(argv[1],"2") && strcmp(argv[1], "1")) {
		fprintf(stderr, "Splash protocol mismatch: %s\n", argv[1]);
		fprintf(stderr, "This version of splashutils supports splash protocol v1 and v2.\n");
		err = -1;
		goto out;
	}

	arg_task = none;
	arg_vc = atoi(argv[3]);
	arg_fb = atoi(argv[4]);

	if (arg_vc < 0 || arg_fb < 0)
		goto out;

	if (!strcmp(argv[1],"1")) {
		i = 6;
	}
	
	/* On 'init' the theme isn't defined yet, and thus NULL is passed
	 * instead of any meaningful value. */
	if (argv[i]) 
		arg_theme = strdup(argv[i]);
	else
		arg_theme = NULL;
	
	if (!mount("sysfs", "/sys", "sysfs", 0, NULL))
		mounts = 1;

	get_fb_settings(arg_fb);

	if (arg_theme) {
		config_file = get_cfg_file(arg_theme);
		if (!config_file)
			goto out;
		parse_cfg(config_file);
	}

#ifdef CONFIG_TTF_KERNEL
	if (TTF_Init() < 0) {
		fprintf(stderr, "Couldn't initialize TTF.\n");
	}
	boot_message = getenv("BOOT_MSG");
#endif
	/* The PROGRESS env. variable can be used to set the progress status. */
	tmpc = getenv("PROGRESS");
	if (tmpc)
		arg_progress = atoi(tmpc);
	
	if (!strcmp(argv[2],"init")) {
		err = handle_init(0);
	} else if (!strcmp(argv[2],"update")) {
		err = handle_init(1);
	}
#ifdef CONFIG_FBSPLASH
	else if (!strcmp(argv[2],"getpic")) {
		err = do_getpic(FB_SPLASH_IO_ORIG_KERNEL, 1, 'v');
	} else if (!strcmp(argv[2],"modechange")) {
		do_config(FB_SPLASH_IO_ORIG_KERNEL);
		do_getpic(FB_SPLASH_IO_ORIG_KERNEL, 1, 'v');
		cmd_setstate(1, FB_SPLASH_IO_ORIG_KERNEL);
	} 
#endif	
	else {
		fprintf(stderr, "Unrecognized splash command: %s.\n", argv[2]);
	}

out:	if (mounts)
		umount2("/sys",0);

	if (config_file)
		free(config_file);
		
	return err;
}
