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
#include <sys/wait.h>
#include <sys/vt.h>
#include <linux/kd.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include "splash.h"

pid_t pid_v = 0, pid_s = 0;

int tty_s = TTY_SILENT;
int tty_v = TTY_VERBOSE;

int fd_tty_s = 0;
int fd_tty_v = 0;
int fd_curr = 0;
int fd_fb = 0;

u8 *fb_mem = NULL;
u8 *bg_buffer = NULL;

struct termios tios;

void daemon_switch(int tty, int fd, u8 silent);
int cmd_repaint(void **args);

typedef struct {
	char *svc;
	enum ESVC state;
} svc_state;

list svcs = { NULL, NULL };

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
	
	if (pid_s) {
		kill(pid_s, SIGTERM);
		waitpid(pid_s, NULL, 1);
	}
		
	if (pid_v) {
		kill(pid_v, SIGTERM);
		waitpid(pid_v, NULL, 1);
	}
		
	pid_s = fork();
	if (pid_s == 0) {
		daemon_switch(tty_v, fd_tty_s, 1);
	}
	
	pid_v = fork();
	if (pid_v == 0) {
		daemon_switch(tty_s, fd_tty_v, 0);
	}
}

void tty_v_handler(int signum)
{
	if (signum != SIGUSR1)
		return;

	ioctl(fd_curr, TIOCSCTTY, 0);
}

void tty_s_switch_handler(int signum)
{
	if (signum == SIGUSR1) {
		ioctl(fd_tty_s, VT_RELDISP, 1);
	} else if (signum == SIGUSR2) {
		ioctl(fd_tty_s, VT_RELDISP, 2);
		/* Let the master daemon know that it has to redraw the picture */
		kill(getppid(), SIGUSR1);
	}
}

void daemon_term_handler(int signum)
{
	if (pid_s) {
		kill(pid_s, SIGTERM);
		waitpid(pid_s, NULL, 1);
	}
		
	if (pid_v) {
		kill(pid_v, SIGTERM);
		waitpid(pid_v, NULL, 1);
	}

	vt_cursor_enable(fd_tty_s);
	exit(0);
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
	if (silent)
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

	memset(&sig, 0, sizeof(sig));
	sigemptyset(&sig.sa_mask);

	if (silent) {
		sig.sa_handler = tty_s_switch_handler;
		sigaction(SIGUSR1, &sig, NULL);
		sigaction(SIGUSR2, &sig, NULL);

		vt.mode   = VT_PROCESS;
		vt.waitv  = 0;
		vt.relsig = SIGUSR1;
		vt.acqsig = SIGUSR2;
	
		ioctl(fd, VT_SETMODE, &vt);
		vt_cursor_disable(fd);
	} else {
		sig.sa_handler = tty_v_handler;
		sigaction(SIGUSR1, &sig, NULL);
	}

	sig.sa_handler = term_handler;
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
			if (read(fd, &t, 3) == 3 && 
			    ((endianess == little && t == 0x425b5b) || 
			     (endianess == big && 0x5b5b4200))) {
				ioctl(fd, VT_ACTIVATE, tty);
				ioctl(fd, VT_WAITACTIVE, tty);
			}
		}
	}
}

void free_images()
{
	item *i, *j;
	
	for (i = objs.head ; i != NULL; ) {
		j = i->next;	
		free(i->p);
		free(i);
		i = j;	
	}		
	
	for (i = rects.head; i != NULL ; ) {
		j = i->next;
		free(i);
		i = j;
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
		i = j;
	}

	list_init(objs);
	list_init(icons);
	list_init(rects);
	
	if (verbose_img.data)
		free((u8*)verbose_img.data);
	if (verbose_img.cmap.red)
		free(verbose_img.cmap.red);

	if (silent_img.data)
		free((u8*)silent_img.data);
	if (silent_img.cmap.red)
		free(silent_img.cmap.red);
}

int cmd_exit(void **args)
{
	item *i, *j;
		
	if (pid_s)
		kill(pid_s, SIGTERM);

	if (pid_v)
		kill(pid_v, SIGTERM);

	free_images();

	for (i = svcs.head; i != NULL; ) {
		j = i->next;
		free(i->p);
		free(i);
		i = j;
	}

	if (pid_s) {
		kill(pid_s, SIGTERM);
		waitpid(pid_s, NULL, 1);
	}
		
	if (pid_v) {
		kill(pid_v, SIGTERM);
		waitpid(pid_v, NULL, 1);
	}

	vt_cursor_enable(fd_tty_s);
	
	exit(0);

	/* We never get here */
	return 0;
}

