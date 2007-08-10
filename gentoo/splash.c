/*
 * splash.c - Splash plugin for the Gentoo RC system.
 *
 * Copyright (c) 2007, Michal Januszewski <spock@gentoo.org>
 *
 * Original splash plugin compatible with baselayout-1's splash-functions.sh
 * written by Roy Marples <uberlord@gentoo.org>.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <linux/fb.h>
#include <einfo.h>
#include <rc.h>
#include <fbsplash.h>

#define SPLASH_CMD "export SPLASH_XRES='%d'; export SPLASH_YRES='%d';" \
				   "export SOFTLEVEL='%s'; export BOOTLEVEL='%s';" \
				   "export DEFAULTLEVEL='%s'; export svcdir=${RC_SVCDIR};" \
				   ". /sbin/splash-functions.sh; %s %s %s"

static char		*bootlevel = NULL;
static char		*defaultlevel = NULL;
static char		**svcs = NULL;
static char		**svcs_done = NULL;
static int		svcs_cnt = 0;
static int		svcs_done_cnt = 0;
static pid_t	pid_daemon = 0;
static spl_cfg_t	*config = NULL;

static int		xres = 0;
static int		yres = 0;

/*
 * Check whether a strlist contains a specific item.
 */
static bool list_has(char **list, const char *item)
{
	for (; list && *list; list++) {
		if (strcmp(*list, item) == 0)
			return true;
	}
	return false;
}

/*
 * Merge two strlists keeping them sorted.
 */
static char** strlist_merge_sort(char **dest, char **src)
{
	int i;

	for (i = 0; src && src[i]; i++) {
		dest = rc_strlist_addsort(dest, src[i]);
	}
	return dest;
}

/*
 * Count the number of items in a strlist.
 */
static int strlist_count(char **list)
{
	int c;

	for (c = 0; list && *list; list++)
		c++;

	return c;
}

/*
 * Create a strlist from a file pointer. Can be used
 * to get a list of words printed by an app/script.
 */
static char **get_list_fp(char **list, FILE *fp)
{
	char buffer[512];
	char *p;
	char *token;

	while (fgets(buffer, 512, fp)) {
		p = buffer;

		/* Remove the newline character */
		if (p[strlen(p)-1] == '\n')
			p[strlen(p)-1] = 0;

		/* Strip leading spaces/tabs */
		while ((*p == ' ') || (*p == '\t'))
			p++;

		/* Get entry - we do not want comments */
		token = strsep(&p, "#");
		if (!(token && (strlen(token) > 1)))
			continue;

		while ((p = strsep(&token, " ")) != NULL) {
			if (strlen(p) > 1) {
				list = rc_strlist_add(list, p);
			}
		}
	}

	return list;
}

/*
 * Create a strlist from a file. Used for svcs_start/svcs_stop.
 */
static char **get_list(char **list, const char *file)
{
	FILE *fp;

	if (!(fp = fopen(file, "r"))) {
		ewarn("%s: `%s': %s", __func__, file, strerror(errno));
		return list;
	}

	list = get_list_fp(list, fp);
	fclose(fp);

	return list;
}

/*
 * Get splash settings from /etc/conf.d/splash
 */
