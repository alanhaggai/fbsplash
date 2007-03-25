/*
 * daemon.c - The splash daemon
 *
 * Copyright (C) 2005-2006 Michal Januszewski <spock@gentoo.org>
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
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <pthread.h>
#include <errno.h>

#include "splash.h"
#include "daemon.h"

/* Threading structures */
pthread_mutex_t mtx_tty = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx_paint = PTHREAD_MUTEX_INITIALIZER;
pthread_t th_switchmon, th_sighandler, th_anim;

int ctty = CTTY_VERBOSE;
int tty_s = TTY_SILENT;
int tty_v = TTY_VERBOSE;

/* File descriptors */
int fd_tty_s = -1;
int fd_tty1 = -1;
int fd_tty0 = -1;
int fd_evdev = -1;
int fd_fb = -1;
int fd_bg = -1;

/* In-memory buffers */
u8 *fb_mem = NULL;
u8 *bg_buffer = NULL;

/* Misc settings */
char *notify[2];
char *evdev = NULL;

/* Service list */
list svcs = { NULL, NULL };
u8 theme_loaded = 0;

/* A container for the original settings of the silent TTY. */
struct termios tios;

#ifdef CONFIG_MNG

/*
 * Renders an animation frame directly to the screen.
 */
void anim_render_frame(anim *a)
{
	int ret;
	mng_anim *mng;

	mng = mng_get_userdata(a->mng);
	mng->wait_msecs = 0;
	memset(&mng->start_time, 0, sizeof(struct timeval));

	/* FIXME: This is a workaround for what seems to be a bug in libmng.
	 * Either we clear the canvas ourselves, or parts of the previous frame
	 * will remain in it after rendering the current one. */
	memset(mng->canvas, 0, mng->canvas_h * mng->canvas_w * mng->canvas_bytes_pp);
	ret = mng_render_next(a->mng);
	if (ret == MNG_NOERROR) {
		if (a->flags & F_ANIM_ONCE) {
			a->status = F_ANIM_STATUS_DONE;
		} else {
			mng_display_restart(a->mng);
		}
	}

	if ((ret == MNG_NOERROR || ret == MNG_NEEDTIMERWAIT) && ctty == CTTY_SILENT) {
		mng_display_buf(a->mng, bg_buffer, fb_mem, a->x, a->y,
				fb_fix.line_length, fb_var.xres * bytespp);
	}
}

/*
 * Display all animations of the type 'once' or 'loop'.
 */
void *thf_anim(void *unused)
{
	anim *a = NULL, *ca;
	item *i;
	mng_anim *mng;
	int delay = 10000, oldstate;

	/* Render the first frame of all animations on the screen. */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
	pthread_mutex_lock(&mtx_paint);
	for (i = anims.head; i != NULL; i = i->next) {
		ca = i->p;

		if ((ca->flags & F_ANIM_METHOD_MASK) == F_ANIM_PROPORTIONAL)
			continue;

		mng = mng_get_userdata(ca->mng);
		if (!mng->displayed_first)
			anim_render_frame(ca);
	}
	pthread_mutex_unlock(&mtx_paint);
	pthread_setcancelstate(oldstate, NULL);

	while(1) {
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
		pthread_mutex_lock(&mtx_paint);
		/* Find the shortest delay. */
		for (i = anims.head; i != NULL; i = i->next) {
			ca = i->p;

			if ((ca->flags & F_ANIM_METHOD_MASK) == F_ANIM_PROPORTIONAL ||
			    ca->status == F_ANIM_STATUS_DONE)
				continue;

			mng = mng_get_userdata(ca->mng);

			if (mng->wait_msecs < delay && mng->wait_msecs > 0) {
				delay = mng->wait_msecs;
				a = ca;
			}
		}
		pthread_mutex_unlock(&mtx_paint);
		pthread_setcancelstate(oldstate, NULL);

		usleep(delay * 1000);

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
		pthread_mutex_lock(&mtx_paint);
		/* Don't paint anything if we aren't in silent mode. */
		if (ctty != CTTY_SILENT)
			goto next;

		if (a)
			anim_render_frame(a);

		/* Update the wait time for all relevant animation objects. */
		for (i = anims.head ; i != NULL; i = i->next) {
			ca = i->p;

			if ((ca->flags & F_ANIM_METHOD_MASK) == F_ANIM_PROPORTIONAL ||
			    ca->status == F_ANIM_STATUS_DONE || ca == a)
				continue;

			mng = mng_get_userdata(ca->mng);
			if (mng->wait_msecs > 0) {
				mng->wait_msecs -= delay;
				if (mng->wait_msecs <= 0)
					anim_render_frame(ca);
			}
		}

next:	pthread_mutex_unlock(&mtx_paint);
		pthread_setcancelstate(oldstate, NULL);

		a = NULL;
		delay = 10000;
	}
}
#endif /* CONFIG_MNG */