void update_icon_status(char *svc, enum ESVC state)
{
	item *i;
	
	for (i = objs.head; i != NULL; i = i->next) { 
		icon *ic;
		obj *o = (obj*)i->p;
	
		if (o->type != o_icon)
			continue;
	
		ic = (icon*)o->p;
		
		if (!ic->svc || strcmp(ic->svc, svc))
			continue;
	
		if (ic->type == state)
			ic->status = 1;
		else
			ic->status = 0;
	}
}

int cmd_update_svc(void **args)
{
	item *i;
	svc_state *ss;
	enum ESVC state;

	if (!parse_svc_state(args[1], &state))
		return -1;
	
	for (i = svcs.head ; i != NULL; i = i->next) {
		ss = (svc_state*)i->p;
		if (!strcmp(ss->svc, args[0]))
			goto cus_update;
	}

	ss = malloc(sizeof(svc_state));
	ss->svc = strdup(args[0]);
	list_add(&svcs, ss);
	
cus_update:
	ss->state = state;
	update_icon_status(args[0], state);
	
	return 0;
}

int cmd_set_theme(void **args)
{
	item *i;
		
	if (arg_theme)
		free(arg_theme);
	
	if (config_file)
		free(config_file);

	arg_theme = strdup(args[0]);
	config_file = get_cfg_file(arg_theme);
	
	if (!config_file)
		return -1;

	free_images();
	free_fonts();
	parse_cfg(config_file);
	load_images('a');
	/* FIXME: free objs */
	global_font = TTF_OpenFont(cf.text_font, cf.text_size);
	load_fonts();
	
	for (i = svcs.head ; i != NULL; i = i->next) {
		svc_state *ss = (svc_state*)i->p;
		update_icon_status(ss->svc, ss->state);
	}
		
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
		if (tty_s == *(int*)args[1])
			return 0;
		tty_s = *(int*)args[1];
	} else if (!strcmp(args[0], "verbose")) {
		if (tty_v == *(int*)args[1])
			return 0;
		tty_v = *(int*)args[1];
	} else {
		return -1;
	}

	/* FIXME: do we need to do anything to the previous terminal? */

	vt_cursor_enable(fd_tty_s);
	start_tty_handlers();	
	vt_cursor_disable(fd_tty_s);
	
	return 0;
}

void do_paint_rect(u8 *dst, u8 *src, rect *re)
{
	u8 *to;
	int y, j;

	j = (re->x2 - re->x1 + 1) * bytespp;
	for (y = re->y1; y <= re->y2; y++) {
		to = dst + y * fb_fix.line_length + re->x1 * bytespp;
		memcpy(to, src + (y * fb_var.xres + re->x1) * bytespp, j);
	}			
}

void do_paint(u8 *dst, u8 *src)
{
	u8 *to;
	int j, y;
	obj *o;
	box *b;
	item *ti;
	rect *re;
			
	for (ti = rects.head ; ti != NULL; ti = ti->next) {
		re = (rect*)ti->p;
		do_paint_rect(dst, src, re);
	}

	for (ti = objs.head; ti != NULL; ti = ti->next) {
		o = (obj*)ti->p;
			
		if (o->type != o_box)
			continue;

		b = (box*)o->p;
	
		if (!(b->attr & BOX_INTER) || ti->next == NULL)
			continue;

		if (((obj*)ti->next->p)->type != o_box) 
			continue;

		b = (box*)((obj*)ti->next->p)->p;

		j = (b->x2 - b->x1 + 1) * bytespp;
		for (y = b->y1; y <= b->y2; y++) {
			to = dst + y * fb_fix.line_length + b->x1 * bytespp;
			memcpy(to, src + (y * fb_var.xres + b->x1) * bytespp, j); 
		}			
	}
}

void do_repaint(u8 *dst, u8 *src)
{
	int y, i;
	u8 *to = dst;
	
	i = fb_var.xres * bytespp;

	for (y = 0; y < fb_var.yres; y++) {
		memcpy(to, src + i*y, i);
		to += fb_fix.line_length;
	}
}

int cmd_paint(void **args)
{
	memcpy(bg_buffer, silent_img.data, fb_var.xres * fb_var.yres * bytespp);
	render_objs('s', (u8*)bg_buffer);

	do_paint(fb_mem, bg_buffer);	
	
	return 0;
}