static int splash_config_gentoo(spl_cfg_t *cfg, spl_type_t type)
{
	char **confd;
	char *t;

	confd = rc_get_config(NULL, "/etc/conf.d/splash");

	t = rc_get_config_entry(confd, "SPLASH_KDMODE");
	if (t) {
		if (!strcasecmp(t, "graphics")) {
			cfg->kdmode = KD_GRAPHICS;
		} else if (!strcasecmp(t, "text")) {
			cfg->kdmode = KD_TEXT;
		}
	}

	t = rc_get_config_entry(confd, "SPLASH_PROFILE");
	if (t) {
		if (!strcasecmp(t, "on") || !strcasecmp(t, "yes"))
			cfg->profile = true;
	}

	t = rc_get_config_entry(confd, "SPLASH_TTY");
	if (t) {
		int i;
		if (sscanf(t, "%d", &i) == 1 && i > 0) {
			cfg->tty_s = i;
		}
	}

	t = rc_get_config_entry(confd, "SPLASH_THEME");
	if (t)
		splash_acc_theme_set(t);

	t = rc_get_config_entry(confd, "SPLASH_MODE_REQ");
	if (t) {
		if (!strcasecmp(t, "verbose")) {
			cfg->reqmode = 'v';
		} else if (!strcasecmp(t, "silent")) {
			cfg->reqmode = 's';
		} else if (!strcasecmp(t, "silentonly")) {
			cfg->reqmode = 't';
		}
	}

	t = rc_get_config_entry(confd, "SPLASH_VERBOSE_ON_ERRORS");
	if (t && (!strcasecmp(t, "on") || !strcasecmp(t, "yes")))
		cfg->vonerr = true;

	switch(type) {
	case spl_reboot:
		t = rc_get_config_entry(confd, "SPLASH_REBOOT_MESSAGE");
		if (t)
			splash_acc_message_set(t);
		break;

	case spl_shutdown:
		t = rc_get_config_entry(confd, "SPLASH_SHUTDOWN_MESSAGE");
		if (t)
			splash_acc_message_set(t);
		break;

	case spl_bootup:
	default:
		t = rc_get_config_entry(confd, "SPLASH_BOOT_MESSAGE");
		if (t)
			splash_acc_message_set(t);
		break;
	}

	t = rc_get_config_entry(confd, "SPLASH_EFFECTS");
	if (t) {
		char *opt;

		while ((opt = strsep(&t, ",")) != NULL) {
			if (!strcmp(opt, "fadein")) {
				cfg->effects |= SPL_EFF_FADEIN;
			} else if (!strcmp(opt, "fadeout")) {
				cfg->effects |= SPL_EFF_FADEOUT;
			}
		}
	}

	rc_strlist_free(confd);
	return 0;
}

/*
 * Call a function from /sbin/splash-functions.sh.
 * This is rather slow, so use it only when really necessary.
 */
static int splash_call(const char *cmd, const char *arg1, const char *arg2)
{
	char *c;
	int l;
	char *soft = getenv("RC_SOFTLEVEL");

	if (!cmd || !soft)
		return -1;

	l = strlen(SPLASH_CMD) + strlen(soft) + strlen(cmd) + 10;
	if (arg1)
		l += strlen(arg1);
	if (arg2)
		l += strlen(arg2);

	c = malloc(sizeof(char*) * l);
	if (!c)
		return -1;

	snprintf(c, l, SPLASH_CMD, xres, yres,
			arg1 ? (strcmp(arg1, RC_LEVEL_SYSINIT) == 0 ? bootlevel : soft) : soft,
			bootlevel, defaultlevel, cmd, arg1 ? arg1 : "", arg2 ? arg2 : "");
	l = system(c);
	free(c);
	return l;
}

/*
 * Run a theme hook script.
 */
static int splash_theme_hook(const char *name, const char *type, const char *arg1)
{
	char *buf;
	int l = 256;

	if (arg1)
		splash_profile("%s %s %s\n", type, name, arg1);
	else
		splash_profile("%s %s\n", type, name);

	l += strlen(name);
	l += strlen(config->theme);

	buf = malloc(l * sizeof(char*));
	snprintf(buf, l, "/etc/splash/%s/scripts/%s-%s", config->theme, name, type);

	if (!rc_is_exec(buf)) {
		free(buf);
		return 0;
	}

	l = splash_call(buf, arg1, NULL);
	free(buf);
	return l;
}

/*
 * Update service state.
 */
static int splash_svc_state(const char *name, const char *state, bool paint)
{
	if (paint)
		splash_theme_hook(state, "pre", name);

	splash_send("update_svc %s %s\n", name, state);

	if (paint) {
		splash_send("paint\n");
		splash_theme_hook(state, "post", name);
	}

	return 0;
}

/*
 * Get the resolution that the silent splash will use.
 */
static void splash_init_res()
{
	struct fb_var_screeninfo var;
	int fh;

	if ((fh = open("/dev/fb0", O_RDONLY)) == -1)
		if ((fh = open("/dev/fb/0", O_RDONLY)) == -1)
			return;

	if (ioctl(fh, FBIOGET_VSCREENINFO, &var))
		return;

	close(fh);

	splash_get_res(config->theme, (int*)&var.xres, (int*)&var.yres);
	xres = var.xres;
	yres = var.yres;
}

/*
 * Init splash config variables and check that the splash daemon
 * is running.
 */