/*
 * The following two functions are called with
 * mtx_tty held.
 */
void vt_silent_init(void)
{
	struct vt_mode vt;
	struct termios w;

	ioctl(fd_tty_s, TIOCSCTTY, 0);

	vt.mode   = VT_PROCESS;
	vt.waitv  = 0;
	vt.relsig = SIGUSR1;
	vt.acqsig = SIGUSR2;
	ioctl(fd_tty_s, VT_SETMODE, &vt);

	tcgetattr(fd_tty_s,&tios);
	w = tios;
	w.c_lflag &= ~(ICANON|ECHO);
	w.c_cc[VTIME] = 0;
	w.c_cc[VMIN] = 1;
	tcsetattr(fd_tty_s, TCSANOW, &w);

	vt_cursor_disable(fd_tty_s);
	return;
}

void vt_silent_cleanup(void)
{
	struct vt_mode vt;

	vt.mode   = VT_AUTO;
	vt.waitv  = 0;

	tcsetattr(fd_tty_s, TCSANOW, &tios);
	ioctl(fd_tty_s, VT_RELDISP, 1);
	ioctl(fd_tty_s, KDSETMODE, KD_TEXT);
	ioctl(fd_tty_s, VT_SETMODE, &vt);

	vt_cursor_enable(fd_tty_s);
	return;
}

/*
 * Handles a switch to the silent mode.
 */
void switch_silent()
{
	struct fb_var_screeninfo old_var;
	struct fb_fix_screeninfo old_fix;

	old_fix = fb_fix;
	old_var = fb_var;

	/* FIXME: we should use the vc<->fb map here */
	get_fb_settings(arg_fb);

	/* Set KD_GRAPHICS if necessary. */
	if (arg_kdmode == KD_GRAPHICS)
		ioctl(fd_tty_s, KDSETMODE, KD_GRAPHICS);

	/* Update CMAP if we're in a DIRECTCOLOR mode. */
	if (fb_fix.visual == FB_VISUAL_DIRECTCOLOR)
		set_directcolor_cmap(fd_fb);

	old_var.yoffset = fb_var.yoffset;
	old_var.xoffset = fb_var.xoffset;

	/*
	 * Has the video mode changed? If it has, we'll have to reload
	 * the theme.
	 */
	if (memcmp(&fb_fix, &old_fix, sizeof(struct fb_fix_screeninfo)) ||
	    memcmp(&fb_var, &old_var, sizeof(struct fb_var_screeninfo))) {

		pthread_mutex_lock(&mtx_paint);

		if (reload_theme()) {
			fprintf(stderr, "Failed to (re-)load the '%s' theme.\n", arg_theme);
			exit(1);
		}

		munmap(fb_mem, old_fix.line_length * old_var.yres);
		fb_mem = mmap(NULL, fb_fix.line_length * fb_var.yres, PROT_WRITE | PROT_READ,
			      MAP_SHARED, fd_fb, 0);

		if (fb_mem == MAP_FAILED) {
			printerr("mmap() " PATH_DEV "/fb%d failed.\n", arg_fb);
			exit(1);
		}

		if (bg_buffer)
			free(bg_buffer);
		bgbuffer_alloc();

		pthread_mutex_unlock(&mtx_paint);
	}

	cmd_repaint(NULL);
}

