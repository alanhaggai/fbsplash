/*
 * libsplash.c - A library of useful splash routines to be used in RC systems
 *               and helper programs.
 *
 * Copyright (c) 2007, Michal Januszewski <spock@gentoo.org>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2.  See the file COPYING in the main directory of this archive for
 * more details.
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mount.h>

#include <linux/kd.h>
#include <linux/vt.h>
#include <linux/fs.h>

#if !defined(MNT_DETACH)
	#define MNT_DETACH 2
#endif

/* If we're not on Gentoo, define eerror() and ewarn() */
#ifndef CONFIG_GENTOO
	#if !defined(eerror)
		#define eerror(args...)		fprintf(stderr, ## args);
	#endif
	#if !defined(ewarn)
		#define ewarn(args...)		fprintf(stdout, ##args);
	#endif
#else
	#include <einfo.h>
#endif

#include "splash.h"

static FILE *fp_fifo = NULL;
static int fd_tty0 = -1;
static scfg_t config;

/*
 * Init the splash library.
 *
 * Arguments:
 *  - type: undef, bootup, reboot, shutdown
 *
 * Returns:
 *  - pointer to a config structure used by all routines
 *    in libsplash.
 */
scfg_t* splash_lib_init(stype_t type)
{
	splash_init_config(&config, type);

	/* The kernel helper cannot touch any tty devices. */
#ifndef TARGET_KERNEL
	if (fd_tty0 == -1) {
		fd_tty0 = open(PATH_DEV "/tty0", O_RDWR);
	}
#endif
	return &config;
}

/*
 * Clean up after splash_lib_init() and subsequent calls
 * to any libsplash routines.
 */
int splash_lib_cleanup(void)
{
	if (config.theme) {
		free(config.theme);
		config.theme = NULL;
	}

	if (config.message) {
		free(config.message);
		config.message = NULL;
	}

	if (fp_fifo) {
		fclose(fp_fifo);
		fp_fifo = NULL;
	}

	if (fd_tty0 >= 0) {
		close(fd_tty0);
		fd_tty0 = -1;
	}
	return 0;
}

/*
 * Initialize the 'cfg' structure with default values.
 */
int splash_init_config(scfg_t *cfg, stype_t type)
{
	char *s;

	cfg->tty_s = TTY_SILENT;
	cfg->tty_v = TTY_VERBOSE;
	cfg->theme = strdup(DEFAULT_THEME);
	cfg->kdmode = KD_TEXT;
	cfg->insane = false;
	cfg->profile = false;
	cfg->vonerr = false;
	cfg->reqmode = 'o';
	cfg->minstances = false;
	cfg->progress = 0;
	cfg->effects = EFF_NONE;
	cfg->verbosity = VERB_NORMAL;

	s = getenv("PROGRESS");
	if (s)
		cfg->progress = atoi(s);

	s = getenv("BOOT_MSG");
	if (s) {
		cfg->message = strdup(s);
	} else {
		switch (type) {
		case reboot:
			cfg->message = strdup(SYSMSG_REBOOT);
			break;

		case shutdown:
			cfg->message = strdup(SYSMSG_SHUTDOWN);
			break;

		case bootup:
			cfg->message = strdup(SYSMSG_BOOTUP);
			break;

		case undef:
		default:
			cfg->message = strdup(SYSMSG_DEFAULT);
			break;
		}
	}
	return 0;
}

/*
 * Parse the kernel command line to get splash settings
 * and save them in 'cfg'.
 */
