/*
 * fgconsole.c - aeb - 960123 - Print foreground console
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/vt.h>
#include "getfd.h"

int
main(int argc, char **argv){
	int fd;
	struct vt_stat vtstat;

	fd = getfd(NULL);
	if (ioctl(fd, VT_GETSTATE, &vtstat)) {
		perror("fgconsole: VT_GETSTATE");
		exit(1);
	}
	printf("%d\n", vtstat.v_active);
	return 0;
}
