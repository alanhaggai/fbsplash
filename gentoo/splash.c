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

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <linux/kd.h>
#include <linux/fs.h>

#if !defined(MNT_DETACH)
	#define MNT_DETACH 2
#endif

#include <einfo.h>
#include <rc.h>
#include "splash.h"

#ifndef LIBDIR
	#define LIBDIR "lib"
#endif

#define SPLASH_CACHEDIR		"/"LIBDIR"/splash/cache"
#define SPLASH_TMPDIR		"/"LIBDIR"/splash/tmp"
#define SPLASH_FIFO			SPLASH_CACHEDIR"/.splash"
#define SPLASH_PIDFILE		SPLASH_CACHEDIR"/daemon.pid"
#define SPLASH_PROFILE		SPLASH_CACHEDIR"/profile"
#define SPLASH_EXEC			"/sbin/splash_util.static"

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
		dest = rc_strlist_add(dest, src[i]);
	}

	return dest;
}

static char** strlist_merge_sort(char **dest, char **src)
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

static int splash_sanity_check()
{
	FILE *fp;
	char buf[128];

	fp = popen("/bin/grep -E -e '(^| )CONSOLE=/dev/tty1( |$)' -e '(^| )console=tty1( |$)' /proc/cmdline", "r");
	if (!fp)
		goto err;

	buf[0] = 0;
	fgets(buf, 128, fp);
	if (strlen(buf) == 0) {
		pclose(fp);
		goto err;
	}
	pclose(fp);
	return 0;

err:
	/* Clear display. */
	printf("\e[H\e[2J");
	fflush(stdout);

	ewarn("You don't appear to have a correct console= setting on your kernel");
	ewarn("command line. Silent splash will not be enabled. Please add");
	ewarn("console=tty1 or CONSOLE=/dev/tty1 to your kernel command line");
	ewarn("to avoid this message.");
	return -1;
}

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
		if (!strcasecmp(t, "on"))
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
		int fd;

		fd = open(SPLASH_FIFO, O_WRONLY | O_NONBLOCK);
		if (fd == -1) {
			eerror("Failed to open "SPLASH_FIFO": %s %s",
					strerror(errno), (errno == ENXIO) ? "(is the splash daemon running?)" : "");
			return -1;
		}

		fp_fifo = fdopen(fd, "w");
		if (!fp_fifo) {
			eerror("Failed to fdopen "SPLASH_FIFO": %s", strerror(errno));
			return -1;
		}
		setbuf(fp_fifo, NULL);
	}

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
		svcs_done = strlist_merge_sort(svcs_done, tmp);
		rc_strlist_free(tmp);
		tmp = NULL;

		tmp = rc_services_in_state(rc_service_failed);
		svcs_done = strlist_merge_sort(svcs_done, tmp);
		rc_strlist_free(tmp);
		tmp = NULL;
	/* .. or rebooting? */
	} else {
		svcs = get_list(NULL, SPLASH_CACHEDIR"/svcs_stop");

		tmp = rc_services_in_state(rc_service_started);

		tmp2 = rc_services_in_state(rc_service_inactive);
		tmp = strlist_merge_sort(tmp, tmp2);
		rc_strlist_free(tmp2);
		tmp2 = NULL;

		tmp2 = rc_services_in_state(rc_service_stopping);
		tmp = strlist_merge_sort(tmp, tmp2);
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

int splash_cache_prep(bool start)
{
	if (mount("cachedir", SPLASH_CACHEDIR, "tmpfs", MS_MGC_VAL, "mode=0644,size=4096k")) {
		eerror("Unable to create splash cache: %s", strerror(errno));
		/* FIXME: switch to verbose here? */
		return -1;
	}

	return 0;
}

int splash_cache_cleanup()
{
	int err = 0;
	char *what = SPLASH_CACHEDIR;

	if (!config.profile)
		goto nosave;

	if (!rc_is_dir(SPLASH_TMPDIR)) {
		unlink(SPLASH_TMPDIR);
		if ((err = mkdir(SPLASH_TMPDIR, 0700))) {
			eerror("Failed to create " SPLASH_TMPDIR": %s", strerror(errno));
			goto nosave;
		}
	}

	if ((err = mount(SPLASH_CACHEDIR, SPLASH_TMPDIR, NULL, MS_MOVE, NULL))) {
		eerror("Failed to move splash cache: %s", strerror(errno));
		goto nosave;
	}

	err = system("/bin/mv "SPLASH_TMPDIR"/profile "SPLASH_CACHEDIR"/profile");
	what = SPLASH_TMPDIR;

nosave:
	/* Clear a stale mtab entry created by /etc/init.d/checkroot */
	system("/bin/sed -i -e '\\#"SPLASH_CACHEDIR"# d' /etc/mtab");

	umount2(what, MNT_DETACH);
	return err;
}

int splash_svcs_stop()
{
	char **tmp = NULL, **tmp2 = NULL;
	char *s;
	int i;
	FILE *fp;

	tmp = rc_services_in_state(rc_service_started);
	tmp2 = rc_services_in_state(rc_service_inactive);
	tmp = strlist_merge_sort(tmp, tmp2);
	rc_strlist_free(tmp2);

	/* FIXME:
	 *  - merge starting services
	 *  - calculate correct order
	 */

	fp = fopen(SPLASH_CACHEDIR"/svcs_stop", "w");
	if (!fp) {
		ewarn("splash_svcs_stop `%s': %s", SPLASH_CACHEDIR"/svcs_stop", strerror(errno));
		return -1;
	}

	i = 0;
	if (tmp && tmp[0]) {
		while ((s = tmp[i++])) {
			if (i > 1) 
				fprintf(fp, " ");
			fprintf(fp, "%s", s);
		}
	}

	rc_strlist_free(tmp);

	fclose(fp);
	return 0;
}

int splash_svcs_start()
{
	rc_depinfo_t *deptree = NULL;
	FILE *fp;
	char **t1, **t2, **t3, **deporder, **types, *r, *s, *runlevel = NULL;
	int i, j;

	/* FIXME: handle ksoftlevel */

	if ((deptree = rc_load_deptree()) == NULL) {
		eerror("splash_svcs_start: failed to load deptree");
		return -1;
	}

	fp = fopen(SPLASH_CACHEDIR"/svcs_start", "w");
	if (!fp) {
		ewarn("splash_svcs_start `%s': %s", SPLASH_CACHEDIR"/svcs_start", strerror(errno));
		return -1;
	}

	/*
	 * This part is a little tricky, because we have to follow
	 * exactly what happens in /sbin/rc.
	 *
	 * First we get the order of services that are started in the
	 * boot runlevel and that have been coldplugged.
	 *
	 * When this code is called, we actually are in the boot runlevel
	 * so things go smoothly here.
	 *
	 */
	t1 = rc_services_in_state(rc_service_coldplugged);
	t2 = rc_services_in_runlevel(RC_LEVEL_BOOT);
	t1 = strlist_merge_sort(t1, t2);
	rc_strlist_free(t2);

	/* Get the services in correct order. */
	types = rc_strlist_add(NULL, "ineed");
	types = rc_strlist_add(types, "iuse");
	types = rc_strlist_add(types, "iafter");

	deporder = rc_get_depends(deptree, types, t1, RC_DEP_TRACE | RC_DEP_STRICT);

	/* Save what we've got so far to the svcs_start. */
	i = 0;
	if (deporder && deporder[0]) {
		while ((s = deporder[i++])) {
			if (i > 1)
				fprintf(fp, " ");
			fprintf(fp, "%s", s);
		}
	}

	t3 = deporder;
	rc_strlist_free(t1);

	/*
	 * This part is responsible for getting a list of the remaining
	 * services. We build a big list from boot, default and coldplugged
	 * services. We then trick RC to think it's already in the default
	 * level and we get the services correctly ordered. After that, the
	 * real runlevel is restored.
	 */
	t1 = rc_services_in_runlevel(RC_LEVEL_BOOT);
	t2 = rc_services_in_state(rc_service_coldplugged);
	t1 = strlist_merge(t1, t2);
	rc_strlist_free(t2);
	t2 = rc_services_in_runlevel(RC_LEVEL_DEFAULT);
	t1 = strlist_merge(t1, t2);
	rc_strlist_free(t2);

	/* Temporarily pretend that we're already in the 'default' runlevel. */
	s = rc_get_runlevel();
	if (s) {
		runlevel = strdup(s);
	} else {
		runlevel = strdup(RC_LEVEL_BOOT);
	}
	rc_set_runlevel(RC_LEVEL_DEFAULT);

	deporder = rc_get_depends(deptree, types, t1, RC_DEP_TRACE | RC_DEP_STRICT);

	/* Restore the real runlevel. */
	rc_set_runlevel(runlevel);

	/* Print the new services and skip ones that have already been started
	 * in the 'boot' runlevel. */
	i = 0;
	if (deporder && deporder[0]) {
		while ((s = deporder[i])) {
			j = 0;
			while ((r = t3[j++])) {
				if (!strcmp(deporder[i], r))
					 goto next;
			}
			fprintf(fp, " %s", s);
next:		i++;
		}
	}

	fclose(fp);

	rc_strlist_free(t3);
	rc_strlist_free(deporder);
	rc_strlist_free(t1);

	rc_strlist_free(types);
	rc_free_deptree(deptree);

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
	char buf[2048];
	char buf2[128];
	FILE *fp;

	char *evdev_cmds[] = {
		"/bin/grep -Hsi keyboard /sys/class/input/event*/device/driver/description | /bin/grep -o 'event[0-9]\\+'",
		"/bin/grep -s -m 1 '^H: Handlers=kbd' /proc/bus/input/devices | grep -o 'event[0-9]\\+'"
	};


	/* Get a list of services that we'll have to handle. */

	/* We're rebooting/shutting down. Just get a list of services that
	 * have been started. */
	if (!strcmp(runlevel, RC_LEVEL_SHUTDOWN) || !strcmp(runlevel, RC_LEVEL_REBOOT)) {
		if ((err = splash_cache_prep(false)))
			return err;
		splash_svcs_stop();
		start = false;
	/* We're booting. A list of services that will be started is
	 * prepared for us by splash_cache_prep(). */
	} else {
		if ((err = splash_cache_prep(true)))
			return err;
		splash_svcs_start();
		start = true;
	}
	splash_theme_hook("rc_init", "pre", runlevel);

	/* Perform sanity checks (console=, CONSOLE=). */
	if (!config.insane) {
		err = splash_sanity_check();
		if (err)
			return err;
	}

	/* Start the splash daemon */
	snprintf(buf, 2048, "BOOT_MSG='%s' " SPLASH_EXEC " -d --theme=\"%s\" --pidfile=" SPLASH_PIDFILE " %s",
			 config.message, config.theme, (config.kdmode == KD_GRAPHICS) ? "--kdgraphics" : "");

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

	/* Try to activate the event device interface so that F2 can
	 * be used to switch from verbose to silent. */
	buf2[0] = 0;
	for (i = 0; i < sizeof(evdev_cmds)/sizeof(char*); i++) {
		fp = popen(evdev_cmds[i], "r");
		if (fp) {
			fgets(buf2, 128, fp);
			if (strlen(buf2) > 0) {
				if (buf2[strlen(buf2)-1] == '\n')
					buf2[strlen(buf2)-1] = 0;
				break;
			}
			pclose(fp);
		}
	}

	if (buf2[0] != 0) {
		snprintf(buf, 128, "set event dev /dev/input/%s\n", buf2);
		splash_send(buf);
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

	return splash_cache_cleanup();

	/* TODO:
	 * switch to verbose if running in silent mode. 
	 * kill it the hard way
	 */
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

	/* Do nothing if we are at a very early stage of booting-up. */
	switch (hook) {
	case rc_hook_runlevel_start_in:
	case rc_hook_runlevel_start_out:
	case rc_hook_runlevel_stop_in:
	case rc_hook_runlevel_stop_out:
		if (!strcmp(name, RC_LEVEL_SYSINIT))
			return 0;
	default:
		break;
	}

	if (!config.theme) {
		splash_config_init(&config, type);
		splash_config_gentoo(&config, type);
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
		/* Start the splash daemon on reboot. The theme hook is called
		 * from splash_start(). */
		if (strcmp(name, RC_LEVEL_REBOOT) == 0 || strcmp(name, RC_LEVEL_SHUTDOWN) == 0) {
			i = splash_start(name);
			splash_theme_hook("rc_init", "post", name);
			return i;
		} else {
			splash_theme_hook("rc_exit", "pre", name);
			splash_theme_hook("rc_exit", "post", name);
		}
		break;

	case rc_hook_runlevel_start_in:
		/* Start the splash daemon during boot right after we finish
		 * sysinit and are entering the boot runlevel. Due to historical
		 * reasons, we simulate a full sysinit cycle here for the theme
		 * scripts. */
		if (strcmp(name, RC_LEVEL_BOOT) == 0) {
			i = splash_start(RC_LEVEL_SYSINIT);
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