static void do_cleanup(void)
{
	pthread_mutex_trylock(&mtx_tty);
	vt_silent_cleanup();
	vt_cursor_enable(fd_tty_s);
}

/*
 * Signal handler.
 *
 * This thread is reponsible for allowing switches between the
 * silent and verbose ttys, and for cleanup tasks after reception
 * of SIGTERM.
 */
void* thf_sighandler(void *unusued)
{
	sigset_t sigset;
	int sig;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR1);
	sigaddset(&sigset, SIGUSR2);
	sigaddset(&sigset, SIGTERM);
	sigaddset(&sigset, SIGINT);

	while (1) {
		sigwait(&sigset, &sig);

		/* Switch from silent to verbose. */
		if (sig == SIGUSR1) {
			pthread_mutex_lock(&mtx_paint);
			pthread_mutex_lock(&mtx_tty);
			ioctl(fd_tty_s, VT_RELDISP, 1);
			pthread_mutex_unlock(&mtx_tty);

			ctty = CTTY_VERBOSE;
			pthread_mutex_unlock(&mtx_paint);
		/* Switch back to silent. */
		} else if (sig == SIGUSR2) {
			pthread_mutex_lock(&mtx_paint);
			pthread_mutex_lock(&mtx_tty);
			ioctl(fd_tty_s, VT_RELDISP, 2);
			pthread_mutex_unlock(&mtx_tty);

			ctty = CTTY_SILENT;
			pthread_mutex_unlock(&mtx_paint);

			switch_silent();
		} else if (sig == SIGINT) {
			/* internally generated terminate signal */
			do_cleanup();
			pthread_exit(NULL);
		} else if (sig == SIGTERM) {
			do_cleanup();
			exit(0);
		}
	}
}

/*
 * Event device monitor thread.
 */
void* thf_switch_evdev(void *unused)
{
	int i, h, oldstate;
	size_t rb;
	struct input_event ev[8];

	while (1) {
		rb = read(fd_evdev, ev, sizeof(struct input_event)*8);
    	if (rb < (int) sizeof(struct input_event))
			continue;

		for (i = 0; i < (int) (rb / sizeof(struct input_event)); i++) {
			if (ev[i].type != EV_KEY || ev[i].value != 0 || ev[i].code != KEY_F2)
				continue;

			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
			pthread_mutex_lock(&mtx_paint);
			if (ctty == CTTY_SILENT) {
				h = tty_v;
			} else {
				h = tty_s;
			}
			pthread_mutex_unlock(&mtx_paint);
			pthread_setcancelstate(oldstate, NULL);

			/* Switch to the new tty. This ioctl has to be done on
			 * the silent tty. Sometimes init will mess with the
			 * settings of the verbose console which will prevent
			 * console switching from working properly.
			 *
			 * Don't worry about fd_tty_s not being protected by a
			 * mutex -- this thread is always killed before any changes
			 * are made to fd_tty_s.
			 */
			ioctl(fd_tty_s, VT_ACTIVATE, h);
		}
	}

	pthread_exit(NULL);
}

/*
 * Silent TTY monitor thread.
 *
 * This thread listens for F2 keypresses on the silent TTY.
 * We don't have to worry about fd_tty_s not being protected
 * by the mtx_tty mutex, since this thread is killed before
 * a new silent tty is opened.
 */
void* thf_switch_ttymon(void *unused)
{
	int flags, oldstate;

	flags = fcntl(fd_tty_s, F_GETFL, 0);

	while(1) {
		char ret = 0xff;
		int t = 0;

		fcntl(fd_tty_s, F_SETFL, flags & (~O_NDELAY));
		read(fd_tty_s, &ret, 1);

		if (ret == '\x1b') {
			fcntl(fd_tty_s, F_SETFL, flags | O_NDELAY);

			/* FIXME: is <F2> always 1b5b5b42? */
			if (read(fd_tty_s, &t, 3) == 3 &&
			    ((endianess == little && t == 0x425b5b) ||
			     (endianess == big && 0x5b5b4200))) {
				pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
				pthread_mutex_lock(&mtx_tty);
				ioctl(fd_tty_s, VT_ACTIVATE, tty_v);
				pthread_mutex_unlock(&mtx_tty);
				pthread_setcancelstate(oldstate, NULL);
//				ioctl(fd_tty_s, VT_WAITACTIVE, tty_v);
			}
		}
	}

	pthread_exit(NULL);
}

