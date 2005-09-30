/*
 * daemon.c - The splash daemon 
 * 
 * Copyright (C) 2005 Michal Januszewski <spock@gentoo.org>
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
#include <linux/tty.h>
#include <linux/input.h>
#include <sys/mman.h>
#include <pthread.h>
#include <errno.h>
#include "splash.h"

/* Notifiers */
#define NOTIFY_REPAINT 	0
#define NOTIFY_PAINT 	1
char *notify[2];

/* Current TTY */
#define CTTY_SILENT 	0
#define CTTY_VERBOSE	1
pthread_mutex_t mtx_ctty = PTHREAD_MUTEX_INITIALIZER;
int ctty;

char *evdev = NULL;

/* This mutex protects tty_s, tty_v, fd_tty_s. */
pthread_mutex_t mtx_tty = PTHREAD_MUTEX_INITIALIZER;
int tty_s = TTY_SILENT;
int tty_v = TTY_VERBOSE;
int fd_tty_s = -1;
int fd_tty1 = -1;

int fd_fb = -1;
int fd_evdev = -1;

u8 *fb_mem = NULL;
u8 *bg_buffer = NULL;
pthread_mutex_t mtx_bgbuf = PTHREAD_MUTEX_INITIALIZER;

int fd_bg = 0;		/* FD for the exported background buffer */
struct termios tios;

pthread_t th_switchmon, th_sighandler;

int cmd_repaint(void **args);
void handle_silent_switch();

typedef struct {
	char *svc;
	enum ESVC state;
} svc_state;

list svcs = { NULL, NULL };

u8 theme_loaded = 0;
pthread_mutex_t mtx_theme = PTHREAD_MUTEX_INITIALIZER;

#define UPD_SILENT 	0x01
#define UPD_MON		0x02
#define UPD_ALL		(UPD_SILENT | UPD_MON)

void init_term_silent(int tty, int fd)
{
	struct vt_mode vt;
	struct termios w;
	
	ioctl(fd, TIOCSCTTY, 0);

	vt.mode   = VT_PROCESS;
	vt.waitv  = 0;
	vt.relsig = SIGUSR1;
	vt.acqsig = SIGUSR2;
	ioctl(fd, VT_SETMODE, &vt);

	tcgetattr(fd_tty_s,&tios);
	w = tios;
	w.c_lflag &= ~(ICANON|ECHO);
	w.c_cc[VTIME] = 0;
	w.c_cc[VMIN] = 1;
	tcsetattr(fd, TCSANOW, &w);

	vt_cursor_disable(fd);
	return;
}

void deinit_term_silent(void)
{
	struct vt_mode vt;
	
	vt.mode   = VT_AUTO;
	vt.waitv  = 0;

	tcsetattr(fd_tty_s, TCSANOW, &tios);
	ioctl(fd_tty_s, KDSETMODE, KD_TEXT);
	ioctl(fd_tty_s, VT_SETMODE, &vt);
}


void* sighandler(void *unusued)
{
	sigset_t sigset;
	int sig, i;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR1);
	sigaddset(&sigset, SIGUSR2);
	sigaddset(&sigset, SIGTERM);

	while (1) {
		sigwait(&sigset, &sig);

		/* Switch from silent to verbose. */
		if (sig == SIGUSR1) {
			pthread_mutex_lock(&mtx_ctty);
			ctty = CTTY_VERBOSE;
			pthread_mutex_unlock(&mtx_ctty);
			
			i = pthread_mutex_trylock(&mtx_tty);
			ioctl(fd_tty_s, VT_RELDISP, 1);
			if (!i)
				pthread_mutex_unlock(&mtx_tty);
		/* Switch back to silent. */
		} else if (sig == SIGUSR2) {
			i = pthread_mutex_trylock(&mtx_tty);
			ioctl(fd_tty_s, VT_RELDISP, 2);
			if (!i)
				pthread_mutex_unlock(&mtx_tty);

			pthread_mutex_lock(&mtx_ctty);
			ctty = CTTY_SILENT;
			pthread_mutex_unlock(&mtx_ctty);
	
			handle_silent_switch();
		} else if (sig == SIGTERM) {
			deinit_term_silent();
			vt_cursor_enable(fd_tty_s);
			pthread_kill_other_threads_np();
			pthread_exit(NULL);
		}
	}
}

