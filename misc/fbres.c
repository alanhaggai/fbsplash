/*
 * A stupid little program that isn't really useful for anything
 * and that has to be here because some people use separate /usr
 * partitions.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <linux/fb.h>

void die(const char *fmt, ...)
{
	va_list ap;

	fflush(stdout);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(1);
}

int main(int argc, char *argv[])
{
	struct fb_var_screeninfo var;
	int fh;
	char *xres, *yres;

	xres = getenv("SPLASH_XRES");
	yres = getenv("SPLASH_YRES");

	if (xres && yres && atoi(xres) > 0 && atoi(yres) > 0) {
		printf("%sx%s\n", xres, yres);
		return 0;
	}

	if ((fh = open("/dev/fb0", O_RDONLY)) == -1)
		if ((fh = open("/dev/fb/0", O_RDONLY)) == -1)
			die("Can't open /dev/fb0 or /dev/fb/0\n");

	if (ioctl(fh, FBIOGET_VSCREENINFO, &var))
		die("Can't get fb_var\n");


	printf("%dx%d\n", var.xres, var.yres);
	close(fh);

	return 0;
}
