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
#include <linux/kd.h>
#include <einfo.h>
#include <rc.h>
#include <splash.h>

#define SPLASH_CMD "export SOFTLEVEL='%s'; export BOOTLEVEL="RC_LEVEL_BOOT";" \
				   "export DEFAULTLEVEL="RC_LEVEL_DEFAULT"; export svcdir=${RC_SVCDIR};" \
				   ". /sbin/splash-functions.sh; %s %s %s"

static char		**svcs = NULL;
static int		svcs_cnt = 0;
static int		svcs_done_cnt = 0;
static pid_t	pid_daemon = 0;
static scfg_t	*config = NULL;

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
 * Create a strlist from a file. Used for svcs_start/svcs_stop.
 */
static char **get_list(char **list, const char *file)
{
	FILE *fp;
	char buffer[512];
	char *p;
	char *token;

	if (!(fp = fopen(file, "r"))) {
		ewarn("%s: `%s': %s", __func__, file, strerror(errno));
		return list;
	}

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
	fclose(fp);

	return list;
}

/*
 * Get splash settings from /etc/conf.d/splash
 */
static int splash_config_gentoo(scfg_t *cfg, stype_t type)
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
	if (t) {
		if (cfg->theme)
			free(cfg->theme);
		cfg->theme = strdup(t);
	}

	t = rc_get_config_entry(confd, "SPLASH_MODE_REQ");
	if (t) {
		if (!strcasecmp(t, "verbose")) {
			cfg->reqmode = 'v';
		} else if (!strcasecmp(t, "silent")) {
			cfg->reqmode = 's';
		}
	}

	t = rc_get_config_entry(confd, "SPLASH_VERBOSE_ON_ERRORS");
	if (t) {
		if (!strcasecmp(t, "on") || !strcasecmp(t, "yes")) {
			cfg->vonerr = true;
		}
	}

	switch(type) {
	case reboot:
		t = rc_get_config_entry(confd, "SPLASH_REBOOT_MESSAGE");
		if (t) {
			if (cfg->message)
				free(cfg->message);
			cfg->message = strdup(t);
		}
		break;

	case shutdown:
		t = rc_get_config_entry(confd, "SPLASH_SHUTDOWN_MESSAGE");
		if (t) {
			if (cfg->message)
				free(cfg->message);
			cfg->message = strdup(t);
		}
		break;

	case bootup:
	default:
		t = rc_get_config_entry(confd, "SPLASH_BOOT_MESSAGE");
		if (t) {
			if (cfg->message)
				free(cfg->message);
			cfg->message = strdup(t);
		}
		break;
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

	l = strlen(SPLASH_CMD) + strlen(soft) + strlen(cmd);
	if (arg1)
		l += strlen(arg1);
	if (arg2)
		l += strlen(arg2);

	c = malloc(sizeof(char*) * l);
	if (!c)
		return -1;

	snprintf(c, l, SPLASH_CMD,
			arg1 ? (strcmp(arg1, RC_LEVEL_SYSINIT) == 0 ? RC_LEVEL_BOOT : soft) : soft,
			cmd, arg1 ? arg1 : "", arg2 ? arg2 : "");
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
	snprintf(buf, 1024, "/etc/splash/%s/scripts/%s-%s", config->theme, name, type);

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
 * Init splash config variables and check that the splash daemon
 * is running.
 */
static int splash_init(bool start)
{
	char **tmp;

	if (svcs)
		ewarn("%s: We already have a svcs list!", __func__);

	/* Booting.. */
	if (start) {
		svcs = get_list(NULL, SPLASH_CACHEDIR"/svcs_start");
		svcs_cnt = strlist_count(svcs);

		tmp = rc_services_in_state(rc_service_started);
		svcs_done_cnt = strlist_count(tmp);
		rc_strlist_free(tmp);

		tmp = rc_services_in_state(rc_service_inactive);
		svcs_done_cnt += strlist_count(tmp);
		rc_strlist_free(tmp);

		tmp = rc_services_in_state(rc_service_failed);
		svcs_done_cnt +=  strlist_count(tmp);
		rc_strlist_free(tmp);
	/* .. or rebooting? */
	} else {
		svcs = get_list(NULL, SPLASH_CACHEDIR"/svcs_stop");
		svcs_cnt = strlist_count(svcs);

		tmp = rc_services_in_state(rc_service_started);
		svcs_done_cnt = strlist_count(tmp);
		rc_strlist_free(tmp);

		tmp = rc_services_in_state(rc_service_starting);
		svcs_done_cnt += strlist_count(tmp);
		rc_strlist_free(tmp);

		tmp = rc_services_in_state(rc_service_inactive);
		svcs_done_cnt += strlist_count(tmp);
		rc_strlist_free(tmp);

		svcs_done_cnt = svcs_cnt - svcs_done_cnt;
	}

	if (splash_check_daemon(&pid_daemon))
		return -1;

	return 0;
}

/*
 * Handle the start/stop of a single service.
 */
static int splash_svc_handle(const char *name, const char *state)
{
	/* If we don't have any services, something must be broken.
	 * Bail out since there is nothing we can do about it. */
	if (svcs_cnt == 0)
		return -1;

	/* Recalculate progress */
	svcs_done_cnt++;
	config->progress = svcs_done_cnt * PROGRESS_MAX / svcs_cnt;

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
	char *bootlevel = NULL, *deflevel = NULL;
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

	/* Get boot and default levels from env variables exported by RC.
	 * If unavailable, use the default ones. */
	s = getenv("RC_BOOTLEVEL");
	if (s)
		bootlevel = strdup(s);
	else
		bootlevel = strdup(RC_LEVEL_BOOT);

	s = getenv("RC_DEFAULTLEVEL");
	if (s)
		deflevel = strdup(s);
	else
		deflevel = strdup(RC_LEVEL_DEFAULT);

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
	deporder = rc_order_services(deptree, deflevel, RC_DEP_START);

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

	free(deflevel);
	free(bootlevel);
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
	splash_theme_hook("rc_init", "pre", runlevel);

	/* Perform sanity checks (console=, CONSOLE= etc). */
	if (!splash_check_sanity())
		return -1;

	/* Start the splash daemon */
	snprintf(buf, 2048, "BOOT_MSG='%s' " SPLASH_EXEC " -d --theme=\"%s\" --pidfile=" SPLASH_PIDFILE " %s",
			 config->message, config->theme, (config->kdmode == KD_GRAPHICS) ? "--kdgraphics" : "");

	err = system(buf);
	if (err == -1 || WEXITSTATUS(err) != 0) {
		eerror("Failed to splash the splash daemon, error code %d", err);
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
	char buf[128];
	int cnt = 0;

	splash_send("set mode verbose\n");
	splash_send("exit\n");
	snprintf(buf, 128, "/proc/%d", pid_daemon);

	/* Wait up to 0.5s for the splash daemon to exit. */
	while (rc_is_dir(buf) && cnt < 50) {
		usleep(10000);
		cnt++;
	}

	/* Just to be sure we aren't stuck in a black ex-silent tty.. */
	splash_set_verbose();

	return splash_cache_cleanup();
}

int _splash_hook (rc_hook_t hook, const char *name)
{
	int i = 0;
	stype_t type = bootup;
	char *t;

	t = rc_get_runlevel();
	if (!strcmp(t, RC_LEVEL_REBOOT))
		type = reboot;
	else if (!strcmp(t, RC_LEVEL_SHUTDOWN))
		type = shutdown;

	/* Do nothing if we are in sysinit */
	if (!strcmp(name, RC_LEVEL_SYSINIT))
		return 0;

	if (!config) {
		config = splash_lib_init(type);
		splash_config_gentoo(config, type);
		splash_parse_kcmdline(config, false);
	}

	/* Extremely weird.. should never happen. */
	if (!config)
		return -1;

	/* Don't do anything if we're not running in silent mode. */
	if (config->reqmode != 's')
		return 0;

	/* Don't do anything if we're starting/stopping a service, but
	 * we aren't in the middle of a runlevel switch. */
	if (!(rc_runlevel_starting() || rc_runlevel_stopping()) &&
		hook != rc_hook_runlevel_stop_in &&
		hook != rc_hook_runlevel_stop_out &&
		hook != rc_hook_runlevel_start_in &&
		hook != rc_hook_runlevel_stop_out)
		return 0;

	switch (hook) {
	case rc_hook_runlevel_stop_in:
		/* Start the splash daemon on reboot. The theme hook is called
		 * from splash_start(). */
		if (strcmp(name, RC_LEVEL_REBOOT) == 0 || strcmp(name, RC_LEVEL_SHUTDOWN) == 0) {
			if ((i = splash_start(name))) {
				splash_set_verbose();
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
		}
		break;

	case rc_hook_runlevel_stop_out:
		/* Make sure the progress indicator reaches 100%, even if
		 * something went wrong along the way. */
		if (strcmp(name, RC_LEVEL_REBOOT) == 0 || strcmp(name, RC_LEVEL_SHUTDOWN) == 0) {
			splash_send("progress %d\n", PROGRESS_MAX);
			splash_send("paint\n");
		}
		break;

	case rc_hook_runlevel_start_in:
		/* Start the splash daemon during boot right after we finish
		 * sysinit and are entering the boot runlevel. Due to historical
		 * reasons, we simulate a full sysinit cycle here for the theme
		 * scripts. */
		if (strcmp(name, RC_LEVEL_BOOT) == 0) {
			if ((i = splash_start(RC_LEVEL_SYSINIT)))
				splash_set_verbose();
			splash_theme_hook("rc_init", "post", RC_LEVEL_SYSINIT);
			splash_theme_hook("rc_exit", "pre", RC_LEVEL_SYSINIT);
			splash_theme_hook("rc_exit", "post", RC_LEVEL_SYSINIT);
		}
		splash_theme_hook("rc_init", "pre", name);
		splash_theme_hook("rc_init", "post", name);
		break;

	case rc_hook_runlevel_start_out:
		/* Stop the splash daemon after boot-up is finished. */
		if (strcmp(name, RC_LEVEL_BOOT)) {
			/* Make sure the progress indicator reaches 100%, even if
			 * something went wrong along the way. */
			splash_send("progress %d\n", PROGRESS_MAX);
			splash_send("paint\n");
			splash_theme_hook("rc_exit", "pre", name);
			i = splash_stop(name);
			splash_theme_hook("rc_exit", "post", name);
		}
		break;

	case rc_hook_service_start_now:
		/* If we're starting or stopping a service, we're being called by
		 * runscript and thus have to reload our config. */
		if (splash_init(true))
			return -1;
		i = splash_svc_handle(name, "svc_start");
		break;

	case rc_hook_service_start_done:
		if (rc_service_state(name, rc_service_started) ||
		    rc_service_state(name, rc_service_inactive)) {
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
				ewarn("splash: service %s failed, switching to verbose mode", name);
				splash_set_verbose();
			}
		}
		break;

	case rc_hook_service_stop_now:
		if (splash_init(false))
			return -1;

		/* We need to stop localmount from unmounting our cache dir.
		   Luckily plugins can add to the unmount list. */
		if (name && !strcmp(name, "localmount")) {
			char *umounts = getenv("RC_NO_UMOUNTS");
			char *new;
			int i = strlen(SPLASH_CACHEDIR) + 1;

			if (umounts)
				i += strlen (umounts) + 1;

			new = malloc(sizeof(char*) * i);
			if (new) {
				if (umounts)
					snprintf(new, i, "%s:%s", umounts, SPLASH_CACHEDIR);
				else
					snprintf(new, i, "%s", SPLASH_CACHEDIR);
			}

			/* We unsetenv first as some libc's leak memory if we overwrite
			   a var with a bigger value */
			if (umounts)
				unsetenv ("RC_NO_UMOUNTS");
			setenv ("RC_NO_UMOUNTS", new, 1);
			free (new);
		}
		i = splash_svc_handle(name, "svc_stop");
		break;

	case rc_hook_service_stop_done:
		if (rc_service_state(name, rc_service_stopped)) {
			i = splash_svc_state(name, "svc_stopped", 1);
		} else {
			i = splash_svc_state(name, "svc_stop_failed", 1);
			if (config->vonerr) {
				ewarn("splash: service %s failed, switching to verbose mode", name);
				splash_set_verbose();
			}
		}
		break;

	default:
		return i;
	}

	return i;
}