void* switch_evdev(void *unused)
{
	int i, h;
	size_t rb; 
	struct input_event ev[8];

	while ((rb = read(fd_evdev, ev, sizeof(struct input_event)*8)) > 0) {
    		if (rb < (int) sizeof(struct input_event))
			continue;
   	
		for (i = 0; i < (int) (rb / sizeof(struct input_event)); i++) {
			if (ev[i].type != EV_KEY || ev[i].value != 0 || ev[i].code != KEY_F2)
				continue;

			pthread_mutex_lock(&mtx_ctty);
			if (ctty == CTTY_SILENT) {
				h = tty_v;
			} else {
				h = tty_s;
			}
			ioctl(fd_tty1, VT_ACTIVATE, h);
			pthread_mutex_unlock(&mtx_ctty);
		}
	}

	pthread_exit(NULL);
}

void* switch_ttymon(void *unused)
{
	int flags;
	
	flags = fcntl(fd_tty_s, F_GETFL, 0);
	
	while(1) {
		char ret = 0xff;
		int t = 0;
		
		fcntl(fd_tty_s, F_SETFL, flags & (~O_NDELAY));
		read(fd_tty_s, &ret, 1);

		if (ret == '\x1b') {
			pthread_mutex_lock(&mtx_tty);
			fcntl(fd_tty_s, F_SETFL, flags | O_NDELAY);
			
			/* FIXME: is <F2> always 1b5b5b42? */
			if (read(fd_tty_s, &t, 3) == 3 && 
			    ((endianess == little && t == 0x425b5b) || 
			     (endianess == big && 0x5b5b4200))) {
				pthread_mutex_unlock(&mtx_tty);
				ioctl(fd_tty1, VT_ACTIVATE, tty_v);
				ioctl(fd_tty1, VT_WAITACTIVE, tty_v);
			} else {
				pthread_mutex_unlock(&mtx_tty);
			}
		}
	}

	pthread_exit(NULL);
}

void start_tty_handlers(int need_update) 
{
	char t[32];

	if (need_update & UPD_SILENT) {
		if (fd_tty_s != -1) { 
			deinit_term_silent();
			close(fd_tty_s);
		}
		
		sprintf(t, PATH_DEV "/tty%d", tty_s);
		fd_tty_s = open(t, O_RDWR);
		if (fd_tty_s == -1) {
			sprintf(t, PATH_DEV "/vc/%d", tty_s);
			fd_tty_s = open(t, O_RDWR);
			if (fd_tty_s == -1) {
				fprintf(stderr, "Can't open %s.\n", t);
				exit(2);
			}
		}

		init_term_silent(tty_s, fd_tty_s);
	}

	/* Do we have to start the monitor thread? */
	if (fd_evdev != -1 && need_update & UPD_MON) {
		if (pthread_create(&th_switchmon, NULL, &switch_evdev, NULL)) {
			fprintf(stderr, "Evdev monitor thread creation failed.\n");
			exit(3);
		}	
	} else if (fd_evdev == -1 && need_update & UPD_SILENT) {
		if (pthread_create(&th_switchmon, NULL, &switch_ttymon, NULL)) {
			fprintf(stderr, "tty monitor thread creation failed.\n");
			exit(3);
		}	
	}
}

void free_objs()
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
		
	free_objs();

	for (i = svcs.head; i != NULL; ) {
		j = i->next;
		free(i->p);
		free(i);
		i = j;
	}

	pthread_cancel(th_switchmon);
	pthread_kill(th_sighandler, SIGTERM);	
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