static int splash_init(bool start)
{
	char **tmp;

	config->verbosity = VERB_QUIET;
	if (splash_check_daemon(&pid_daemon)) {
		config->verbosity = VERB_NORMAL;
		return -1;
	}

	config->verbosity = VERB_NORMAL;

	if (svcs)
		ewarn("%s: We already have a svcs list!", __func__);

	/* Booting.. */
	if (start) {
		svcs = get_list(NULL, SPLASH_CACHEDIR"/svcs_start");
		svcs_cnt = strlist_count(svcs);

		svcs_done = rc_services_in_state(rc_service_started);

		tmp = rc_services_in_state(rc_service_inactive);
		svcs_done = strlist_merge_sort(svcs_done, tmp);
		rc_strlist_free(tmp);

		tmp = rc_services_in_state(rc_service_failed);
		svcs_done = strlist_merge_sort(svcs_done, tmp);
		rc_strlist_free(tmp);

		tmp = rc_services_in_state(rc_service_scheduled);
		svcs_done = strlist_merge_sort(svcs_done, tmp);
		rc_strlist_free(tmp);

		svcs_done_cnt = strlist_count(svcs_done);
	/* .. or rebooting? */
	} else {
		svcs = get_list(NULL, SPLASH_CACHEDIR"/svcs_stop");
		svcs_cnt = strlist_count(svcs);

		svcs_done = rc_services_in_state(rc_service_started);

		tmp = rc_services_in_state(rc_service_starting);
		svcs_done = strlist_merge_sort(svcs_done, tmp);
		rc_strlist_free(tmp);

		tmp = rc_services_in_state(rc_service_inactive);
		svcs_done = strlist_merge_sort(svcs_done, tmp);
		rc_strlist_free(tmp);

		svcs_done_cnt = svcs_cnt - strlist_count(svcs_done);
	}

	splash_init_res();

	return 0;
}

/*
 * Handle the start/stop of a single service.
 */
static int splash_svc_handle(const char *name, const char *state, bool skip)
{
	if (!skip) {
		/* If we don't have any services, something must be broken.
		 * Bail out since there is nothing we can do about it. */
		if (svcs_cnt == 0)
			return -1;

		/* Don't process services twice. */
		if (list_has(svcs_done, name))
			return 0;

		rc_strlist_add(svcs_done, name);
		svcs_done_cnt++;
	}

	/* Recalculate progress */
	config->progress = svcs_done_cnt * SPL_PROGRESS_MAX / svcs_cnt;

	splash_theme_hook(state, "pre", name);
	splash_svc_state(name, state, 0);
	splash_send("progress %d\n", config->progress);
	splash_send("paint\n");
	splash_theme_hook(state, "post", name);

	return 0;
}

/*
 * Create a list of services that will be started during bootup.
 */
int splash_svcs_start()
{
	rc_depinfo_t *deptree;
	FILE *fp;
	char **t, **deporder, *s, *r;
	int i, j, err = 0;

	fp = fopen(SPLASH_CACHEDIR"/svcs_start", "w");
	if (!fp) {
		ewarn("%s: `%s': %s", __func__, SPLASH_CACHEDIR"/svcs_start", strerror(errno));
		return -1;
	}

	if ((deptree = rc_load_deptree()) == NULL) {
		eerror("%s: failed to load deptree", __func__);
		err = -2;
		goto out;
	}

	deporder = rc_order_services(deptree, bootlevel, RC_DEP_START);

	/* Save what we've got so far to the svcs_start. */
	i = 0;
	if (deporder && deporder[0]) {
		while ((s = deporder[i++])) {
			if (i > 1)
				fprintf(fp, " ");
			fprintf(fp, "%s", s);
		}
	}

	t = deporder;
	deporder = rc_order_services(deptree, defaultlevel, RC_DEP_START);

	/* Print the new services and skip ones that have already been started
	 * in the 'boot' runlevel. */
	i = 0;
	if (deporder && deporder[0]) {
		while ((s = deporder[i])) {
			j = 0;
			while ((r = t[j++])) {
				if (!strcmp(deporder[i], r))
					 goto next;
			}
			fprintf(fp, " %s", s);
next:		i++;
		}
	}

	rc_strlist_free(deporder);
	rc_strlist_free(t);
	rc_free_deptree(deptree);

out:
	fclose(fp);
	return 0;
}

/*
 * Create a list of services that will be stopped during reboot/shutdown.
 */
