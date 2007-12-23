/*
 * daemon_cmd.c -- Code handling the daemon commands.
 *
 * Copyright (C) 2005-2007 Michal Januszewski <spock@gentoo.org>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "common.h"
#include "daemon.h"

/*
 * 'exit' command handler.
 */
int cmd_exit(void **args)
{
	item *i, *j;

	pthread_cancel(th_switchmon);
	pthread_mutex_lock(&mtx_paint);

	if (ctty == CTTY_SILENT) {
		if (config.effects & FBSPL_EFF_FADEOUT)
			fbsplashr_render_screen(theme, true, false, FBSPL_EFF_FADEOUT);

		if (!args[0] || strcmp(args[0], "staysilent")) {
			/* Switch to the verbose tty if we're in silent mode when the
			 * 'exit' command is received. */
			ioctl(fd_tty0, VT_ACTIVATE, config.tty_v);
		}
	}

	pthread_mutex_unlock(&mtx_paint);

	pthread_kill(th_sighandler, SIGINT);
	pthread_join(th_sighandler, NULL);

	fbsplashr_theme_free(theme);
	fbsplashr_cleanup();
	fbsplash_lib_cleanup();

	for (i = svcs.head; i != NULL; ) {
		j = i->next;
		free(i->p);
		free(i);
		i = j;
	}

	exit(0);

	/* We never get here */
	return 0;
}

/*
 * 'set theme' command handler.
 *
 * Switches to a new theme.
 */
int cmd_set_theme(void **args)
{
	fbsplash_acc_theme_set(args[0]);
	pthread_mutex_lock(&mtx_paint);
	reload_theme();
	pthread_mutex_unlock(&mtx_paint);

	return 0;
}

/*
 * 'set mode' command handler.
 *
 * Switches the splash to verbose or silent mode.
 */
int cmd_set_mode(void **args)
{
	int n, i = 0;
	struct itimerval itv;

	if (!strcmp(args[0], "silent")) {
		n = config.tty_s;
	} else if (!strcmp(args[0], "verbose")) {
		n = config.tty_v;
	} else {
		return -1;
	}

	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = 0;
	itv.it_value.tv_sec = 0;
	itv.it_value.tv_usec = 250000;

vtswitch:
	if (ioctl(fd_tty0, VT_ACTIVATE, n) == -1) {
		iprint(MSG_ERROR, "Switch to tty%d failed with: %d '%s'\n", n, errno, strerror(errno));
		return -1;
	}

	/*
	 * Interrupt the VT_WAITACTIVE call every 0.25s.  This makes sure we actually
	 * return from this function in a timely manner and that we can complete the
	 * switch to silent mode even when someone does a VT_ACTIVATE before our
	 * VT_WAITACTIVE.
	 *
	 * This fixes the problem of the silent mode not being activated when
	 * rebooting directly from X.
	 */
	i++;
	setitimer(ITIMER_REAL, &itv, NULL);

	if (ioctl(fd_tty0, VT_WAITACTIVE, n) == -1) {
		if (i < 9 && errno == EINTR)
			goto vtswitch;

		iprint(MSG_ERROR, "Wait for tty%d failed with: %d '%s'\n", n, errno, strerror(errno));
		return -1;
	}

	return 0;
}

/*
 * 'set gpm' command handler.
 *
 * Creates a GPM client for the silent tty.
 */
int cmd_set_gpm(void **args)
{
#ifdef CONFIG_GPM
	Gpm_Connect conn;

	conn.eventMask = 0;
	conn.defaultMask = 0;
	conn.minMod = 0;
	conn.maxMod = ~0;
	pthread_mutex_lock(&mtx_tty);
	fd_gpm = Gpm_Open(&conn, config.tty_s);
	pthread_mutex_unlock(&mtx_tty);
	return fd_gpm;
#else
	return 0;
#endif
}

/*
 * 'set effects' command handler.
 *
 * Makes it possible to dynamically enable special effects.
 */
int cmd_set_effects(void **args)
{
	int i;

	config.effects = FBSPL_EFF_NONE;

	for (i = 0; i < 4; i++) {
		if (!args[i])
			break;

		if (!strcmp(args[i], "fadein")) {
			config.effects |= FBSPL_EFF_FADEIN;
		} else if (!strcmp(args[i], "fadeout")) {
			config.effects |= FBSPL_EFF_FADEOUT;
		}
	}

	return 0;
}