int reload_theme(void)
{
	item *i;
	theme_loaded = 0;
	
	if (config_file)
		free(config_file);

	config_file = get_cfg_file(arg_theme);
	
	if (!config_file)
		return -1;

	free_objs();
#ifdef CONFIG_TTF
	free_fonts();
#endif

	parse_cfg(config_file);
	if (load_images('a'))
		return -2;
	
#ifdef CONFIG_TTF
	load_fonts();
#endif
	for (i = svcs.head ; i != NULL; i = i->next) {
		svc_state *ss = (svc_state*)i->p;
		update_icon_status(ss->svc, ss->state);
	}
	
	return 0;
}

int cmd_set_theme(void **args)
{
	if (arg_theme)
		free(arg_theme);

	arg_theme = strdup(args[0]);
	pthread_mutex_lock(&mtx_theme);
	reload_theme();
	pthread_mutex_unlock(&mtx_theme);

	return 0;
}

void alloc_bg_buffer()
{
	int i = 0;

	/* Allocate the background buffer */
	if (!arg_export) {
		bg_buffer = malloc(fb_var.xres * fb_var.yres * bytespp);
		if (!bg_buffer) {
			printerr("Can't allocate background image buffer.\n");
			exit(1);
		}
	} else {
		fd_bg = open(arg_export, O_CREAT | O_TRUNC | O_RDWR, 0660);
		if (fd_bg == -1) {
			printerr("Can't open file for the background buffer.\n");
			exit(1);
		}
		lseek(fd_bg, fb_var.xres * fb_var.yres * bytespp, SEEK_SET);
		write(fd_bg, &i, 1);
		
		bg_buffer = mmap(NULL, fb_var.xres * fb_var.yres * bytespp, PROT_WRITE | PROT_READ,
				MAP_SHARED, fd_bg, 0);

		if (bg_buffer == MAP_FAILED) {
			printerr("Failed to mmap the background buffer.\n");
			close(fd_bg);
			exit(1);
		}
	}
}

void handle_silent_switch()
{
	struct fb_var_screeninfo old_var;
	struct fb_fix_screeninfo old_fix;
		
	old_fix = fb_fix;
	old_var = fb_var;

	/* FIXME: we should use the vc<->fb map here */		
	get_fb_settings(arg_fb);

//	/* Clear the screen */
//	write(fd_tty_s, "\e[2J", 4);

	if (arg_kdmode == KD_GRAPHICS)
		ioctl(fd_tty_s, KDSETMODE, KD_GRAPHICS);

	if (fb_fix.visual == FB_VISUAL_DIRECTCOLOR)
		set_directcolor_cmap(fd_fb);

	old_var.yoffset = fb_var.yoffset;
	old_var.xoffset = fb_var.xoffset;

	if (memcmp(&fb_fix, &old_fix, sizeof(struct fb_fix_screeninfo)) || 
	    memcmp(&fb_var, &old_var, sizeof(struct fb_var_screeninfo))) {

		pthread_mutex_lock(&mtx_theme);
		reload_theme();

		munmap(fb_mem, old_fix.line_length * old_var.yres);
		fb_mem = mmap(NULL, fb_fix.line_length * fb_var.yres, PROT_WRITE | PROT_READ,
			      MAP_SHARED, fd_fb, 0); 
	
		if (fb_mem == MAP_FAILED) {
			printerr("mmap() " PATH_DEV "/fb%d failed.\n", arg_fb);
			exit(1);	
		}

		pthread_mutex_lock(&mtx_bgbuf);
		if (bg_buffer)
			free(bg_buffer);
		alloc_bg_buffer();
		pthread_mutex_unlock(&mtx_bgbuf);
		pthread_mutex_unlock(&mtx_theme);
	}

	cmd_repaint(NULL);
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

	ioctl(fd_tty1, VT_ACTIVATE, n);
	ioctl(fd_tty1, VT_WAITACTIVE, n);

	return 0;
}