int splash_svcs_stop(const char *runlevel)
{
	rc_depinfo_t *deptree;
	char **deporder, *s;
	FILE *fp;
	int i, err = 0;

	fp = fopen(SPLASH_CACHEDIR"/svcs_stop", "w");
	if (!fp) {
		ewarn("%s: `%s': %s", __func__, SPLASH_CACHEDIR"/svcs_stop", strerror(errno));
		return -1;
	}

	if ((deptree = rc_load_deptree()) == NULL) {
		eerror("%s: failed to load deptree", __func__);
		err = -2;
		goto out;
	}

	deporder = rc_order_services(deptree, runlevel, RC_DEP_STOP);

	i = 0;
	if (deporder && deporder[0]) {
		while ((s = deporder[i++])) {
			if (i > 1)
				fprintf(fp, " ");
			fprintf(fp, "%s", s);
		}
	}

	rc_strlist_free(deporder);
	rc_free_deptree(deptree);
out:
	fclose(fp);
	return err;
}

/*
 * Start the splash daemon during boot/reboot.
 */
static int splash_start(const char *runlevel)
{
	bool start;
	int i, err = 0;
	char buf[2048];

	/* Get a list of services that we'll have to handle. */
	/* We're rebooting/shutting down. */
	if (!strcmp(runlevel, RC_LEVEL_SHUTDOWN) || !strcmp(runlevel, RC_LEVEL_REBOOT)) {
		if ((err = splash_cache_prep()))
			return err;
		splash_svcs_stop(runlevel);
		start = false;
	/* We're booting. */
	} else {
		if ((err = splash_cache_prep()))
			return err;
		splash_svcs_start();
		start = true;
	}
	splash_init_res();
	splash_theme_hook("rc_init", "pre", runlevel);

	/* Perform sanity checks (console=, CONSOLE= etc). */
	if (splash_check_sanity())
		return -1;

	/* Start the splash daemon */
	snprintf(buf, 2048, "BOOT_MSG='%s' " SPLASH_DAEMON " --theme=\"%s\" --pidfile=" SPLASH_PIDFILE " --type=%s %s %s",
			 config->message, config->theme,
			 (config->type == spl_reboot) ? "reboot" : ((config->type == spl_shutdown) ? "shutdown" : "bootup"),
			 (config->kdmode == KD_GRAPHICS) ? "--kdgraphics" : "",
			 (config->effects & (SPL_EFF_FADEOUT | SPL_EFF_FADEIN)) ? "--effects=fadeout,fadein" :
				 ((config->effects & SPL_EFF_FADEOUT) ? "--effects=fadeout" :
					 ((config->effects & SPL_EFF_FADEIN) ? "--effects=fadein" : "")));

	err = system(buf);
	if (err == -1 || WEXITSTATUS(err) != 0) {
		eerror("Failed to start the splash daemon, error code %d", err);
		return err;
	}

	err = splash_init(start);
	if (err)
		return err;

	/* Set the initial state of all services. */
	for (i = 0; svcs && svcs[i]; i++) {
		splash_svc_state(svcs[i], start ? "svc_inactive_start" : "svc_inactive_stop", 0);
	}

	splash_set_evdev();
	splash_send("set tty silent %d\n", config->tty_s);
	splash_send("set mode silent\n");
	splash_send("repaint\n");
	return err;
}

/*
 * Stop the splash daemon.
 */
static int splash_stop(const char *runlevel)
{
	char *save[] = { "profile", "svcs_start", NULL };
	char buf[128];
	int cnt = 0;

	splash_send("exit\n");
	snprintf(buf, 128, "/proc/%d", pid_daemon);

	/* Wait up to 1.0s for the splash daemon to exit. */
	while (rc_is_dir(buf) && cnt < 100) {
		usleep(10000);
		cnt++;
	}

	/* Just to be sure we aren't stuck in a black ex-silent tty.. */
	if (splash_is_silent())
		splash_set_verbose(0);

	/* If we don't get a runlevel argument, then we're being executed
	 * because of a rc-abort event and we don't save any data. */
	if (runlevel == NULL) {
		return splash_cache_cleanup(NULL);
	} else {
		return splash_cache_cleanup(save);
	}
}