/*
 * Start a keypress monitoring thread and reopen switch
 * to a new silent TTY.
 *
 * When called with UPD_SILENT, mtx_tty should be held.
 */
void switchmon_start(int update)
{
	char t[32];

	/* Has the silent TTY changed? */
	if (update & UPD_SILENT) {
		/*
		 * Close the previous silent TTY and restore its
		 * normal settings.
		 */
		if (fd_tty_s != -1) {
			vt_silent_cleanup();
			close(fd_tty_s);
		}

		/* Open a new silent TTY and initialize it. */
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

		vt_silent_init();
	}

	/* Do we have to start a monitor thread? */
	if (fd_evdev != -1 && (update & UPD_MON)) {
		if (pthread_create(&th_switchmon, NULL, &thf_switch_evdev, NULL)) {
			fprintf(stderr, "Evdev monitor thread creation failed.\n");
			exit(3);
		}

	/* We start the standard monitoring thread only if no event device
	 * has been defined and if the silent TTY has just been changed. */
	} else if (fd_evdev == -1 && (update & UPD_SILENT)) {
		if (pthread_create(&th_switchmon, NULL, &thf_switch_ttymon, NULL)) {
			fprintf(stderr, "TTY monitor thread creation failed.\n");
			exit(3);
		}
	}
}

/*
 * Free all objects.
 */
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

/*
 * Update icons after a service status change.
 */
void icon_update_status(char *svc, enum ESVC state)
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

/*
 * Allocates a background buffer.
 */
void bgbuffer_alloc()
{
	int i = 0;

	/*
	 * If we're not going to export the memory buffer to a file,
	 * simply allocate a chunk of memory large enough to hold the
	 * contents of the screen.
	 */
	if (!arg_export) {
		bg_buffer = malloc(fb_var.xres * fb_var.yres * bytespp);
		if (!bg_buffer) {
			fprintf(stderr, "Can't allocate background image buffer.\n");
			exit(1);
		}
	} else {
		fd_bg = open(arg_export, O_CREAT | O_TRUNC | O_RDWR, 0660);
		if (fd_bg == -1) {
			fprintf(stderr, "Can't open file for the background buffer.\n");
			exit(1);
		}
		lseek(fd_bg, fb_var.xres * fb_var.yres * bytespp, SEEK_SET);
		write(fd_bg, &i, 1);

		bg_buffer = mmap(NULL, fb_var.xres * fb_var.yres * bytespp, PROT_WRITE | PROT_READ,
						 MAP_SHARED, fd_bg, 0);

		if (bg_buffer == MAP_FAILED) {
			fprintf(stderr, "Failed to mmap the background buffer.\n");
			close(fd_bg);
			exit(1);
		}
	}
}

/*
 * Load a new theme.
 *
 */
int reload_theme(void)
{
	item *i;
	theme_loaded = 0;

#ifdef CONFIG_MNG
	/* The anim thread will be restarted when we're done
	 * reloading the theme. */
	pthread_cancel(th_anim);
#endif

	if (config_file)
		free(config_file);

	config_file = get_cfg_file(arg_theme);
	if (!config_file)
		return -1;

	/* Free all objects associated with the previous theme. */
	free_objs();
#ifdef CONFIG_TTF
	free_fonts();
#endif

	/* Parse a new config file. */
	parse_cfg(config_file);
	if (load_images('s'))
		return -2;

#ifdef CONFIG_TTF
	load_fonts();
#endif
	for (i = svcs.head ; i != NULL; i = i->next) {
		svc_state *ss = (svc_state*)i->p;
		icon_update_status(ss->svc, ss->state);
	}

#ifdef CONFIG_MNG
	pthread_create(&th_anim, NULL, &thf_anim, NULL);
#endif
	return 0;
}

