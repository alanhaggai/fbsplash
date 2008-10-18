/*
 * splash.c - Splash plugin for the Gentoo RC system.
 *
 * Copyright (c) 2007-2008, Michal Januszewski <spock@gentoo.org>
 *
 * Original splash plugin compatible with baselayout-1's splash-functions.sh
 * written by Roy Marples <uberlord@gentoo.org>.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */
#include <sys/stat.h>
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

/* Some queue.h implenetations don't have this macro */
#ifndef TAILQ_CONCAT
#define TAILQ_CONCAT(head1, head2, field) do {                          \
	if (!TAILQ_EMPTY(head2)) {                                      \
		*(head1)->tqh_last = (head2)->tqh_first;                \
		(head2)->tqh_first->field.tqe_prev = (head1)->tqh_last; \
		(head1)->tqh_last = (head2)->tqh_last;                  \
		TAILQ_INIT((head2));                                    \
	}                                                               \
} while (0)
#endif

#define SPLASH_CMD "export SPLASH_XRES='%d'; export SPLASH_YRES='%d';" \
				   "export SOFTLEVEL='%s'; export BOOTLEVEL='%s';" \
				   "export DEFAULTLEVEL='%s'; export svcdir=${RC_SVCDIR};" \
				   ". /sbin/splash-functions.sh; %s %s %s"

static char		*bootlevel = NULL;
static char		*defaultlevel = NULL;
static RC_STRINGLIST	*svcs = NULL;
static RC_STRINGLIST	*svcs_done = NULL;
static int		svcs_cnt = 0;
static int		svcs_done_cnt = 0;
static pid_t	pid_daemon = 0;
static fbspl_cfg_t	*config = NULL;

static int		xres = 0;
static int		yres = 0;

/*
 * Check whether a strlist contains a specific item.
 */
static bool list_has(RC_STRINGLIST *list, const char *item)
{
	RC_STRING *s;

	if (list) {
		TAILQ_FOREACH(s, list, entries)
			if (strcmp(s->value, item) == 0)
				return true;
	}
	return false;
}

/*
 * Count the number of items in a strlist.
 */
static int strlist_count(RC_STRINGLIST *list)
{
	RC_STRING *s;
	int c = 0;

	if (list)
		TAILQ_FOREACH(s, list, entries)
			c++;

	return c;
}

/*
 * Create a strlist from a file pointer. Can be used
 * to get a list of words printed by an app/script.
 */
static void get_list_fp(RC_STRINGLIST *list, FILE *fp)
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
				rc_stringlist_add(list, p);
			}
		}
	}
}

/*
 * Create a strlist from a file. Used for svcs_start/svcs_stop.
 */
static void get_list(RC_STRINGLIST *list, const char *file)
{
	FILE *fp;

	if (!(fp = fopen(file, "r"))) {
		ewarn("%s: `%s': %s", __func__, file, strerror(errno));
	} else {
		get_list_fp(list, fp);
		fclose(fp);
	}
}

/*
 * Get splash settings from /etc/conf.d/splash
 */