/*
 * 'set tty' command handlers.
 *
 * Assigns a TTY for verbose/silent splash screen.
 */
int cmd_set_tty(void **args)
{
	/* Make sure the new tty setting is sane. */
	if (*(int*)args[1] < 0 || *(int*)args[1] > MAX_NR_CONSOLES)
		return -1;

	if (!strcmp(args[0], "silent")) {
		pthread_mutex_lock(&mtx_tty);

		/* Do nothing if the new tty is the same as the old one. */
		if (config.tty_s == *(int*)args[1]) {
			pthread_mutex_unlock(&mtx_tty);
			return 0;
		}

		pthread_cancel(th_switchmon);

		switchmon_start(UPD_SILENT, *(int*)args[1]);
		pthread_mutex_unlock(&mtx_tty);
#ifdef CONFIG_GPM
		if (fd_gpm > 0) {
			Gpm_Close();
			cmd_set_gpm(NULL);
		}
#endif
	} else if (!strcmp(args[0], "verbose")) {

		pthread_mutex_lock(&mtx_tty);

		/* Do nothing if the new tty is the same as the old one. */
		if (config.tty_v == *(int*)args[1]) {
			pthread_mutex_unlock(&mtx_tty);
			return 0;
		}

		config.tty_v = *(int*)args[1];
		pthread_mutex_unlock(&mtx_tty);
	} else {
		return -1;
	}

	return 0;
}

/*
 * 'set event dev' command handler.
 *
 * Sets a new event device to monitor for keypresses.
 */
int cmd_set_event_dev(void **args)
{
	if (evdev)
		free(evdev);

	evdev = strdup(args[0]);

	pthread_cancel(th_switchmon);

	if (fd_evdev != -1)
		close(fd_evdev);

	fd_evdev = open(evdev, O_RDONLY);

	switchmon_start(UPD_MON, config.tty_s);

	return 0;
}

/*
 * 'paint' command handler.
 *
 * Updates the picture displayed on the screen.
 */
int cmd_paint(void **args)
{
	char ret = 0;

	pthread_mutex_lock(&mtx_paint);
	if (!theme) {
		ret = -1;
		goto out;
	}

	if (ctty != CTTY_SILENT)
		goto out;

	fbsplashr_render_screen(theme, false, false, FBSPL_EFF_NONE);
out:
	pthread_mutex_unlock(&mtx_paint);
	return ret;
}

/*
 * 'repaint' command handler.
 *
 * Repaints the whole screen.
 */
int cmd_repaint(void **args)
{
	char ret = 0;

	pthread_mutex_lock(&mtx_paint);
	if (!theme) {
		ret = -1;
		goto out;
	}

	if (ctty != CTTY_SILENT)
		goto out;

	if (config.effects & FBSPL_EFF_FADEIN) {
		config.effects &= ~FBSPL_EFF_FADEIN;
		fbsplashr_render_screen(theme, true, false, FBSPL_EFF_FADEIN);
	} else {
		fbsplashr_render_screen(theme, true, false, FBSPL_EFF_NONE);
	}
out:
	pthread_mutex_unlock(&mtx_paint);

	return 0;
}

/*
 * 'progress' command handler.
 *
 * Updates the progress indicator. Note that this only updates
 * an internal variable, it doesn't update the image on the screen.
 */
int cmd_progress(void **args)
{
	fbsplashr_progress_set(theme, *(int*)args[0]);
	return 0;
}

/*
 * 'set message' command handler.
 *
 * Sets the system message. This only has any effect if the
 * splash daemon was compiled with support for truetype fonts.
 */
int cmd_set_mesg(void **args)
{
	fbsplashr_message_set(theme, args[0]);
	return 0;
}

/*
 * 'paint rect' command handler.
 *
 * Paints a rectangular part of the background buffer on thre
 * screen.
 */
