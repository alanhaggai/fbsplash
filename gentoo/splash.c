/*
   splash.c

   Splash plugin for the Gentoo RC system.

   (c) 2007, by Michal Januszewski <spock@gentoo.org>

   For any themes that use scripts, such as the live-cd theme,
   they will have to source /sbin/splash-functions.sh themselves like so

   if ! type splash >/dev/null 2>/dev/null ; then
      . /sbin/splash-functions.sh
   fi
   */

#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <einfo.h>
#include <rc.h>
#include "splash.h"

#ifndef LIBDIR
	#define LIBDIR "lib"
#endif

#define SPLASH_CACHEDIR		"/"LIBDIR"/splash/cache"
#define SPLASH_FIFO			SPLASH_CACHEDIR"/.splash"
#define SPLASH_PIDFILE		SPLASH_CACHEDIR"/daemon.pid"
#define SPLASH_PROFILE		SPLASH_CACHEDIR"/profile"

#define SPLASH_CMD "bash -c 'export SOFTLEVEL='%s'; export BOOTLEVEL="RC_LEVEL_BOOT";" \
				   "export DEFAULTLEVEL="RC_LEVEL_DEFAULT"; export svcdir=${RC_SVCDIR};" \
				   ". /sbin/splash-functions.sh; %s %s %s'"

static char		**svcs = NULL;
static char		**svcs_done = NULL;
static int		svcs_cnt = 0;
static int		svcs_done_cnt = 0;
static int		progress = 0;
static scfg_t	config;
static pid_t	pid_daemon = 0;
static FILE*	fp_fifo = NULL;

static char** strlist_diff(char **a, char **b)
{
	char **res = NULL;
	int i, j;

	if (!a)
		return NULL;

	if (!b)
		return a;

	for (i = 0, j = 0; a[i]; i++) {
		if (b[j] && !strcmp(a[i], b[j])) {
			j++;
			continue;
		} else {
			res = rc_strlist_add(res, a[i]);
		}
	}

	return res;
}

static char** strlist_merge(char **dest, char **src)
{
	int i = 0;

	for (i = 0; src && src[i]; i++) {
		dest = rc_strlist_addsort(dest, src[i]);
	}

	return dest;
}

static int list_count(char **list)
{
	int c;

	for (c = 0; list && *list; list++)
		c++;

	return c;
}

static int list_has(char **list, const char *item)
{
	for (; list && *list; list++) {
		if (strcmp(*list, item) == 0)
			return true;
	}

	return false;
}