int _splash_hook (rc_hook_t hook, const char *name)
{
	int i = 0;
	spl_type_t type = spl_bootup;
	char *runlev;
	bool skip = false;

	runlev = rc_get_runlevel();
	if (!strcmp(runlev, RC_LEVEL_REBOOT))
		type = spl_reboot;
	else if (!strcmp(runlev, RC_LEVEL_SHUTDOWN))
		type = spl_shutdown;

	/* We generally do nothing if we're in sysinit. Except if the
	 * autoconfig service is present, when we get a list of services
	 * that will be started by it and mark them as coldplugged. */
	if (name && !strcmp(name, RC_LEVEL_SYSINIT)) {
		if (hook == rc_hook_runlevel_start_out) {
			FILE *fp;
			char **list = NULL;
			int i;

			fp = popen("if [ -e /etc/init.d/autoconfig ]; then . /etc/init.d/autoconfig ; list_services ; fi", "r");
			if (!fp)
				return 0;

			list = get_list_fp(NULL, fp);
			for (i = 0; list && list[i]; i++) {
				rc_mark_service(list[i], rc_service_coldplugged);
			}
			pclose(fp);
		}
		return 0;
	}

	/* Get boot and default levels from env variables exported by RC.
	 * If unavailable, use the default ones. */
	bootlevel = getenv("RC_BOOTLEVEL");
	if (!bootlevel)
		bootlevel = RC_LEVEL_BOOT;

	defaultlevel = getenv("RC_DEFAULTLEVEL");
	if (!defaultlevel)
		defaultlevel = RC_LEVEL_DEFAULT;

	/* Don't do anything if we're starting/stopping a service, but
	 * we aren't in the middle of a runlevel switch. */
	if (!(rc_runlevel_starting() || rc_runlevel_stopping())) {
		if (hook != rc_hook_runlevel_stop_in &&
			hook != rc_hook_runlevel_stop_out &&
			hook != rc_hook_runlevel_start_in &&
			hook != rc_hook_runlevel_start_out)
			return 0;
	} else {
		/* We're starting/stopping a runlevel. Check whether we're
		 * actually booting/rebooting. */
		if (rc_runlevel_starting() && strcmp(runlev, bootlevel) &&
			strcmp(runlev, defaultlevel) && strcmp(runlev, RC_LEVEL_SYSINIT))
			return 0;

		if (rc_runlevel_stopping() && strcmp(runlev, bootlevel) &&
			strcmp(runlev, RC_LEVEL_REBOOT) && strcmp(runlev, RC_LEVEL_SHUTDOWN))
			return 0;
	}

	if (!config) {
		config = splash_lib_init(type);
		splash_config_gentoo(config, type);
		splash_parse_kcmdline(false);
	}

	/* Extremely weird.. should never happen. */
	if (!config)
		return -1;

	/* Don't do anything if we're not running in silent mode. */
	if (config->reqmode != 's')
		return 0;

	switch (hook) {
	case rc_hook_runlevel_stop_in:
		/* Start the splash daemon on reboot. The theme hook is called
		 * from splash_start(). */
		if (strcmp(name, RC_LEVEL_REBOOT) == 0 || strcmp(name, RC_LEVEL_SHUTDOWN) == 0) {
			if ((i = splash_start(name))) {
				splash_set_verbose(0);
				return i;
			} else {
				if (rc_service_state("gpm", rc_service_started)) {
					splash_send("set gpm\n");
					splash_send("repaint\n");
				}
			}
			splash_theme_hook("rc_init", "post", name);
			return i;
		} else {
			splash_theme_hook("rc_exit", "pre", name);
			splash_theme_hook("rc_exit", "post", name);
			splash_lib_cleanup();
			config = NULL;
		}
		break;

	case rc_hook_runlevel_stop_out:
		/* Make sure the progress indicator reaches 100%, even if
		 * something went wrong along the way. */
		if (strcmp(name, RC_LEVEL_REBOOT) == 0 || strcmp(name, RC_LEVEL_SHUTDOWN) == 0) {
			config->verbosity = VERB_QUIET;
			i = splash_check_daemon(&pid_daemon);
			config->verbosity = VERB_NORMAL;
			if (i)
				return -1;

			splash_send("progress %d\n", SPL_PROGRESS_MAX);
			splash_send("paint\n");
			splash_cache_cleanup(NULL);
		}
		break;

	case rc_hook_runlevel_start_in:
		/* Start the splash daemon during boot right after we finish
		 * sysinit and are entering the boot runlevel. Due to historical
		 * reasons, we simulate a full sysinit cycle here for the theme
		 * scripts. */
		if (strcmp(name, bootlevel) == 0) {
			if ((i = splash_start(RC_LEVEL_SYSINIT)))
				splash_set_verbose(0);
			splash_theme_hook("rc_init", "post", RC_LEVEL_SYSINIT);
			splash_theme_hook("rc_exit", "pre", RC_LEVEL_SYSINIT);
			splash_theme_hook("rc_exit", "post", RC_LEVEL_SYSINIT);
		}
		splash_theme_hook("rc_init", "pre", name);
		splash_theme_hook("rc_init", "post", name);
		break;

	case rc_hook_runlevel_start_out:
		/* Stop the splash daemon after boot-up is finished. */
		if (strcmp(name, bootlevel)) {
			config->verbosity = VERB_QUIET;
			i = splash_check_daemon(&pid_daemon);
			config->verbosity = VERB_NORMAL;
			if (i)
				return -1;

			/* Make sure the progress indicator reaches 100%, even if
			 * something went wrong along the way. */
			splash_send("progress %d\n", SPL_PROGRESS_MAX);
			splash_send("paint\n");
			splash_theme_hook("rc_exit", "pre", name);
			i = splash_stop(name);
			splash_theme_hook("rc_exit", "post", name);
			splash_lib_cleanup();
			config = NULL;
		}
		break;

	case rc_hook_service_start_now:
do_start:
		/* If we've been inactive, do nothing since the service has
		 * already been handled before it went inactive. */
		if (rc_service_state(name, rc_service_wasinactive))
			return 0;

		/* If we're starting or stopping a service, we're being called by
		 * runscript and thus have to reload our config. */
		if (splash_init(true))
			return -1;
		i = splash_svc_handle(name, "svc_start", skip);
		break;

	case rc_hook_service_start_out:
		/* If a service gets scheduled, we want to increment the progress
		 * bar (as it is no longer blocking boot completion). However,
		 * the service may actually start during boot (some time after
		 * being scheduled), so we don't want to increment the progress
		 * bar twice. The following if clause satisfies this by catching
		 * the first case but not the second. */
		if (rc_service_state(name, rc_service_scheduled) &&
		    !rc_service_state(name, rc_service_starting)) {
			skip = true;
			goto do_start;
		}
		break;

	case rc_hook_service_start_done:
		config->verbosity = VERB_QUIET;
		i = splash_check_daemon(&pid_daemon);
		config->verbosity = VERB_NORMAL;
		if (i)
			return -1;

		if (!rc_service_state(name, rc_service_failed) &&
		    !rc_service_state(name, rc_service_stopped)) {
			bool gpm = false;

			if (!strcmp(name, "gpm")) {
				int cnt = 0;
				gpm = true;
				/* Wait up to 0.25s for the GPM socket to appear. */
				while (rc_exists("/dev/gpmctl") && cnt < 25) {
					usleep(10000);
					cnt++;
				}
				splash_send("set gpm\n");
			}

			i = splash_svc_state(name, "svc_started", 1);

			if (gpm) {
				splash_send("repaint\n");
			}
		} else {
			i = splash_svc_state(name, "svc_start_failed", 1);
			if (config->vonerr) {
				splash_set_verbose(0);
			}
		}
		splash_lib_cleanup();
		config = NULL;
		break;

	case rc_hook_service_stop_now:
		if (splash_init(false))
			return -1;

		/* We need to stop localmount from unmounting our cache dir.
		   Luckily plugins can add to the unmount list. */
		if (name && !strcmp(name, "localmount")) {
			char *umounts = getenv("RC_NO_UMOUNTS");

            if (umounts)
                fprintf(rc_environ_fd, "RC_NO_UMOUNTS=%s:%s", umounts, SPLASH_CACHEDIR);
            else
                fprintf(rc_environ_fd, "RC_NO_UMOUNTS=%s", SPLASH_CACHEDIR);
		}
		i = splash_svc_handle(name, "svc_stop", false);
		break;

	case rc_hook_service_stop_done:
		config->verbosity = VERB_QUIET;
		i = splash_check_daemon(&pid_daemon);
		config->verbosity = VERB_NORMAL;
		if (i)
			return -1;

		if (rc_service_state(name, rc_service_stopped)) {
			i = splash_svc_state(name, "svc_stopped", 1);
		} else {
			i = splash_svc_state(name, "svc_stop_failed", 1);
			if (config->vonerr) {
				splash_set_verbose(0);
			}
		}
		splash_lib_cleanup();
		config = NULL;
		break;

	case rc_hook_abort:
		i = splash_stop(name);
		splash_lib_cleanup();
		config = NULL;
		break;

	default:
		break;
	}

	if (svcs) {
		rc_strlist_free(svcs);
		svcs = NULL;
	}

	return i;
}