int cmd_set_tty(void **args)
{
	int upd = 0;
	
	if (*(int*)args[1] < 0 || *(int*)args[1] > MAX_NR_CONSOLES)
		return -1;
	
	if (!strcmp(args[0], "silent")) {
		if (tty_s == *(int*)args[1])
			return 0;
		
		pthread_mutex_lock(&mtx_tty);
		tty_s = *(int*)args[1];
		upd = UPD_SILENT;
		pthread_cancel(th_switchmon);
	} else if (!strcmp(args[0], "verbose")) {
		if (tty_v == *(int*)args[1])
			return 0;
	
		pthread_mutex_lock(&mtx_tty);
		tty_v = *(int*)args[1];
	} else {
		return -1;
	}

	/* FIXME: do we need to do anything to the previous terminal? */
	vt_cursor_enable(fd_tty_s);
	start_tty_handlers(upd);	
	vt_cursor_disable(fd_tty_s);

	pthread_mutex_unlock(&mtx_tty);

	return 0;
}

int cmd_set_event_dev(void **args)
{
	if (evdev)
		free(evdev);
	evdev = strdup(args[0]);
	
	pthread_cancel(th_switchmon);

	fd_evdev = open(evdev, O_RDONLY);
	start_tty_handlers(UPD_MON);
	
	return 0;
}