static char **get_list(char **list, const char *file)
{
	FILE *fp;
	char buffer[512];
	char *p;
	char *token;

	if (!(fp = fopen(file, "r"))) {
		ewarn("get_list `%s': %s", file, strerror(errno));
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
 * splash_call(cmd, arg1, arg2)
 *
 * Call a function from /sbin/splash-functions.sh.
 * This is rather slow, so use it only when really necessary.
 *
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

static int splash_profile(const char *fmt, ...)
{
	va_list ap;
	FILE *fp;
	float uptime;

	if (!config.profile)
		return 0;

	fp = fopen("/proc/uptime", "r");
	if (!fp)
		return -1;
	fscanf(fp, "%f", &uptime);
	fclose(fp);

	fp = fopen(SPLASH_PROFILE, "a");
	if (!fp)
		return -1;
	va_start(ap, fmt);
	fprintf(fp, "%.2f: ", uptime);
	vfprintf(fp, fmt, ap);
	fclose(fp);
	va_end(ap);
	return 0;
}


static int splash_theme_hook(const char *name, const char *type, const char *arg1)
{
	char *buf;
	int l = 256;

	if (arg1)
		splash_profile("%s %s %s\n", type, name, arg1);
	else
		splash_profile("%s %s\n", type, name);

	l += strlen(name);
	l += strlen(config.theme);

	buf = malloc(l * sizeof(char*));
	snprintf(buf, 1024, "/etc/splash/%s/scripts/%s-%s", config.theme, name, type);

	if (!rc_is_exec(buf)) {
		free(buf);
		return 0;
	}

	l = splash_call(buf, arg1, NULL);
	free(buf);
	return l;
}

/*
 * splash_send(cmd)
 *
 * Send cmd to the splash daemon.
 */
static int splash_send(char *cmd)
{
	if (!fp_fifo) {
		fp_fifo = fopen(SPLASH_FIFO, "a");
		if (!fp_fifo)
			return -1;
		setbuf(fp_fifo, NULL);
/*		t = fileno(fp_fifo);
		flags = fcntl(t, F_GETFL);
		fcntl(t, F_SETFL, flags | O_NONBLOCK);
*/	}
	/* FIXME: this is risky, because we could be getting a SIGPIPE..
	 * needs to be fixed. */

	fprintf(fp_fifo, cmd);
	splash_profile("comm %s", cmd);
	return 0;
}

static int splash_svc_state(const char *name, const char *state, bool paint)
{
	char buf[512];
	int l;

	if (paint)
		splash_theme_hook(state, "pre", name);

	l = snprintf(buf, 512, "update_svc %s %s\n", name, state);

	splash_send(buf);

	if (paint) {
		splash_send("paint\n");
		splash_theme_hook(state, "post", name);
	}

	return 0;
}

static int splash_daemon_check()
{
	int err = 0;
	FILE *fp;
	char buf[64];

	fp = fopen(SPLASH_PIDFILE, "r");
	if (!fp) {
		eerror("Failed to open "SPLASH_PIDFILE);
		return -1;
	}

	if (fscanf(fp, "%d", &pid_daemon) != 1) {
		eerror("Failed to get the PID of the splash daemon.");
		fclose(fp);
		return -1;
	}
	fclose(fp);

	sprintf(buf, "/proc/%d/stat", pid_daemon);
	fp = fopen(buf, "r");
	if (!fp)
		goto stale;

	if (fscanf(fp, "%*d (%s)", buf) != 1 || strncmp(buf, DAEMON_NAME, strlen(DAEMON_NAME)))
		goto stale;

out:
	fclose(fp);
	return err;
stale:
	err = -1;
	eerror("Stale pidfile. Splash daemon not running.");
	goto out;
}

static int splash_init(bool start)
{
	char **tmp, **tmp2;

	if (svcs)
		ewarn("splash_init: We already have a svcs list!");

	/* Booting.. */
	if (start) {
		svcs = get_list(NULL, SPLASH_CACHEDIR"/svcs_start");
		svcs_done = rc_services_in_state(rc_service_started);

		tmp = rc_services_in_state(rc_service_inactive);
		svcs_done = strlist_merge(svcs_done, tmp);
		rc_strlist_free(tmp);
		tmp = NULL;

		tmp = rc_services_in_state(rc_service_failed);
		svcs_done = strlist_merge(svcs_done, tmp);
		rc_strlist_free(tmp);
		tmp = NULL;
	/* .. or rebooting? */
	} else {
		svcs = get_list(NULL, SPLASH_CACHEDIR"/svcs_stop");

		tmp = rc_services_in_state(rc_service_started);

		tmp2 = rc_services_in_state(rc_service_inactive);
		tmp = strlist_merge(tmp, tmp2);
		rc_strlist_free(tmp2);
		tmp2 = NULL;

		tmp2 = rc_services_in_state(rc_service_stopping);
		tmp = strlist_merge(tmp, tmp2);
		rc_strlist_free(tmp2);
		tmp2 = NULL;

		svcs_done = strlist_diff(svcs, tmp);
		rc_strlist_free(tmp);
		tmp = NULL;
	}

	svcs_cnt = list_count(svcs);
	svcs_done_cnt = list_count(svcs_done);

	if (splash_daemon_check())
		return -1;

	return 0;
}

static int splash_svc_handle(const char *name, const char *state)
{
	char buf[512];

	if (list_has(svcs_done, name))
		return 0;

	if (svcs_cnt == 0)
		return -1;

	/* Add this service to the list of services that have
	 * already been handled. */
	rc_strlist_add(svcs_done, name);
	svcs_done_cnt++;

	/* Recalculate progress */
	progress = svcs_done_cnt * 65535 / svcs_cnt;

	splash_theme_hook(state, "pre", name);
	splash_svc_state(name, state, 0);
	snprintf(buf, 512, "progress %d\n", progress);
	splash_send(buf);
	splash_send("paint\n");
	splash_theme_hook(state, "post", name);

	return 0;
}

int splash_svcs_stop()
{
	char **tmp = NULL, **tmp2 = NULL;
	char *s;
	int i;
	FILE *fp;

	tmp = rc_services_in_state(rc_service_started);
	tmp2 = rc_services_in_state(rc_service_inactive);
	tmp = strlist_merge(tmp, tmp2);
	rc_strlist_free(tmp2);

	fp = fopen(SPLASH_CACHEDIR"/svcs_stop", "w");
	if (!fp) {
		ewarn("splash_svcs_stop `%s': %s", SPLASH_CACHEDIR"/svcs_stop", strerror(errno));
		return -1;
	}

	i = 0;
	if (tmp && tmp[0])
		while ((s = tmp[i++])) {
			if (i > 0) {
				fprintf(fp, " ");
			fprintf(fp, "%s", s);
		}
	}

	rc_strlist_free(tmp);

	fclose(fp);
	return 0;
}

/*
 * splash_start(runlevel)
 *
 * Start the splash daemon during boot/reboot.
 *
 */
static int splash_start(const char *runlevel)
{
	bool start;
	int i, err = 0;
	char buf[128];

	/* Get a list of services that we'll have to handle. */

	/* We're rebooting/shutting down. Just get a list of services that
	 * have been started. */
	if (!strcmp(runlevel, RC_LEVEL_SHUTDOWN) || !strcmp(runlevel, RC_LEVEL_REBOOT)) {
		splash_call("splash_cache_prep", "stop", NULL);
		start = false;
		splash_svcs_stop();
	/* We're booting. A list of services that will be started is
	 * prepared for us by splash_cache_prep(). */
	} else {
		splash_call("splash_cache_prep", "start", NULL);
		start = true;
	}

	splash_theme_hook("rc_init", "pre", runlevel);
	splash_call("splash_start", NULL, NULL);
	/* TODO:
	 * check if the env is sane (tty1, etc)
	 * prepare the FIFO
	 * get the boot message
	 * start the splash daemon
	 */

	err = splash_init(start);
	if (err)
		return err;

	/* Set the initial state of all services. */
	for (i = 0; svcs && svcs[i]; i++) {
		splash_svc_state(svcs[i], start ? "svc_inactive_start" : "svc_inactive_stop", 0);
	}

	snprintf(buf, 128, "set tty silent %d\n", config.tty_s);
	splash_send(buf);
	splash_send("set mode silent\n");
	splash_send("repaint\n");
	return err;
}

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

	splash_call("splash_cache_cleanup", NULL, NULL);

	/* TODO:
	 * switch to verbose if running in silent mode. 
	 * kill it the hard way
	 */

	return 0;
}

int _splash_hook (rc_hook_t hook, const char *name)
{
	int i = 0;

	/* Do nothing if we are at a very early stage of booting-up. */
	if (hook == rc_hook_runlevel_start_in && !strcmp(name, RC_LEVEL_SYSINIT))
		return 0;

	if (!config.theme) {
		splash_config_init(&config);
		splash_parse_kcmdline(&config);
	}

	/* Don't do anything if we're not running in silent mode. */
	if (config.reqmode != 's')
		return 0;

	/* Don't do anything if we're starting/stopping a service, but
	 * we aren't in the middle of a runlevel switch. */
	switch (hook) {
	case rc_hook_service_start_in:
	case rc_hook_service_stop_in:
	case rc_hook_service_start_out:
	case rc_hook_service_stop_out:
		if (!rc_runlevel_starting() && !rc_runlevel_stopping())
			return 0;

	default:
		break;
	}

	switch (hook) {

	case rc_hook_runlevel_stop_in:

		/* Ignore the sysinit runlevel. */
		if (!strcmp(name, RC_LEVEL_SYSINIT))
			break;

		/* Start the splash daemon on reboot. The theme hook is called
		 * from splash_start(). */
		if (strcmp(name, RC_LEVEL_REBOOT) == 0 || strcmp(name, RC_LEVEL_SHUTDOWN) == 0)
			i = splash_start(name);
		else
			splash_theme_hook("rc_init", "pre", name);
		if (i)
			return i;

		splash_theme_hook("rc_init", "post", name);
		break;

	case rc_hook_runlevel_start_in:
		/* If we are here and it's not sysinit, then we had a runlevel
		 * switch during boot and we have to reinitialize our internal
		 * variables. */
		if (strcmp(name, RC_LEVEL_SYSINIT)) {
			if (splash_init(true))
				return -1;
		}
		break;

	case rc_hook_runlevel_start_out:
		/* Start the splash daemon during boot right after we finish
		 * sysinit. */
		if (strcmp(name, RC_LEVEL_SYSINIT) == 0) {
			i = splash_start(name);
			splash_theme_hook("rc_init", "post", name);
		}
		/* Stop the splash daemon after boot-up is finished. */
		else if (strcmp(name, RC_LEVEL_BOOT)) {
			splash_theme_hook("rc_exit", "pre", name);
			i = splash_stop(name);
			splash_theme_hook("rc_exit", "post", name);
		}
		break;

	case rc_hook_service_start_in:
		/* If we're starting or stopping a service, we're being called by
		 * runscript and thus have to reload our config. */
		if (splash_init(true))
			return -1;
		i = splash_svc_handle(name, "svc_start");
		break;

	case rc_hook_service_start_out:
		if (rc_service_state(name, rc_service_started)) {
			i = splash_svc_state(name, "svc_started", 1);
		} else {
			i = splash_svc_state(name, "svc_start_failed", 1);
			/* FIXME: switch to verbose if verbose_on_errors */
		}
		break;

	case rc_hook_service_stop_in:
		if (splash_init(false))
			return -1;

		/* We need to stop localmount from unmounting our cache dir.
		   Luckily plugins can add to the unmount list. */
		if (name && strcmp(name, "localmount") == 0) {
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

	case rc_hook_service_stop_out:
		if (rc_service_state(name, rc_service_stopped))
			i = splash_svc_state(name, "svc_stopped", 1);
		else
			i = splash_svc_state(name, "svc_stop_failed", 1);
			/* FIXME: switch to verbose if verbose_on_errors */
		break;

	default:
		return i;
	}

	return i;
}
