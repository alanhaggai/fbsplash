/*
 * daemon.c - The splash daemon 
 * 
 * Copyright (C) 2005 Michael Januszewski <spock@gentoo.org>
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
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/vt.h>
#include <linux/kd.h>
#include <linux/fb.h>
#include "splash.h"

pid_t pid_v, pid_s;

int tty_s = TTY_SILENT;
int tty_v = TTY_VERBOSE;

int fd_tty_s = 0;
int fd_tty_v = 0;
int fd_curr = 0;

u8 *bg_buffer = NULL;

struct termios tios;

void daemon_switch(int tty, int fd, u8 silent);

void start_tty_handlers() 
{
	char t[16];
	
	if (fd_tty_v)
		close(fd_tty_v);
		
	sprintf(t, "/dev/tty%d", tty_v);
	fd_tty_v = open(t, O_RDWR);
	if (fd_tty_v == -1) {
		sprintf(t, "/dev/vc/%d", tty_v);
		fd_tty_v = open(t, O_RDWR);
		if (fd_tty_v == -1) {
			fprintf(stderr, "Can't open %s.\n", t);
			exit(1);
		}
	}

	if (fd_tty_s)
		close(fd_tty_s);
		
	sprintf(t, "/dev/tty%d", tty_s);
	fd_tty_s = open(t, O_RDWR);
	if (fd_tty_s == -1) {
		sprintf(t, "/dev/vc/%d", tty_s);
		fd_tty_s = open(t, O_RDWR);
		if (fd_tty_s == -1) {
			fprintf(stderr, "Can't open %s.\n", t);
			exit(2);
		}
	}
	
	if (pid_s)
		kill(pid_s, SIGTERM);

	if (pid_v)
		kill(pid_v, SIGTERM);
		
	pid_s = fork();
	if (pid_s == 0) {
		daemon_switch(tty_v, fd_tty_s, 1);
	}
	
	pid_v = fork();
	if (pid_v == 0) {
		daemon_switch(tty_s, fd_tty_v, 0);
	}
}

void tty_s_switch_handler(int signum)
{
	if (signum == SIGUSR1) {
		ioctl(fd_tty_s, VT_RELDISP, 1);
	} else if (signum == SIGUSR2) {
		ioctl(fd_tty_s, VT_RELDISP, 2);
//		ioctl(fd_tty_s, KDSETMODE, KD_GRAPHICS);
	}
}

void term_handler(int signum)
{
	tcsetattr(fd_curr, TCSANOW, &tios);
	exit(0);
}

void daemon_switch(int tty, int fd, u8 silent)
{
	struct termios w;
	struct sigaction sig;
	struct vt_mode vt;
	int flags;
	
	fd_curr = fd;
	setsid();
	ioctl(fd, TIOCSCTTY, 0);

	tcgetattr(fd,&tios);
	w = tios;
	w.c_lflag &= (~ICANON);
	if (silent)
		w.c_lflag &= (~ECHO);
	w.c_cc[VTIME] = 0;
	w.c_cc[VMIN] = 1;
	tcsetattr(fd, TCSANOW, &w);

	if (silent) {
		memset(&sig, 0, sizeof(sig));
		sig.sa_handler = tty_s_switch_handler;
		sigemptyset(&sig.sa_mask);
		sigaction(SIGUSR1, &sig, NULL);
		sigaction(SIGUSR2, &sig, NULL);

		vt.mode   = VT_PROCESS;
		vt.waitv  = 0;
		vt.relsig = SIGUSR1;
		vt.acqsig = SIGUSR2;
	
		ioctl(fd, VT_SETMODE, &vt);
	}

	memset(&sig, 0, sizeof(sig));
	sig.sa_handler = term_handler;
	sigemptyset(&sig.sa_mask);
	sigaction(SIGTERM, &sig, NULL);
	
	flags = fcntl(fd, F_GETFL, 0);
	
	while(1) {
		char ret = 0xff;
		int t = 0;
		
		fcntl(fd, F_SETFL, flags & (~O_NDELAY));
		read(fd, &ret, 1);

		if (ret == '\x1b') {
			fcntl(fd, F_SETFL, flags | O_NDELAY);
			
			/* FIXME: is <F2> always 1b5b5b42? */
			/* FIXME: endianess! */
			if (read(fd, &t, 3) == 3 && t == 0x425b5b) {
				ioctl(fd, VT_ACTIVATE, tty);
				ioctl(fd, VT_WAITACTIVE, tty);
			}
		}
	}
}

int cmd_update_svc(void **args)
{
	return 0;
}

