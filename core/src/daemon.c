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
#include <dirent.h>
#include <time.h>
#include <getopt.h>

#include "common.h"
#include "daemon.h"

/* Threading structures */
pthread_mutex_t mtx_tty = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx_paint = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx_anim = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cnd_anim  = PTHREAD_COND_INITIALIZER;

pthread_t th_switchmon, th_sighandler, th_anim;

int ctty = CTTY_VERBOSE;

/* File descriptors */
int fd_evdev = -1;
#ifdef CONFIG_GPM
int fd_gpm = -1;
#endif

char		*arg_pidfile = NULL;
stheme_t *theme;

/* Misc settings */
char *notify[2];
char *evdev = NULL;

/* Service list */
list svcs = { NULL, NULL };

/* A container for the original settings of the silent TTY. */
struct termios tios;

#if WANT_MNG
/*
 * Display all animations of the type 'once' or 'loop'.
 */
void *thf_anim(void *unused)
{
	anim *ca;
	obj *co;
	item *i;
	mng_anim *mng;
	int delay = 10000, rdelay, oldstate;
	struct timespec ts, tsc;

	while(1) {
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
		pthread_mutex_lock(&mtx_paint);
		/* Find the shortest delay. */
		for (i = theme->anims.head; i != NULL; i = i->next) {
			ca = i->p;
			co = container_of(ca);

			if (!co->visible ||
				(ca->flags & F_ANIM_METHOD_MASK) == F_ANIM_PROPORTIONAL ||
			    ca->status == F_ANIM_STATUS_DONE)
				continue;

			mng = mng_get_userdata(ca->mng);

			/* If this is a new animation (activated by a service),
			 * display it immediately. */
			if (!mng->displayed_first) {
				anim_render_canvas(ca);

				if (ctty == CTTY_SILENT)
					fbsplashr_render_screen(theme, true, false, FBSPL_EFF_NONE);
			}

			if (mng->wait_msecs < delay && mng->wait_msecs > 0) {
				delay = mng->wait_msecs;
			}
		}
		pthread_mutex_unlock(&mtx_paint);
		pthread_setcancelstate(oldstate, NULL);

		pthread_mutex_lock(&mtx_anim);
		clock_gettime(CLOCK_REALTIME, &ts);

		ts.tv_sec  += (int)(delay / 1000);
		ts.tv_nsec += (delay % 1000) * 1000000;

		/* Check for overflow of the nanoseconds field */
		if (ts.tv_nsec >= 1000000000) {
			ts.tv_sec++;
			ts.tv_nsec -= 1000000000;
		}

		pthread_cond_timedwait(&cnd_anim, &mtx_anim, &ts);
		pthread_mutex_unlock(&mtx_anim);

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
		pthread_mutex_lock(&mtx_paint);
		/* Don't paint anything if we aren't in silent mode. */
		if (ctty != CTTY_SILENT)
			goto next;

		/* Calculate the real delay. We might have been signalled by
		 * the splash daemon before 'delay' msecs passed. */
		clock_gettime(CLOCK_REALTIME, &tsc);
		rdelay = delay + (tsc.tv_sec - ts.tv_sec)*1000 + (tsc.tv_nsec - ts.tv_nsec)/1000000;

		/* Update the wait time for all relevant animation objects. */
		for (i = theme->anims.head ; i != NULL; i = i->next) {
			ca = i->p;
			co = container_of(ca);

			if (!co->visible || (ca->flags & F_ANIM_METHOD_MASK) == F_ANIM_PROPORTIONAL ||
			    ca->status == F_ANIM_STATUS_DONE)
				continue;

			mng = mng_get_userdata(ca->mng);
			if (mng->wait_msecs > 0) {
				mng->wait_msecs -= rdelay;
				if (mng->wait_msecs <= 0)
					anim_render_canvas(ca);
			}
		}
		fbsplashr_render_screen(theme, true, false, FBSPL_EFF_NONE);

next:	pthread_mutex_unlock(&mtx_paint);
		pthread_setcancelstate(oldstate, NULL);

		/* Default delay is 10s */
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

	fbsplashr_tty_silent_init(false);
	ioctl(fd_tty[config.tty_s], TIOCSCTTY, 0);

	vt.mode   = VT_PROCESS;
	vt.waitv  = 0;
	vt.relsig = SIGUSR1;
	vt.acqsig = SIGUSR2;
	ioctl(fd_tty[config.tty_s], VT_SETMODE, &vt);

	return;
}

void vt_silent_cleanup(void)
{
	struct vt_mode vt;

	vt.mode   = VT_AUTO;
	vt.waitv  = 0;

	tcsetattr(fd_tty[config.tty_s], TCSANOW, &tios);
	ioctl(fd_tty[config.tty_s], VT_RELDISP, 1);
	ioctl(fd_tty[config.tty_s], KDSETMODE, KD_TEXT);
	ioctl(fd_tty[config.tty_s], VT_SETMODE, &vt);

	fbsplashr_tty_silent_cleanup();
	return;
}

/*
 * Handles a switch to the silent mode.
 */
void switch_silent()
{
	pthread_mutex_lock(&mtx_paint);

	if (fbsplashr_tty_silent_update()) {
		if (reload_theme()) {
			iprint(MSG_ERROR, "Failed to (re-)load the '%s' theme.\n", config.theme);
			exit(1);
		}
	}

	/* Set KD_GRAPHICS if necessary. */
	if (config.kdmode == KD_GRAPHICS)
		ioctl(fd_tty[config.tty_s], KDSETMODE, KD_GRAPHICS);

	pthread_mutex_unlock(&mtx_paint);
	cmd_repaint(NULL);
}

static void do_cleanup(void)
{
	pthread_mutex_trylock(&mtx_tty);
#ifdef CONFIG_GPM
	if (fd_gpm >= 0) {
		Gpm_Close();
	}
#endif
	vt_silent_cleanup();
}

/*
 * A dummy handler for SIGALRM.
 */
void handler_alarm(int unused)
{
	return;
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
			ioctl(fd_tty[config.tty_s], VT_RELDISP, 1);
			pthread_mutex_unlock(&mtx_tty);

			ctty = CTTY_VERBOSE;
			pthread_mutex_unlock(&mtx_paint);
		/* Switch back to silent. */
		} else if (sig == SIGUSR2) {
			pthread_mutex_lock(&mtx_paint);
			pthread_mutex_lock(&mtx_tty);
			ioctl(fd_tty[config.tty_s], VT_RELDISP, 2);
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
				h = config.tty_v;
			} else {
				h = config.tty_s;
			}
			pthread_mutex_unlock(&mtx_paint);
			pthread_setcancelstate(oldstate, NULL);

			/* Switch to the new tty. This ioctl has to be done on
			 * the silent tty. Sometimes init will mess with the
			 * settings of the verbose console which will prevent
			 * console switching from working properly.
			 *
			 * Don't worry about fd_tty[config.tty_s] not being protected by a
			 * mutex -- this thread is always killed before any changes
			 * are made to fd_tty[config.tty_s].
			 */
			ioctl(fd_tty[config.tty_s], VT_ACTIVATE, h);
		}
	}

	pthread_exit(NULL);
}

