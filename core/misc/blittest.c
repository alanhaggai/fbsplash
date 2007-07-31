#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>

#define xres 1280
#define yres 800
#define cnt 1000

int main(int argc, char **argv)
{
	int fd = open("/dev/fb0", O_RDWR);
	char *fb;
	char buf[xres*4];
	int i, j;

	fb = mmap(NULL, xres*yres*4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	for (i = 0; i < xres; i++) {
		buf[i] = rand();
	}

	printf("performing %d full-screen blits\n", cnt);
	for (j = 0; j < cnt; j++) {
		for (i = 0; i < yres; i++) {
			memcpy(fb + xres*4*i, buf, xres*4);
		}
	}

	munmap(fb, xres*yres*4);
	close(fd);

	return 0;
}