int splash_parse_kcmdline(scfg_t *cfg, bool sysmsg)
{
	FILE *fp;
	char *p, *t, *opt, *pbuf;
	char *buf = malloc(1024);
	char quot;
	int err = 0, i, len;

	if (!buf)
		return -1;

	fp = fopen(PATH_PROC "/cmdline", "r");
	if (!fp)
		goto fail;

	fgets(buf, 1024, fp);
	fclose(fp);

	len = strlen(buf);

	/* Remove the newline character so that it doesn't get
	 * included in the theme name. */
	if (buf[len-1] == '\n') {
		buf[len-1] = 0;
	}

	if (sysmsg) {
		t = strstr(buf, "BOOT_MSG=\"");
		if (t) {
			t += 10;
			for (i = 0; i < len-(t-buf) && buf[i]; i++) {
				if (t[i] == '"' && !quot)
					break;
				if (t[i] == '\\') {
					quot = 1;
				} else {
					quot = 0;
				}
			}
			t[i] = 0;
			if (cfg->message)
				free(cfg->message);
			cfg->message = strdup(t);
			t[i] = '"';
		}
	}

	pbuf = buf;

	/* We support multiple splash= arguments on the kernel command line.
	 * The arguments are passed from left to right. This is useful with
	 * some bootloaders which don't let the user modify their config during
	 * boot but which do provide a way to add arguments to the kernel. */
	while ((pbuf = strstr(pbuf, "splash="))) {

		t = pbuf;
		t += 7; p = t;

		for (p = t; *p != ' ' && *p != 0; p++);

		if (*p != 0)
			pbuf = p+1;
		else
			pbuf = p;

		*p = 0;

		while ((opt = strsep(&t, ",")) != NULL) {

			if (!strncmp(opt, "tty:", 4)) {
				cfg->tty_v = strtol(opt+4, NULL, 0);
			} else if (!strncmp(opt, "fadein", 6)) {
				cfg->effects |= EFF_FADEIN;
			} else if (!strncmp(opt, "verbose", 7)) {
				cfg->reqmode = 'v';
			} else if (!strncmp(opt, "silent", 6)) {
				cfg->reqmode = 's';
			} else if (!strncmp(opt, "off", 3)) {
				cfg->reqmode = 'o';
			} else if (!strncmp(opt, "insane", 6)) {
				cfg->insane = true;
			} else if (!strncmp(opt, "theme:", 6)) {
				if (cfg->theme)
					free(cfg->theme);
				cfg->theme = strdup(opt+6);
			} else if (!strncmp(opt, "kdgraphics", 10)) {
				cfg->kdmode = KD_GRAPHICS;
			} else if (!strncmp(opt, "profile", 7)) {
				cfg->profile = true;
			}
		}
	}

out:
	free(buf);
	return err;

fail:
	err = -1;
	goto out;
}

/*
 * Switch to verbose mode.
 */
int splash_set_verbose(void)
{
	if (fd_tty0 == -1)
		return -1;
	return ioctl(fd_tty0, VT_ACTIVATE, config.tty_v);
}

/*
 * Returns true if the silent splash screen is currently displayed.
 */
bool splash_is_silent(void)
{
	struct vt_stat vtstat;

	if (fd_tty0 == -1)
		return false;

	if (ioctl(fd_tty0, VT_GETSTATE, &vtstat) != -1) {
		return (vtstat.v_active == config.tty_s);

	} else {
		return false;
	}
}

#ifndef TARGET_KERNEL
/*
 * Switch to silent mode.
 */
int splash_set_silent(void)
{
	if (fd_tty0 == -1) {
		splash_send("set mode silent\n");
		return 0;
	} else {
		return ioctl(fd_tty0, VT_ACTIVATE, config.tty_s);
	}
}

/*
 * Prepare a writable splash cache.
 */
int splash_cache_prep(void)
{
	if (mount("cachedir", SPLASH_CACHEDIR, "tmpfs", MS_MGC_VAL, "mode=0644,size=4096k")) {
		eerror("Unable to create splash cache: %s", strerror(errno));
		return -1;
	}

	return 0;
}

/*
 * Clean the splash cache.
 */