int cmd_paint_rect(void **args)
{
	rect re;
	int t, ret = 0;

	pthread_mutex_lock(&mtx_paint);
	if (!theme) {
		ret = -1;
		goto out;
	}
	if (ctty != CTTY_SILENT)
		goto out;

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

	if (re.x1 >= theme->xres)
		re.x1 = theme->xres-1;

	if (re.x2 >= theme->xres)
		re.x2 = theme->xres-1;

	if (re.y1 >= theme->yres)
		re.y1 = theme->yres-1;

	if (re.y2 >= theme->yres)
		re.y2 = theme->yres-1;

	paint_rect(theme, fb_mem, theme->bgbuf, re.x1, re.y1, re.x2, re.y2);
out:

	pthread_mutex_unlock(&mtx_paint);
	return 0;
}

/*
 * 'update_svc' command handler.
 *
 * Updates status of a specific service.
 */
int cmd_update_svc(void **args)
{
	item *i;
	svc_state *ss;
	enum ESVC state;
	struct timespec ts;

	if (!parse_svc_state(args[1], &state))
		return -1;

	clock_gettime(CLOCK_MONOTONIC, &ts);

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

	if (ss->state == e_svc_start) {
		ss->ts = ts;
	} else if (ss->state == e_svc_started || ss->state == e_svc_start_failed) {
		ss->ts.tv_sec  = ts.tv_sec  - ss->ts.tv_sec;
		ss->ts.tv_nsec = ts.tv_nsec - ss->ts.tv_nsec;

		/* Check for overflow of the nanoseconds field */
		if (ss->ts.tv_nsec < 0) {
			ss->ts.tv_sec--;
			ss->ts.tv_nsec += 1000000000;
		}
	}

	pthread_mutex_lock(&mtx_paint);
	invalidate_service(theme, args[0], state);
#if WANT_MNG
	pthread_mutex_lock(&mtx_anim);
	pthread_cond_signal(&cnd_anim);
	pthread_mutex_unlock(&mtx_anim);
#endif
	pthread_mutex_unlock(&mtx_paint);

	return 0;
}

int cmd_dump_svc_timings(void **args)
{
	svc_state *ss;
	item *i;
	FILE *fp = fopen("/lib/splash/cache/svc_timings", "w");

	if (!fp)
		return -1;

	for (i = svcs.head; i != NULL; i = i->next) {
		ss = (svc_state*)i->p;
		if (ss->svc && (ss->ts.tv_sec > 0 || ss->ts.tv_nsec > 0)) {
			fprintf(fp, "%s: %d.%.6d\n", ss->svc, (int)ss->ts.tv_sec, (int)(ss->ts.tv_nsec/1000));
		}
	}

	fclose(fp);
	return 0;
}

cmdhandler known_cmds[] =
{
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

	{	.cmd = "set gpm",
		.handler = cmd_set_gpm,
		.args = 0,
		.specs = NULL,
	},

	{	.cmd = "set effects",
		.handler = cmd_set_effects,
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

	{	.cmd = "dump_svc_timings",
		.handler = cmd_dump_svc_timings,
		.args = 0,
		.specs = NULL,
	},

	{	.cmd = "exit",
		.handler = cmd_exit,
		.args = 1,
		.specs = "s",
	},
};

/*
 * FIFO communication handler.
 */
int daemon_comm(FILE *fp_fifo)
{
	char buf[PIPE_BUF];
	int i,j,k;

	while (1) {
inner:
		while (fgets(buf, PIPE_BUF, fp_fifo)) {
			char *t;
			int args_i[4];
			void *args[4];

			memset(&args, 0, sizeof(args));
			buf[PIPE_BUF-1] = 0;
			buf[strlen(buf)-1] = 0;

			for (i = 0; i < sizeof(known_cmds)/sizeof(known_cmds[0]); i++) {
				k = strlen(known_cmds[i].cmd);

				if (strncmp(buf, known_cmds[i].cmd, k))
					continue;

				for (j = 0; j < known_cmds[i].args; j++) {
					for (; buf[k] == ' '; buf[k] = 0, k++);
					if (!buf[k]) {
						args[j] = NULL;
						continue;
					}

					switch (known_cmds[i].specs[j]) {
					case 's':
						args[j] = &(buf[k]);
						for (; buf[k] != ' ' && buf[k]; k++);
						break;

					case 'd':
						args_i[j] = strtol(&(buf[k]), &t, 0);
						if (t == &(buf[k]))
							goto inner;

						args[j] = &(args_i[j]);
						k = t - buf;
						break;
					}
				}

				known_cmds[i].handler(args);
			}
		}
	}
}