int cmd_set_notify(void **args)
{
	if (!strcmp(args[0], "repaint")) {
		if (notify[NOTIFY_REPAINT])
			free(notify[NOTIFY_REPAINT]);
		notify[NOTIFY_REPAINT] = strdup(args[1]);
	} else if (!strcmp(args[1], "paint")) {
		if (notify[NOTIFY_PAINT])
			free(notify[NOTIFY_PAINT]);
		notify[NOTIFY_PAINT] = strdup(args[1]);
	}
	
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

int cmd_paint(void **args)
{
	char i = 0, ret = 0;

	pthread_mutex_lock(&mtx_theme);
	if (!theme_loaded) {
		ret = -1;
		goto out2;
	}

	pthread_mutex_lock(&mtx_ctty);
	if (ctty != CTTY_SILENT)
		goto out1;

	if (fd_bg) {
		lseek(fd_bg, fb_var.xres * fb_var.yres * bytespp, SEEK_SET);
		write(fd_bg, &i, 1);
	}
	
/*	
 	memcpy(bg_buffer, silent_img.data, fb_var.xres * fb_var.yres * bytespp);
	render_objs('s', (u8*)bg_buffer, FB_SPLASH_IO_ORIG_USER);		
*/
	
	pthread_mutex_lock(&mtx_bgbuf);
	render_objs((u8*)bg_buffer, (u8*)silent_img.data, 's', FB_SPLASH_IO_ORIG_USER);

	if (notify[NOTIFY_PAINT])
		system(notify[NOTIFY_PAINT]);

	do_paint(fb_mem, bg_buffer);	
	pthread_mutex_unlock(&mtx_bgbuf);
out1:	pthread_mutex_unlock(&mtx_ctty);
out2:	pthread_mutex_unlock(&mtx_theme);
	return ret;
}

int cmd_repaint(void **args)
{
	char i = 0, ret = 0;

	pthread_mutex_lock(&mtx_theme);
	if (!theme_loaded) {
		ret = -1;
		goto out2;
	}

	pthread_mutex_lock(&mtx_ctty);
	if (ctty != CTTY_SILENT)
		goto out1;
	
	if (fd_bg) {
		lseek(fd_bg, fb_var.xres * fb_var.yres * bytespp, SEEK_SET);
		write(fd_bg, &i, 1);
	}
	
	pthread_mutex_lock(&mtx_bgbuf);
	memcpy(bg_buffer, silent_img.data, fb_var.xres * fb_var.yres * bytespp);
	render_objs((u8*)bg_buffer, NULL, 's', FB_SPLASH_IO_ORIG_USER);

	if (notify[NOTIFY_REPAINT])
		system(notify[NOTIFY_REPAINT]);
	
	put_img(fb_mem, bg_buffer);
	pthread_mutex_unlock(&mtx_bgbuf);
out1:	pthread_mutex_unlock(&mtx_ctty);
out2:	pthread_mutex_unlock(&mtx_theme);

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
#ifdef CONFIG_TTF
	if (boot_message)
		free(boot_message);
	boot_message = strdup(args[0]);
#endif
	return 0;
}

int cmd_paint_rect(void **args)
{
	rect re;
	int t, ret = 0;

	pthread_mutex_lock(&mtx_theme);
	if (!theme_loaded) {
		ret = -1;
		goto out2;
	}
	pthread_mutex_lock(&mtx_ctty);
	if (ctty != CTTY_SILENT)
		goto out1;

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

	pthread_mutex_lock(&mtx_bgbuf);
	do_paint_rect(fb_mem, bg_buffer, &re);
	pthread_mutex_unlock(&mtx_bgbuf);
	pthread_mutex_unlock(&mtx_ctty);
	pthread_mutex_unlock(&mtx_theme);
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

	{	.cmd = "set event dev",
		.handler = cmd_set_event_dev,
		.args = 1,
		.specs = "s"
	},
			
	{	.cmd = "set message",
		.handler = cmd_set_mesg,
		.args = 1,
		.specs = "s"
	},
	
	{	.cmd = "set notify",
		.handler = cmd_set_notify,
		.args = 2,
		.specs = "ss"
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
			if (errno == EINTR)
				continue;
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
	struct vt_stat vtstat;
	sigset_t sigset;

	/* Create a mmap of the framebuffer */
	fd_fb = open_fb();
	if (!fd_fb)
		exit(1);

	alloc_bg_buffer();
	
	fb_mem = mmap(NULL, fb_fix.line_length * fb_var.yres, 
		      PROT_WRITE | PROT_READ, MAP_SHARED, fd_fb, 0); 
	
	if (fb_mem == MAP_FAILED) {
		printerr("mmap() " PATH_DEV "/fb%d failed.\n", arg_fb);
		close(fd_fb);
		exit(1);	
	}

	fd_tty1 = open(PATH_DEV "/tty1", O_RDWR);
	if (fd_tty1 == -1) {
		fd_tty1 = open(PATH_DEV "/vc/1", O_RDWR);
		if (fd_tty1 == -1) {
			fprintf(stderr, "Can't open " PATH_DEV "/tty1.\n");
			exit(2);
		}
	}

	/* Create the splash FIFO if it's not already in place. */
	if (stat(SPLASH_FIFO, &mystat) == -1 || !S_ISFIFO(mystat.st_mode)) {
		unlink(SPLASH_FIFO);
		if (mkfifo(SPLASH_FIFO, 0700))
			exit(3);
	}

	/* No one is being notified about anything by default. */
	for (i = 0; i < 2; i++) {
		notify[i] = NULL;
	}

	/* Go into background. */
	i = fork();
	if (i)
		exit(0);

#ifdef CONFIG_TTF
	load_fonts();
#endif
	
	/* arg_theme is a reference to argv[x] of the original splash_util
	 * process. We're freeing arg_theme in cmd_set_theme(), so the strdup()
	 * call is necessary to make sure things don't break there. */
	arg_theme = strdup(arg_theme);
	setsid();

	signal(SIGABRT, SIG_IGN);
	signal(SIGINT,  SIG_IGN);

	/* These signals will be handled by the sighandler thread. */
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR1);
	sigaddset(&sigset, SIGUSR2);
	sigaddset(&sigset, SIGTERM);
	sigprocmask(SIG_BLOCK, &sigset, NULL);
	pthread_create(&th_sighandler, NULL, &sighandler, NULL);
	
	start_tty_handlers(UPD_ALL);

	pthread_mutex_lock(&mtx_ctty);
	if (ioctl(fd_tty_s, VT_GETSTATE, &vtstat) != -1) {
		if (vtstat.v_active == tty_s) {
			ctty = CTTY_SILENT;
		} else {
			ctty = CTTY_VERBOSE;
		}
	}
	pthread_mutex_unlock(&mtx_ctty);

	daemon_comm();	
	exit(0);
}