static int splash_config_gentoo(fbspl_cfg_t *cfg, fbspl_type_t type)
{
	RC_STRINGLIST *confd;
	char *t;

	confd = rc_config_load("/etc/conf.d/splash");

	t = rc_config_value(confd, "SPLASH_KDMODE");
	if (t) {
		if (!strcasecmp(t, "graphics")) {
			cfg->kdmode = KD_GRAPHICS;
		} else if (!strcasecmp(t, "text")) {
			cfg->kdmode = KD_TEXT;
		}
	}

	t = rc_config_value(confd, "SPLASH_PROFILE");
	if (t) {
		if (!strcasecmp(t, "on") || !strcasecmp(t, "yes"))
			cfg->profile = true;
	}

	t = rc_config_value(confd, "SPLASH_TTY");
	if (t) {
		int i;
		if (sscanf(t, "%d", &i) == 1 && i > 0) {
			cfg->tty_s = i;
		}
	}

	t = rc_config_value(confd, "SPLASH_THEME");
	if (t)
		fbsplash_acc_theme_set(t);

	t = rc_config_value(confd, "SPLASH_MODE_REQ");
	if (t) {
		if (!strcasecmp(t, "verbose")) {
			cfg->reqmode = FBSPL_MODE_VERBOSE;
		} else if (!strcasecmp(t, "silent")) {
			cfg->reqmode = FBSPL_MODE_VERBOSE | FBSPL_MODE_SILENT;
		} else if (!strcasecmp(t, "silentonly")) {
			cfg->reqmode = FBSPL_MODE_SILENT;
		}
	}

	t = rc_config_value(confd, "SPLASH_VERBOSE_ON_ERRORS");
	if (t && (!strcasecmp(t, "on") || !strcasecmp(t, "yes")))
		cfg->vonerr = true;

	switch(type) {
	case fbspl_reboot:
		t = rc_config_value(confd, "SPLASH_REBOOT_MESSAGE");
		if (t)
			fbsplash_acc_message_set(t);
		break;

	case fbspl_shutdown:
		t = rc_config_value(confd, "SPLASH_SHUTDOWN_MESSAGE");
		if (t)
			fbsplash_acc_message_set(t);
		break;

	case fbspl_bootup:
	default:
		t = rc_config_value(confd, "SPLASH_BOOT_MESSAGE");
		if (t)
			fbsplash_acc_message_set(t);
		break;
	}

	t = rc_config_value(confd, "SPLASH_TEXTBOX");
	if (t) {
		if (!strcasecmp(t, "on") || !strcasecmp(t, "yes"))
			cfg->textbox_visible = true;
	}

	t = rc_config_value(confd, "SPLASH_AUTOVERBOSE");
	if (t) {
		cfg->autoverbose = atoi(t);
	}

	t = rc_config_value(confd, "SPLASH_EFFECTS");
	if (t) {
		char *opt;

		while ((opt = strsep(&t, ",")) != NULL) {
			if (!strcmp(opt, "fadein")) {
				cfg->effects |= FBSPL_EFF_FADEIN;
			} else if (!strcmp(opt, "fadeout")) {
				cfg->effects |= FBSPL_EFF_FADEOUT;
			}
		}
	}

	rc_stringlist_free(confd);
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
	char *soft = getenv("RC_RUNLEVEL");

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
	struct stat st;

	if (arg1)
		fbsplash_profile("%s %s %s\n", type, name, arg1);
	else
		fbsplash_profile("%s %s\n", type, name);

	l += strlen(name);
	l += strlen(config->theme);

	buf = malloc(l * sizeof(char*));
	snprintf(buf, l, "/etc/splash/%s/scripts/%s-%s", config->theme, name, type);
	if (stat(buf, &st) != 0) {
		free(buf);
		return 0;
	}

	/*
	 * Set the 2nd parameter to 0 so that we don't break themes using the
	 * legacy interface in which events could have up to two parameters.
	 */
	l = splash_call(buf, arg1, "0");
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

	if (!strcmp(state, "svc_started")) {
		fbsplash_send("log Service '%s' started.\n", name);
	} else if (!strcmp(state, "svc_start_failed")) {
		fbsplash_send("log Service '%s' failed to start.\n", name);
	} else if (!strcmp(state, "svc_stopped")) {
		fbsplash_send("log Service '%s' stopped.\n", name);
	} else if (!strcmp(state, "svc_stop_failed")) {
		fbsplash_send("log Service '%s' failed to stop.\n", name);
	}

	fbsplash_send("update_svc %s %s\n", name, state);

	if (paint) {
		fbsplash_send("paint\n");
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

	fbsplash_get_res(config->theme, (int*)&var.xres, (int*)&var.yres);
	xres = var.xres;
	yres = var.yres;
}

/*
 * Init splash config variables and check that the splash daemon
 * is running.
 */
static int splash_init(bool start)
{
	RC_STRINGLIST *tmp;

	config->verbosity = FBSPL_VERB_QUIET;
	if (fbsplash_check_daemon(&pid_daemon)) {
		config->verbosity = FBSPL_VERB_NORMAL;
		return -1;
	}

	config->verbosity = FBSPL_VERB_NORMAL;

	if (svcs) {
		ewarn("%s: We already have a svcs list!", __func__);
		rc_stringlist_free(svcs);
	}
	svcs = rc_stringlist_new();

	/* Booting.. */
	if (start) {
		get_list(svcs, FBSPLASH_CACHEDIR"/svcs_start");
		svcs_cnt = strlist_count(svcs);

		svcs_done = rc_services_in_state(RC_SERVICE_STARTED);

		tmp = rc_services_in_state(RC_SERVICE_INACTIVE);
		if (svcs_done && tmp) {
			TAILQ_CONCAT(svcs_done, tmp, entries);
			free(tmp);
		} else if (tmp)
			svcs_done = tmp;

		tmp = rc_services_in_state(RC_SERVICE_FAILED);
		if (svcs_done && tmp) {
			TAILQ_CONCAT(svcs_done, tmp, entries);
			free(tmp);
		} else if (tmp)
			svcs_done = tmp;

		tmp = rc_services_in_state(RC_SERVICE_SCHEDULED);
		if (svcs_done && tmp) {
			TAILQ_CONCAT(svcs_done, tmp, entries);
			free(tmp);
		} else if (tmp)
			svcs_done = tmp;

		svcs_done_cnt = strlist_count(svcs_done);
	/* .. or rebooting? */
	} else {
		get_list(svcs, FBSPLASH_CACHEDIR"/svcs_stop");
		svcs_cnt = strlist_count(svcs);

		svcs_done = rc_services_in_state(RC_SERVICE_STARTED);

		tmp = rc_services_in_state(RC_SERVICE_STARTING);
		if (svcs_done && tmp) {
			TAILQ_CONCAT(svcs_done, tmp, entries);
			free(tmp);
		} else if (tmp)
			svcs_done = tmp;

		tmp = rc_services_in_state(RC_SERVICE_INACTIVE);
		if (svcs_done && tmp) {
			TAILQ_CONCAT(svcs_done, tmp, entries);
			free(tmp);
		} else if (tmp)
			svcs_done = tmp;

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

		if (!svcs_done)
			svcs_done = rc_stringlist_new();
		rc_stringlist_add(svcs_done, name);
		svcs_done_cnt++;
	}

	/* Recalculate progress */
	config->progress = svcs_done_cnt * FBSPL_PROGRESS_MAX / svcs_cnt;

	splash_theme_hook(state, "pre", name);
	splash_svc_state(name, state, 0);
	fbsplash_send("progress %d\n", config->progress);
	fbsplash_send("paint\n");
	splash_theme_hook(state, "post", name);

	return 0;
}

/*
 * Create a list of services that will be started during bootup.
 */
int splash_svcs_start()
{
	RC_DEPTREE *deptree;
	FILE *fp;
	RC_STRINGLIST *t, *deporder;
	RC_STRING *s, *r;
	int i, err = 0;

	fp = fopen(FBSPLASH_CACHEDIR"/svcs_start", "w");
	if (!fp) {
		ewarn("%s: `%s': %s", __func__, FBSPLASH_CACHEDIR"/svcs_start", strerror(errno));
		return -1;
	}

	if ((deptree = rc_deptree_load()) == NULL) {
		eerror("%s: failed to load deptree", __func__);
		err = -2;
		goto out;
	}

	deporder = rc_deptree_order(deptree, bootlevel, RC_DEP_START);

	/* Save what we've got so far to the svcs_start. */
	if (deporder) {
		i = 0;
		TAILQ_FOREACH(s, deporder, entries) {
			if (i > 0)
				fprintf(fp, " ");
			fprintf(fp, "%s", s->value);
			i++;
		}
	}

	t = deporder;
	deporder = rc_deptree_order(deptree, defaultlevel, RC_DEP_START);

	/* Print the new services and skip ones that have already been started
	 * in the 'boot' runlevel. */
	if (deporder) {
		TAILQ_FOREACH(s, deporder, entries) {
			char duplicate = 0;
			TAILQ_FOREACH(r, t, entries) {
				if (!strcmp(s->value, r->value)) {
					duplicate = 1;
					break;
				}
			}
			if (!duplicate) {
				fprintf(fp, " %s", s->value);
			}
		}
	}

	rc_stringlist_free(deporder);
	rc_stringlist_free(t);
	rc_deptree_free(deptree);

out:
	fclose(fp);
	return 0;
}

/*
 * Create a list of services that will be stopped during reboot/shutdown.
 */
int splash_svcs_stop(const char *runlevel)
{
	RC_DEPTREE *deptree;
	RC_STRINGLIST *deporder;
	RC_STRING *s;
	FILE *fp;
	int i, err = 0;

	fp = fopen(FBSPLASH_CACHEDIR"/svcs_stop", "w");
	if (!fp) {
		ewarn("%s: `%s': %s", __func__, FBSPLASH_CACHEDIR"/svcs_stop", strerror(errno));
		return -1;
	}

	if ((deptree = rc_deptree_load()) == NULL) {
		eerror("%s: failed to load deptree", __func__);
		err = -2;
		goto out;
	}

	deporder = rc_deptree_order(deptree, runlevel, RC_DEP_STOP);

	if (deporder) {
		i = 0;
		TAILQ_FOREACH(s, deporder, entries) {
			if (i > 0)
				fprintf(fp, " ");
			fprintf(fp, "%s", s->value);
			i++;
		}
	}

	rc_stringlist_free(deporder);
	rc_deptree_free(deptree);
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
	int err = 0;
	char buf[2048];
	RC_STRING *s;

	/* Get a list of services that we'll have to handle. */
	/* We're rebooting/shutting down. */
	if (!strcmp(runlevel, RC_LEVEL_SHUTDOWN) || !strcmp(runlevel, RC_LEVEL_REBOOT)) {
		if ((err = fbsplash_cache_prep()))
			return err;
		splash_svcs_stop(runlevel);
		start = false;
	/* We're booting. */
	} else {
		if ((err = fbsplash_cache_prep()))
			return err;
		splash_svcs_start();
		start = true;
	}
	splash_init_res();
	splash_theme_hook("rc_init", "pre", runlevel);

	/* Perform sanity checks (console=, CONSOLE= etc). */
	if (fbsplash_check_sanity())
		return -1;

	/* Start the splash daemon */
	snprintf(buf, 2048, "BOOT_MSG='%s' " FBSPLASH_DAEMON " --theme=\"%s\" --pidfile=" FBSPLASH_PIDFILE " --type=%s %s %s %s",
			 config->message, config->theme,
			 (config->type == fbspl_reboot) ? "reboot" : ((config->type == fbspl_shutdown) ? "shutdown" : "bootup"),
			 (config->kdmode == KD_GRAPHICS) ? "--kdgraphics" : "",
			 (config->textbox_visible) ? "--textbox" : "",
			 (config->effects & (FBSPL_EFF_FADEOUT | FBSPL_EFF_FADEIN)) ? "--effects=fadeout,fadein" :
				 ((config->effects & FBSPL_EFF_FADEOUT) ? "--effects=fadeout" :
					 ((config->effects & FBSPL_EFF_FADEIN) ? "--effects=fadein" : "")));

	err = system(buf);
	if (err == -1 || WEXITSTATUS(err) != 0) {
		eerror("Failed to start the splash daemon, error code %d", err);
		return err;
	}

	err = splash_init(start);
	if (err)
		return err;

	/* Set the initial state of all services. */
	if (svcs)
		TAILQ_FOREACH(s, svcs, entries)
			splash_svc_state(s->value, start ? "svc_inactive_start" : "svc_inactive_stop", 0);

	fbsplash_set_evdev();
	fbsplash_send("set autoverbose %d\n", config->autoverbose);
	fbsplash_send("set tty silent %d\n", config->tty_s);
	fbsplash_send("set mode silent\n");
	fbsplash_send("repaint\n");
	return err;
}

/*
 * Stop the splash daemon.
 */
static int splash_stop(const char *runlevel)
{
	char *save[] = { "profile", "svcs_start", NULL };
	char buf[128];
	struct stat st;
	int cnt = 0;

	if (rc_service_state("xdm") & RC_SERVICE_STARTED) {
		fbsplash_send("exit staysilent\n");
	} else {
		fbsplash_send("exit\n");
	}
	snprintf(buf, 128, "/proc/%d", pid_daemon);

	/* Wait up to 1.0s for the splash daemon to exit. */
	while (stat(buf, &st) == 0 && cnt < 100) {
		usleep(10000);
		cnt++;
	}

	/* Just to be sure we aren't stuck in a black ex-silent tty.. */
	if (fbsplash_is_silent() && !(rc_service_state("xdm") & RC_SERVICE_STARTED))
		fbsplash_set_verbose(0);

	/* If we don't get a runlevel argument, then we're being executed
	 * because of a rc-abort event and we don't save any data. */
	if (runlevel == NULL) {
		return fbsplash_cache_cleanup(NULL);
	} else {
		return fbsplash_cache_cleanup(save);
	}
}

int rc_plugin_hook(RC_HOOK hook, const char *name)
{
	int i = 0;
	fbspl_type_t type = fbspl_bootup;
	char *runlev;
	bool skip = false;
	int retval = 0;

	runlev = rc_runlevel_get();
	if (!strcmp(runlev, RC_LEVEL_REBOOT))
		type = fbspl_reboot;
	else if (!strcmp(runlev, RC_LEVEL_SHUTDOWN))
		type = fbspl_shutdown;

	/* Get boot and default levels from env variables exported by RC.
	 * If unavailable, use the default ones. */
	bootlevel = getenv("RC_BOOTLEVEL");
	defaultlevel = getenv("RC_DEFAULTLEVEL");

	/* We generally do nothing if we're in sysinit. Except if the
	 * autoconfig service is present, when we get a list of services
	 * that will be started by it and mark them as coldplugged. */
	if (name && !strcmp(name, RC_LEVEL_SYSINIT)) {
		if (hook == RC_HOOK_RUNLEVEL_START_OUT && rc_service_in_runlevel("autoconfig", defaultlevel)) {
			FILE *fp;
			RC_STRINGLIST *list;
			RC_STRING *s;

			fp = popen("if [ -e /etc/init.d/autoconfig ]; then . /etc/init.d/autoconfig ; list_services ; fi", "r");
			if (!fp)
				goto exit;

			list = rc_stringlist_new();
			get_list_fp(list, fp);
			TAILQ_FOREACH(s, list, entries)
				rc_service_mark(s->value, RC_SERVICE_COLDPLUGGED);
			pclose(fp);
			rc_stringlist_free(list);
		}
		goto exit;
	}

	/* Don't do anything if we're starting/stopping a service, but
	 * we aren't in the middle of a runlevel switch. */
	if (!(rc_runlevel_starting() || rc_runlevel_stopping())) {
		if (hook != RC_HOOK_RUNLEVEL_STOP_IN &&
			hook != RC_HOOK_RUNLEVEL_STOP_OUT &&
			hook != RC_HOOK_RUNLEVEL_START_IN &&
			hook != RC_HOOK_RUNLEVEL_START_OUT)
			goto exit;
	} else {
		/* We're starting/stopping a runlevel. Check whether we're
		 * actually booting/rebooting. */
		if (rc_runlevel_starting() && strcmp(runlev, bootlevel) &&
			strcmp(runlev, defaultlevel) && strcmp(runlev, RC_LEVEL_SYSINIT))
			goto exit;

		if (rc_runlevel_stopping() && strcmp(runlev, bootlevel) &&
			strcmp(runlev, RC_LEVEL_REBOOT) && strcmp(runlev, RC_LEVEL_SHUTDOWN))
			goto exit;
	}

	if (!config) {
		config = fbsplash_lib_init(type);
		splash_config_gentoo(config, type);
		fbsplash_parse_kcmdline(false);
	}

	/* Extremely weird.. should never happen. */
	if (!config) {
		retval = -1;
		goto exit;
	}

	/* Don't do anything if we're not running in silent mode. */
	if (!(config->reqmode & FBSPL_MODE_SILENT))
		goto exit;

	switch (hook) {
	case RC_HOOK_RUNLEVEL_STOP_IN:
		/* Start the splash daemon on reboot. The theme hook is called
		 * from splash_start(). */
		if (strcmp(name, RC_LEVEL_REBOOT) == 0 || strcmp(name, RC_LEVEL_SHUTDOWN) == 0) {
			if ((i = splash_start(name))) {
				fbsplash_set_verbose(0);
				retval= i;
				goto exit;
			} else {
				if (rc_service_state("gpm") & RC_SERVICE_STARTED) {
					fbsplash_send("set gpm\n");
					fbsplash_send("repaint\n");
				}
			}
			splash_theme_hook("rc_init", "post", name);
			retval = i;
			goto exit;
		} else {
			splash_theme_hook("rc_exit", "pre", name);
			splash_theme_hook("rc_exit", "post", name);
			fbsplash_lib_cleanup();
			config = NULL;
		}
		break;

	case RC_HOOK_RUNLEVEL_STOP_OUT:
		/* Make sure the progress indicator reaches 100%, even if
		 * something went wrong along the way. */
		if (strcmp(name, RC_LEVEL_REBOOT) == 0 || strcmp(name, RC_LEVEL_SHUTDOWN) == 0) {
			config->verbosity = FBSPL_VERB_QUIET;
			i = fbsplash_check_daemon(&pid_daemon);
			config->verbosity = FBSPL_VERB_NORMAL;
			if (i) {
				retval = -1;
				goto exit;
			}

			fbsplash_send("progress %d\n", FBSPL_PROGRESS_MAX);
			fbsplash_send("paint\n");
			fbsplash_cache_cleanup(NULL);
		}
		break;

	case RC_HOOK_RUNLEVEL_START_IN:
		/* Start the splash daemon during boot right after we finish
		 * sysinit and are entering the boot runlevel. Due to historical
		 * reasons, we simulate a full sysinit cycle here for the theme
		 * scripts. */
		if (strcmp(name, bootlevel) == 0) {
			if ((i = splash_start(RC_LEVEL_SYSINIT)))
				fbsplash_set_verbose(0);
			splash_theme_hook("rc_init", "post", RC_LEVEL_SYSINIT);
			splash_theme_hook("rc_exit", "pre", RC_LEVEL_SYSINIT);
			splash_theme_hook("rc_exit", "post", RC_LEVEL_SYSINIT);
		}
		splash_theme_hook("rc_init", "pre", name);
		splash_theme_hook("rc_init", "post", name);
		break;

	case RC_HOOK_RUNLEVEL_START_OUT:
		/* Stop the splash daemon after boot-up is finished. */
		if (strcmp(name, bootlevel)) {
			config->verbosity = FBSPL_VERB_QUIET;
			i = fbsplash_check_daemon(&pid_daemon);
			config->verbosity = FBSPL_VERB_NORMAL;
			if (i) {
				retval = -1;
				goto exit;
			}

			/* Make sure the progress indicator reaches 100%, even if
			 * something went wrong along the way. */
			fbsplash_send("progress %d\n", FBSPL_PROGRESS_MAX);
			fbsplash_send("paint\n");
			splash_theme_hook("rc_exit", "pre", name);
			i = splash_stop(name);
			splash_theme_hook("rc_exit", "post", name);
			fbsplash_lib_cleanup();
			config = NULL;
		}
		break;

	case RC_HOOK_SERVICE_START_NOW:
do_start:
		/* If we've been inactive, do nothing since the service has
		 * already been handled before it went inactive. */
		if (rc_service_state(name) & RC_SERVICE_WASINACTIVE)
			goto exit;

		/* If we're starting or stopping a service, we're being called by
		 * runscript and thus have to reload our config. */
		if (splash_init(true)) {
			retval = -1;
			goto exit;
		}
		i = splash_svc_handle(name, "svc_start", skip);
		break;

	case RC_HOOK_SERVICE_START_OUT:
		/* If a service gets scheduled, we want to increment the progress
		 * bar (as it is no longer blocking boot completion). However,
		 * the service may actually start during boot (some time after
		 * being scheduled), so we don't want to increment the progress
		 * bar twice. The following if clause satisfies this by catching
		 * the first case but not the second. */
		if ((rc_service_state(name) & RC_SERVICE_SCHEDULED) &&
		    !(rc_service_state(name) & RC_SERVICE_STARTING)) {
			skip = true;
			goto do_start;
		}
		break;

	case RC_HOOK_SERVICE_START_DONE:
		config->verbosity = FBSPL_VERB_QUIET;
		i = fbsplash_check_daemon(&pid_daemon);
		config->verbosity = FBSPL_VERB_NORMAL;
		if (i) {
			retval = -1;
			goto exit;
		}

		if (!(rc_service_state(name) & RC_SERVICE_FAILED) &&
		    !(rc_service_state(name) & RC_SERVICE_STOPPED)) {
			bool gpm = false;

			if (!strcmp(name, "gpm")) {
				struct stat st;
				int cnt = 0;
				gpm = true;
				/* Wait up to 0.25s for the GPM socket to appear. */
				while (stat("/dev/gpmctl", &st) == 0 && cnt < 25) {
					usleep(10000);
					cnt++;
				}
				fbsplash_send("set gpm\n");
			}

			i = splash_svc_state(name, "svc_started", 1);

			if (gpm) {
				fbsplash_send("repaint\n");
			}
		} else {
			i = splash_svc_state(name, "svc_start_failed", 1);
			if (config->vonerr) {
				fbsplash_set_verbose(0);
			}
		}
		fbsplash_lib_cleanup();
		config = NULL;
		break;

	case RC_HOOK_SERVICE_STOP_NOW:
		if (splash_init(false)) {
			retval = -1;
			goto exit;
		}

		/* We need to stop localmount from unmounting our cache dir.
		   Luckily plugins can add to the unmount list. */
		if (name && !strcmp(name, "localmount")) {
			char *umounts = getenv("RC_NO_UMOUNTS");

            if (umounts)
                fprintf(rc_environ_fd, "RC_NO_UMOUNTS=%s:%s", umounts, FBSPLASH_CACHEDIR);
            else
                fprintf(rc_environ_fd, "RC_NO_UMOUNTS=%s", FBSPLASH_CACHEDIR);
		}
		i = splash_svc_handle(name, "svc_stop", false);
		break;

	case RC_HOOK_SERVICE_STOP_DONE:
		config->verbosity = FBSPL_VERB_QUIET;
		i = fbsplash_check_daemon(&pid_daemon);
		config->verbosity = FBSPL_VERB_NORMAL;
		if (i) {
			retval = -1;
			goto exit;
		}

		if (rc_service_state(name) & RC_SERVICE_STOPPED) {
			i = splash_svc_state(name, "svc_stopped", 1);
		} else {
			i = splash_svc_state(name, "svc_stop_failed", 1);
			if (config->vonerr) {
				fbsplash_set_verbose(0);
			}
		}
		fbsplash_lib_cleanup();
		config = NULL;
		break;

	case RC_HOOK_ABORT:
		i = splash_stop(name);
		fbsplash_lib_cleanup();
		config = NULL;
		break;

	default:
		break;
	}

exit:	
	rc_stringlist_free(svcs);
	free (runlev);
	return i;
}
