/*
 * splash_dev.c - Functions to create /dev entries based upon sysfs data
 *
 * Copyright (C) 2004, Michal Januszewski <spock@gentoo.org>
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "splash.h"

static int flags = 0;

int create_dev(char *fn, char *sys, int flag)
{
	char buf[256];
	unsigned int major = 0, minor = 0;
	int fd;
	int res;
	
	if (!access(fn, W_OK | R_OK))
		return 0;	
	
	fd = open(sys, O_RDONLY);
	
	if (fd == -1) {
		return 1;
	}
	
	read(fd, buf, 256);
	close(fd);

	buf[255] = 0;

	sscanf(buf, "%u:%u", &major, &minor);

	if (major == 0) {
		return 2;
	}
		
	res = mknod(fn, 0600 | S_IFCHR, makedev(major, minor));
	if (!res)
		flags |= flag;
	
	return res;
}

int remove_dev(char *fn, int flag)
{
	int res;
	
	if (!(flags & flag))
		return 1;
	
	res = unlink(fn);

	if (!res)
		flags &= ~flag;
	return res;
}