/*
 * Start the splash daemon.
 */
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

	bgbuffer_alloc();

	fb_mem = mmap(NULL, fb_fix.line_length * fb_var.yres,
		      PROT_WRITE | PROT_READ, MAP_SHARED, fd_fb, 0);

	if (fb_mem == MAP_FAILED) {
		iprint(MSG_ERROR, "mmap() " PATH_DEV "/fb%d failed.\n", arg_fb);
		close(fd_fb);
		exit(1);
	}

	/* Create the splash FIFO if it's not already in place. */
	if (stat(SPLASH_FIFO, &mystat) == -1 || !S_ISFIFO(mystat.st_mode)) {
		unlink(SPLASH_FIFO);
		if (mkfifo(SPLASH_FIFO, 0700)) {
			iprint(MSG_ERROR, "mkfifo("SPLASH_FIFO") failed.\n");
			exit(3);
		}
	}

	/* No one is being notified about anything by default. */
	for (i = 0; i < 2; i++) {
		notify[i] = NULL;
	}

	fd_tty0 = open(PATH_DEV "/tty0", O_RDWR);
	if (fd_tty0 == -1) {
		fd_tty0 = open(PATH_DEV "/vc/0", O_RDWR);
		if (fd_tty0 == -1) {
			iprint(MSG_ERROR, "Can't open " PATH_DEV "/tty1.\n");
			exit(2);
		}
	}

	fd_tty1 = open(PATH_DEV "/tty1", O_RDWR);
	if (fd_tty1 == -1) {
		fd_tty1 = open(PATH_DEV "/vc/1", O_RDWR);
		if (fd_tty1 == -1) {
			iprint(MSG_ERROR, "Can't open " PATH_DEV "/tty1.\n");
			exit(2);
		}
	}

	/* Go into background. */
	i = fork();
	if (i) {
		if (arg_pidfile) {
			FILE *fp = fopen(arg_pidfile, "w");
			if (!fp) {
				iprint(MSG_ERROR, "Failed to open pidfile %s for writing.\n", arg_pidfile);
			} else {
				fprintf(fp, "%d\n", i);
				fclose(fp);
			}
		}
		exit(0);
	}

	setsid();
	chdir("/");

	/* Make /dev/null stdin, and /dev/console stdout/stderr */
	i=open("/dev/null", O_RDWR);
	dup2(i, 0);
	i=open("/dev/console", O_RDWR);
	dup2(i, 1);
	dup2(i, 2);

	/* arg_theme is a reference to argv[x] of the original splash_util
	 * process. We're freeing arg_theme in cmd_set_theme(), so the strdup()
	 * call is necessary to make sure things don't break there. */
	arg_theme = strdup(arg_theme);

#ifdef CONFIG_TTF
	load_fonts();
#endif

	signal(SIGABRT, SIG_IGN);

	/* These signals will be handled by the sighandler thread. */
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR1);
	sigaddset(&sigset, SIGUSR2);
	sigaddset(&sigset, SIGTERM);
	sigaddset(&sigset, SIGINT);
	sigprocmask(SIG_BLOCK, &sigset, NULL);
	pthread_mutex_lock(&mtx_paint);
	pthread_create(&th_sighandler, NULL, &thf_sighandler, NULL);

	/* Check which TTY is active */
	if (ioctl(fd_tty0, VT_GETSTATE, &vtstat) != -1) {
		if (vtstat.v_active == tty_s) {
			ctty = CTTY_SILENT;
		} else {
			ctty = CTTY_VERBOSE;
		}
	}
	pthread_mutex_unlock(&mtx_paint);

#ifdef CONFIG_MNG
	/* Start the animation thread */
	pthread_create(&th_anim, NULL, &thf_anim, NULL);
#endif

	pthread_mutex_lock(&mtx_tty);
	switchmon_start(UPD_ALL);
	pthread_mutex_unlock(&mtx_tty);

	daemon_comm();
	exit(0);
}