/*
 * Silent TTY monitor thread.
 *
 * This thread listens for F2 keypresses on the silent TTY.
 * We don't have to worry about fd_tty[config.tty_s] not being protected
 * by the mtx_tty mutex, since this thread is killed before
 * a new silent tty is opened.
 */
void* thf_switch_ttymon(void *unused)
{
	int flags, oldstate;

	flags = fcntl(fd_tty[config.tty_s], F_GETFL, 0);

	while(1) {
		char ret = 0xff;
		int t = 0;

		fcntl(fd_tty[config.tty_s], F_SETFL, flags & (~O_NDELAY));
		read(fd_tty[config.tty_s], &ret, 1);

		if (ret == '\x1b') {
			fcntl(fd_tty[config.tty_s], F_SETFL, flags | O_NDELAY);

			/* FIXME: is <F2> always 1b5b5b42? */
			if (read(fd_tty[config.tty_s], &t, 3) == 3 &&
			    ((endianess == little && t == 0x425b5b) ||
			     (endianess == big && 0x5b5b4200))) {
				pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
				pthread_mutex_lock(&mtx_tty);
				ioctl(fd_tty0, VT_ACTIVATE, config.tty_v);
				pthread_mutex_unlock(&mtx_tty);
				pthread_setcancelstate(oldstate, NULL);
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
void switchmon_start(int update, int stty)
{
	/* Has the silent TTY changed? */
	if (update & UPD_SILENT) {
		if (config.tty_s != stty) {
			vt_silent_cleanup();
			fbsplashr_tty_silent_set(stty);
		}
		vt_silent_init();
	}

	/* Do we have to start a monitor thread? */
	if (update & UPD_MON) {
		if (fd_evdev != -1) {
			if (pthread_create(&th_switchmon, NULL, &thf_switch_evdev, NULL)) {
				iprint(MSG_ERROR, "Evdev monitor thread creation failed.\n");
				exit(3);
			}
		} else {
			if (pthread_create(&th_switchmon, NULL, &thf_switch_ttymon, NULL)) {
				iprint(MSG_ERROR, "TTY monitor thread creation failed.\n");
				exit(3);
			}
		}
	}
}

/*
 * Load a new theme.
 */
int reload_theme(void)
{
	item *i;

	fbsplashr_theme_free(theme);
	theme = fbsplashr_theme_load();

	for (i = svcs.head ; i != NULL; i = i->next) {
		svc_state *ss = (svc_state*)i->p;
		invalidate_service(theme, ss->svc, ss->state);
	}

	return 0;
}

static int dcr_filter(const struct dirent *dre)
{
	int pid;

	if (sscanf(dre->d_name, "%d", &pid) == 1)
		return 1;
	else
		return 0;
}

static int daemon_check_running(const char *pname)
{
	struct dirent **namelist;
	FILE *fp;
	char name[128];
	char buf[128];
	int n, pid, fpid = 0, mpid = getpid();
	int l = min(strlen(pname), 15);

	n = scandir("/proc", &namelist, dcr_filter, alphasort);
	if (n < 0)
		perror("blah");
	else {
		while(n--) {
			snprintf(name, 128, "/proc/%s/stat", namelist[n]->d_name);
			if ((fp = fopen(name, "r")) != NULL) {
				if ((fscanf(fp, "%d (%s)", &pid, buf) == 2) && mpid != pid && !strncmp(buf, pname, l)) {
					fpid = pid;
				}
				fclose(fp);
			}

			free(namelist[n]);
		}
		free(namelist);
	}

	return fpid;
}

/*
 * Start the splash daemon.
 */
void daemon_start()
{
	int i = 0;
	FILE *fp_fifo = NULL;
	struct stat mystat;
	struct vt_stat vtstat;
	struct sigaction sa;
	sigset_t sigset;

	if (!config.minstances && (i = daemon_check_running("fbsplashd"))) {
		iprint(MSG_ERROR, "It looks like there's another instance of the splash daemon running (pid %d).\n", i);
		iprint(MSG_ERROR, "Stop it first or run this program with `--minstances'.\n");
		exit(1);
	}

	/* No one is being notified about anything by default. */
	for (i = 0; i < 2; i++) {
		notify[i] = NULL;
	}

	/* Create the splash FIFO if it's not already in place. */
	if (stat(FBSPLASH_FIFO, &mystat) == -1 || !S_ISFIFO(mystat.st_mode)) {
		unlink(FBSPLASH_FIFO);
		if (mkfifo(FBSPLASH_FIFO, 0700)) {
			iprint(MSG_ERROR, "mkfifo("FBSPLASH_FIFO") failed.\n");
			exit(3);
		}
	}

	while (!fp_fifo) {
		fp_fifo = fopen(FBSPLASH_FIFO, "r+");
		if (!fp_fifo) {
			if (errno == EINTR)
				continue;
			iprint(MSG_ERROR, "Can't open the splash FIFO (" FBSPLASH_FIFO ") for reading: %s\n", strerror(errno));
			exit(4);
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
	i = open("/dev/null", O_RDWR);
	dup2(i, 0);
	i = open("/dev/console", O_RDWR);
	dup2(i, 1);
	dup2(i, 2);

	/* Make all our threads ignore these signals. SIGUSR1, SIGUSR2,
	 * SIGTERM and SIGINT will be handled in the sighandler thread.
	 * The use of a separate thread for handling signals is required
	 * in order to avoid potential deadlocks. */
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGABRT);
	sigaddset(&sigset, SIGUSR1);
	sigaddset(&sigset, SIGUSR2);
	sigaddset(&sigset, SIGTERM);
	sigaddset(&sigset, SIGINT);
	pthread_sigmask(SIG_BLOCK, &sigset, NULL);
	pthread_mutex_lock(&mtx_paint);
	pthread_create(&th_sighandler, NULL, &thf_sighandler, NULL);

	/* Setup a dummy handler for SIGALRM. Unlike the other signals,
	 * we don't care which thread executes this handler. */
	sa.sa_handler = handler_alarm;
	sa.sa_flags = SA_RESTART;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGALRM, &sa, NULL);

	/* Check which TTY is active */
	if (ioctl(fd_tty0, VT_GETSTATE, &vtstat) != -1) {
		if (vtstat.v_active == config.tty_s) {
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
	switchmon_start(UPD_ALL, config.tty_s);
	pthread_mutex_unlock(&mtx_tty);

	daemon_comm(fp_fifo);
	exit(0);
}

static struct option options[] = {
	{ "theme",	required_argument, NULL, 0x100 },
	{ "progress",required_argument, NULL, 0x101 },
	{ "kdgraphics", no_argument, NULL, 0x102 },
#ifdef CONFIG_TTF
	{ "mesg",	required_argument, NULL, 0x103 },
#endif
	{ "pidfile",required_argument, NULL, 0x104 },
	{ "minstances", no_argument, NULL, 0x105 },
	{ "effects", required_argument, NULL, 0x106 },
	{ "type", required_argument, NULL, 0x107 },
	{ "help",	no_argument, NULL, 'h'},
	{ "verbose", no_argument, NULL, 'v'},
	{ "quiet",  no_argument, NULL, 'q'},
};

static void usage()
{
	printf(
"fbsplashd/splashutils-" PACKAGE_VERSION "\n"
"Usage: fbsplashd [options]\n\n"
"Options:\n"
"  -h, --help          show this help message\n"
"  -t, --theme=THEME   use theme THEME\n"
"  -p, --progress=NUM  set progress to NUM/65535 * 100%%\n"
"      --kdgraphics    use KD_GRAPHICS mode for the splash screen\n"
"  -v, --verbose       display verbose error messages\n"
"  -q, --quiet         don't display any messages\n"
#ifdef CONFIG_TTF
"      --mesg=TEXT     use TEXT as the main splash message\n"
#endif
"      --pidfile=FILE  save the PID of the daemon to FILE\n"
"      --minstances    allow multiple instances of the splash daemon\n"
"      --effects=LIST  a comma-separated list of effects to use;\n"
"                      supported effects: fadein, fadeout\n"
"      --type=TYPE     TYPE can be: bootup, reboot, shutdown, suspend, resume\n"
);
}

int fbsplashd_main(int argc, char **argv)
{
	unsigned int c, i;
	int err = 0;
	int arg_vc = -1;

	arg_vc = -1;

	fbsplash_lib_init(fbspl_undef);
	fbsplashr_init(false);

	config.reqmode = FBSPL_MODE_SILENT;

	while ((c = getopt_long(argc, argv, "t:p:hvq", options, NULL)) != EOF) {

		switch (c) {
		case 'h':
			usage();
			return 0;

		case 0x100:
		case 't':
			fbsplash_acc_theme_set(optarg);
			break;

		case 'p':
		case 0x101:
			config.progress = atoi(optarg);
			break;

		case 0x102:
			config.kdmode = KD_GRAPHICS;
			break;
#ifdef CONFIG_TTF
		case 0x103:
			fbsplash_acc_message_set(optarg);
			break;
#endif
		case 0x104:
			arg_pidfile = strdup(optarg);
			break;

		case 0x105:
			config.minstances = true;
			break;

		case 0x106:
		{
			char *topt;

			while ((topt = strsep(&optarg, ",")) != NULL) {
				if (!strcmp(topt, "fadein"))
					config.effects |= FBSPL_EFF_FADEIN;
				else if (!strcmp(topt, "fadeout"))
					config.effects |= FBSPL_EFF_FADEOUT;
			}
			break;
		}

		case 0x107:
			if (!strcmp(optarg, "reboot"))
				config.type = fbspl_reboot;
			else if (!strcmp(optarg, "shutdown"))
				config.type = fbspl_shutdown;
			else if (!strcmp(optarg, "suspend"))
				config.type = fbspl_suspend;
			else if (!strcmp(optarg, "resume"))
				config.type = fbspl_resume;
			else
				config.type = fbspl_bootup;
			break;

		/* Verbosity level adjustment. */
		case 'q':
			config.verbosity = FBSPL_VERB_QUIET;
			break;

		case 'v':
			config.verbosity = FBSPL_VERB_HIGH;
			break;
		}
	}

	if (fbsplash_is_silent())
		config.effects &= ~FBSPL_EFF_FADEIN;

	theme = fbsplashr_theme_load();
	if (!theme) {
		iprint(MSG_ERROR, "Failed to load theme '%s'.\n", config.theme);
		exit(1);
	}

	daemon_start();
}

#ifndef UNIFIED_BUILD
int main(int argc, char **argv)
{
	return fbsplashd_main(argc, argv);
}
#endif