int cmd_repaint(void **args)
{
	memcpy(bg_buffer, silent_img.data, fb_var.xres * fb_var.yres * bytespp);
	render_objs('s', (u8*)bg_buffer);

	do_repaint(fb_mem, bg_buffer);
	
	return 0;
}

int cmd_progress(void **args)
{
	if (*(int*)args[0] < 0 || *(int*)args[0] > PROGRESS_MAX)
		return -1;

	arg_progress = *(int*)args[0];
	return 0;
}

int cmd_set_mesg(void **args)
{
	if (boot_message)
		free(boot_message);
	boot_message = strdup(args[0]);
	return 0;
}

int cmd_paint_rect(void **args)
{
	rect re;
	int t;

	re.x1 = *(int*)args[0];
	re.x2 = *(int*)args[2];
	re.y1 = *(int*)args[1];
	re.y2 = *(int*)args[3];

	if (re.x1 > re.x2) {
		t = re.x2;
		re.x2 = re.x1;
		re.x1 = t;
	}

	if (re.y1 > re.y2) {
		t = re.y2;
		re.y2 = re.y1;
		re.y1 = t;
	}

	if (re.y1 < 0)
		re.y1 = 0;

	if (re.y2 < 0)
		re.y2 = 0;

	if (re.x1 < 0)
		re.x1 = 0;

	if (re.x2 < 0)
		re.x2 = 0;

	if (re.x1 >= fb_var.xres)
		re.x1 = fb_var.xres-1;

	if (re.x2 >= fb_var.xres)
		re.x2 = fb_var.xres-1;

	if (re.y1 >= fb_var.yres)
		re.y1 = fb_var.yres-1;

	if (re.y2 >= fb_var.yres)
		re.y2 = fb_var.yres-1;
		
	do_paint_rect(fb_mem, bg_buffer, &re);
	return 0;
}

struct {
	const char *cmd;
	int (*handler)(void**);
	int args;
	char *specs;
} known_cmds[] = {

	{	.cmd = "set theme",
		.handler = cmd_set_theme,
		.args = 1,
		.specs = "s"
	},

	{	.cmd = "set mode",
		.handler = cmd_set_mode,
		.args = 1,
		.specs = "s"
	},

	{	.cmd = "set tty",
		.handler = cmd_set_tty,
		.args = 2,
		.specs = "sd"
	},

	{	.cmd = "set message",
		.handler = cmd_set_mesg,
		.args = 1,
		.specs = "s"
	},

	{	.cmd = "paint rect",
		.handler = cmd_paint_rect,
		.args = 4,
		.specs = "dddd"
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
	},

	{	.cmd = "exit",
		.handler = cmd_exit,
		.args = 0,
		.specs = NULL,
	},
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
						for (; buf[k] != ' '; k++);
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

	/* Allocate the background buffer */
	bg_buffer = malloc(fb_var.xres * fb_var.yres * (fb_var.bits_per_pixel >> 3));
	if (!bg_buffer) {
		printerr("Can't allocate background image buffer\n.");
		exit(1);
	}

	/* Create a mmap of the framebuffer */
	fd_fb = open_fb();
	if (!fd_fb)
		exit(1);

	fb_mem = mmap(NULL, fb_fix.line_length * fb_var.yres, PROT_WRITE | PROT_READ,
			MAP_SHARED, fd_fb, 0); 
	
	if (fb_mem == MAP_FAILED) {
		printerr("mmap() /dev/fb%d failed.\n", arg_fb);
		close(fd_fb);
		exit(1);	
	}

	bytespp = (fb_var.bits_per_pixel + 7) >> 3;

	/* Create the splash FIFO if it's not already in place */
	stat(SPLASH_FIFO, &mystat);
	if (!S_ISFIFO(mystat.st_mode)) {
		if (mkfifo(SPLASH_FIFO, 0700))
			exit(3);
	}

	i = fork();
	if (i)
		exit(0);

	global_font = TTF_OpenFont(cf.text_font, cf.text_size);
	load_fonts();
	
	/* arg_theme is a reference to argv[x] of the original splash_util
	 * process. We're freeing arg_theme in cmd_set_theme(), so the strdup()
	 * call is necessary to make sure things don't break there. */
	arg_theme = strdup(arg_theme);
	
	signal(SIGTERM,daemon_term_handler);
	signal(SIGUSR1,(void(*)(int))cmd_repaint);
	signal(SIGUSR2,daemon_term_handler);
	signal(SIGSEGV,daemon_term_handler);
	signal(SIGINT,daemon_term_handler);
	signal(SIGABRT,daemon_term_handler);
	
	start_tty_handlers();
	daemon_comm();	
}