int cmd_set_theme(void **args)
{
	item *i, *j;
		
	if (arg_theme)
		free(arg_theme);

	arg_theme = strdup(args[0]);
	config_file = get_cfg_file(arg_theme);
	
	if (!config_file)
		return -1;
		
	for (i = objs.head ; i != NULL; ) {
		j = i->next;	
		free(i->p);
		free(i);
		j = i;	
	}
	
	for (i = rects.head; i != NULL ; ) {
		j = i->next;
		free(i);
		j = i;
	}
	
	for (i = icons.head; i != NULL; ) {
		icon_img *ii = (icon_img*) i->p;
		j = i->next;
		if (ii->filename)
			free(ii->filename);
		if (ii->picbuf)
			free(ii->picbuf);
		free(ii);
		free(i);
		j = i;
	}

	/* FIXME: free image buffers and stuff */

	parse_cfg(config_file);

	/* FIXME: reload pictures */
	
	return 0;
}

int cmd_set_mode(void **args)
{
	int n;
	
	if (!strcmp(args[0], "silent")) {
		n = tty_s;
	} else if (!strcmp(args[0], "verbose")) {
		n = tty_v;
	} else {
		return -1;
	}
	
	ioctl(fd_tty_s, VT_ACTIVATE, n);
	ioctl(fd_tty_s, VT_WAITACTIVE, n);
	
	return 0;
}

int cmd_set_tty(void **args)
{
	if (!strcmp(args[0], "silent")) {
		tty_s = *(int*)args[1];
	} else if (!strcmp(args[0], "verbose")) {
		tty_v = *(int*)args[1];
	} else {
		return -1;
	}

	/* FIXME: do we need to do anything to the previous terminal? */
	start_tty_handlers();	
	
	return 0;
}

int cmd_paint(void **args)
{
	return 0;
}

int cmd_repaint(void **args)
{
	return 0;
}

int cmd_progress(void **args)
{
	if (*(int*)args[0] < 0 || *(int*)args[0] > PROGRESS_MAX)
		return -1;

	arg_progress = *(int*)args[0];
	return 0;
}

struct {
	const char *cmd;
	int (*handler)(void**);
	int args;
	char *specs;
} known_cmds[] = {

	{	.cmd = "set_theme",
		.handler = cmd_set_theme,
		.args = 1,
		.specs = "s"
	},

	{	.cmd = "set_mode",
		.handler = cmd_set_mode,
		.args = 1,
		.specs = "s"
	},

	{	.cmd = "set_tty",
		.handler = cmd_set_tty,
		.args = 2,
		.specs = "sd"
	},

	{	.cmd = "paint",
		.handler = cmd_paint,
		.args = 0,
		.specs = NULL,
	},

	{	.cmd = "repaint",
		.handler = cmd_repaint,
		.args = 0,
		.specs = NULL,
	},

	{	.cmd = "progress",
		.handler = cmd_progress,
		.args = 1,
		.specs = "d"
	},

	{	.cmd = "update_svc",
		.handler = cmd_update_svc,
		.args = 2,
		.specs = "ss",
	}
};

int daemon_comm()
{
	char buf[1024];
	FILE *fp_fifo; 
	int i,j,k;

	while (1) {
		fp_fifo = fopen(SPLASH_FIFO, "r");
		if (!fp_fifo) {
			perror("Can't open the splash FIFO (" SPLASH_FIFO ") for reading");
			return -1;
		}

		while (fgets(buf, 1024, fp_fifo)) {
			
			char *t;
			int args_i[4];
			void *args[4];
			
			buf[1023] = 0;
			buf[strlen(buf)-1] = 0;
			
			for (i = 0; i < sizeof(known_cmds)/sizeof(known_cmds[0]); i++) {

				k = strlen(known_cmds[i].cmd);
				
				if (strncmp(buf, known_cmds[i].cmd, k))
					continue;
						
				for (j = 0; j < known_cmds[i].args; j++) {
			
					for (; buf[k] == ' '; buf[k] = 0, k++);
					if (!buf[k])
						goto out;
				
					switch (known_cmds[i].specs[j]) {
					
					case 's':
						args[j] = &(buf[k]);
						break;

					case 'd':
						args_i[j] = strtol(&(buf[k]), &t, 0);
						if (t == &(buf[k]))
							goto out;
												
						args[j] = &(args_i[j]);
						k = t - buf;
						break;
					}
				}

				known_cmds[i].handler(args);
			}
		}

out:		fclose(fp_fifo);
	}
}


void daemon_start()
{
	int i = 0;
	struct stat mystat;
	
	stat(SPLASH_FIFO, &mystat);
	if (!S_ISFIFO(mystat.st_mode)) {
		if (mkfifo(SPLASH_FIFO, 0700))
			exit(3);
	}

	i = fork();
	if (i)
		exit(0);

	start_tty_handlers();
	daemon_comm();	
}