int splash_cache_cleanup(void)
{
	int err = 0;
	char *what = SPLASH_CACHEDIR;
	struct stat buf;

	if (!config.profile)
		goto nosave;

	if (stat(SPLASH_TMPDIR, &buf) != 0 || !S_ISDIR(buf.st_mode)) {
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

	err  = system("/bin/mv "SPLASH_TMPDIR"/profile "SPLASH_CACHEDIR"/profile");
	err += system("/bin/mv "SPLASH_TMPDIR"/svcs_start "SPLASH_CACHEDIR"/svcs_start");
//	err += system("/bin/mv "SPLASH_TMPDIR"/svc_timings "SPLASH_CACHEDIR"/svc_timings");
	what = SPLASH_TMPDIR;

nosave:
	/* Clear a stale mtab entry that might have been created by the initscripts. */
	system("/bin/sed -i -e '\\#"SPLASH_CACHEDIR"# d' /etc/mtab");

	umount2(what, MNT_DETACH);
	return err;
}

/*
 * Check that the splash daemon is running. Sets 'pid_daemon'
 * to the PID of the splash daemon if it's found running.
 */
int splash_check_daemon(int *pid_daemon, bool verbose)
{
	int err = 0;
	FILE *fp;
	char buf[64];

	fp = fopen(SPLASH_PIDFILE, "r");
	if (!fp) {
		if (verbose)
			eerror("Failed to open "SPLASH_PIDFILE);
		return -1;
	}

	if (fscanf(fp, "%d", pid_daemon) != 1) {
		if (verbose)
			eerror("Failed to get the PID of the splash daemon.");
		fclose(fp);
		return -1;
	}
	fclose(fp);

	sprintf(buf, PATH_PROC "/%d/stat", *pid_daemon);
	fp = fopen(buf, "r");
	if (!fp)
		goto stale;

	/* Check if the process name matches DAEMON_NAME */
	if (fscanf(fp, "%*d (%s)", buf) != 1 || strncmp(buf, DAEMON_NAME, strlen(DAEMON_NAME)))
		goto stale;

out:
	if (fp)
		fclose(fp);
	return err;
stale:
	err = -1;
	if (verbose)
		eerror("Stale pidfile. Splash daemon not running.");
	goto out;
}

/*
 * Perform sanity checks to make sure that it's safe to start the
 * splash daemon.
 */
bool splash_check_sanity(void)
{
	FILE *fp;
	char buf[128];

	/* Do nothing if 'insane' is set. */
	if (config.insane)
		return true;

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
	return true;

err:
	/* Clear display. */
	printf("\e[H\e[2J");
	fflush(stdout);

	ewarn("You don't appear to have a correct console= setting on your kernel");
	ewarn("command line. Silent splash will not be enabled. Please add");
	ewarn("console=tty1 or CONSOLE=/dev/tty1 to your kernel command line");
	ewarn("to avoid this message.");
	return false;
}

/*
 * Try to set the event device for the splash daemon.
 */
bool splash_set_evdev(void)
{
	char buf[128];
	FILE *fp;
	int i, j;

	char *evdev_cmds[] = {
		"/bin/grep -Hsi keyboard " PATH_SYS "/class/input/event*/device/driver/description | /bin/grep -o 'event[0-9]\\+'",
		"/bin/grep -s -m 1 '^H: Handlers=kbd' " PATH_PROC "/bus/input/devices | grep -o 'event[0-9]\\+'"
	};

	/* Try to activate the event device interface so that F2 can
	 * be used to switch from verbose to silent. */
	buf[0] = 0;
	for (i = 0; i < sizeof(evdev_cmds)/sizeof(char*); i++) {
		fp = popen(evdev_cmds[i], "r");
		if (fp) {
			fgets(buf, 128, fp);
			if ((j = strlen(buf)) > 0) {
				if (buf[j-1] == '\n')
					buf[j-1] = 0;
				break;
			}
			pclose(fp);
		}
	}

	if (buf[0] != 0) {
		splash_send("set event dev " PATH_DEV "/input/%s\n", buf);
		return true;
	} else {
		return false;
	}
}

/*
 * Save splash profiling data.
 */
int splash_profile(const char *fmt, ...)
{
	va_list ap;
	FILE *fp;
	float uptime;

	if (!config.profile)
		return 0;

	fp = fopen(PATH_PROC "/uptime", "r");
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

/*
 * Send stuff to the splash daemon using the splash FIFO.
 */
int splash_send(const char *fmt, ...)
{
	char cmd[256];
	va_list ap;

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

	va_start(ap, fmt);
	vsnprintf(cmd, 256, fmt, ap);
	va_end(ap);

	fprintf(fp_fifo, cmd);
	splash_profile("comm %s", cmd);
	return 0;
}

#endif /* TARGET_KERNEL */

