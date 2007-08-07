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
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mount.h>

#include <linux/fs.h>

#if !defined(MNT_DETACH)
	#define MNT_DETACH 2
#endif

#include "util.h"

#define eerror(args...)		{ fprintf(stderr, ## args); fprintf(stderr, "\n"); }
#define ewarn(args...)		{ fprintf(stdout, ## args); fprintf(stdout, "\n"); }

static FILE *fp_fifo = NULL;
int fd_tty0 = -1;
spl_cfg_t config;
sendian_t endianess;

/* A list of loaded fonts. */
list fonts = { NULL, NULL };

static void detect_endianess(sendian_t *end)
{
	u16 t = 0x1122;

	if (*(u8*)&t == 0x22) {
		*end = little;
	} else {
		*end = big;
	}
}

/**
 * Init the splash library.
 *
 * @param  type One of: spl_undef, spl_bootup, spl_reboot, spl_shutdown
 * @return Pointer to a config structure used by all routines in libsplash.
 *         You should not attempt to free this pointer.  If a splash_acc_*
 *         function exists to set a members of this config structure, use it
 *         instead of setting the member directly.
 */
spl_cfg_t* splash_lib_init(spl_type_t type)
{
	detect_endianess(&endianess);
	splash_init_config(type);

	/* The kernel helper cannot touch any tty devices. */
#ifndef TARGET_KERNEL
	if (fd_tty0 == -1) {
		fd_tty0 = open(PATH_DEV "/tty0", O_RDWR);
	}
#endif
	return &config;
}

/**
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

/**
 * Initialize the config structure with default values.
 *
 * @param type One of spl_reboot, spl_shutdown, spl_bootup, spl_undef.
 */
int splash_init_config(spl_type_t type)
{
	char *s;

	config.tty_s = TTY_SILENT;
	config.tty_v = TTY_VERBOSE;
	config.kdmode = KD_TEXT;
	config.insane = false;
	config.profile = false;
	config.vonerr = false;
	config.reqmode = 'o';
	config.minstances = false;
	config.progress = 0;
	config.effects = SPL_EFF_NONE;
	config.verbosity = VERB_NORMAL;
	config.type = type;
	splash_acc_theme_set(SPL_DEFAULT_THEME);

	s = getenv("PROGRESS");
	if (s)
		config.progress = atoi(s);

	s = getenv("BOOT_MSG");
	if (s) {
		config.message = strdup(s);
	} else {
		switch (type) {
		case spl_reboot:
			config.message = strdup(SYSMSG_REBOOT);
			break;

		case spl_shutdown:
			config.message = strdup(SYSMSG_SHUTDOWN);
			break;

		case spl_bootup:
			config.message = strdup(SYSMSG_BOOTUP);
			break;

		case spl_undef:
		default:
			config.message = strdup(SYSMSG_DEFAULT);
			break;
		}
	}
	return 0;
}

/**
 * Parse the kernel command line to get splash settings and save
 * them in 'config'.
 *
 * @param sysmsg If true, the command line will be scanned for the BOOT_MSG
 *               splash message override.
 */
int splash_parse_kcmdline(bool sysmsg)
{
	FILE *fp;
	char *p, *t, *opt, *pbuf;
	char *buf = malloc(1024);
	char quot = 0;
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
			splash_acc_message_set(t);
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
				int tty = strtol(opt+4, NULL, 0);
				if (tty < 0 || tty > MAX_NR_CONSOLES) {
					config.tty_s = TTY_SILENT;
				} else {
					config.tty_s = tty;
				}
			} else if (!strcmp(opt, "fadein")) {
				config.effects |= SPL_EFF_FADEIN;
			} else if (!strcmp(opt, "fadeout")) {
				config.effects |= SPL_EFF_FADEOUT;
			} else if (!strcmp(opt, "verbose")) {
				config.reqmode = 'v';
			} else if (!strcmp(opt, "silent")) {
				config.reqmode = 's';
			} else if (!strcmp(opt, "silentonly")) {
				config.reqmode = 't';
			} else if (!strcmp(opt, "off")) {
				config.reqmode = 'o';
			} else if (!strcmp(opt, "insane")) {
				config.insane = true;
			} else if (!strncmp(opt, "theme:", 6)) {
				splash_acc_theme_set(opt+6);
			} else if (!strcmp(opt, "kdgraphics")) {
				config.kdmode = KD_GRAPHICS;
			} else if (!strcmp(opt, "profile")) {
				config.profile = true;
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

/**
 * Switch to verbose mode.
 *
 * @param old_tty If larger than 0, switch to old_tty instead of the verbose tty
 *                defined by config.tty_v.
 */
int splash_set_verbose(int old_tty)
{
	if (fd_tty0 == -1)
		return -1;
	if (old_tty > 0)
		return ioctl(fd_tty0, VT_ACTIVATE, old_tty);
	else
		return ioctl(fd_tty0, VT_ACTIVATE, config.tty_v);
}

/**
 * Check whether the silent splash screen is being displayed.
 *
 * @return True if the silent splash is displayed, false otherwise.
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

/**
 * Get the resolution that the splash screen will use.
 *
 * @param theme Theme name.
 * @param xres Preferred horizontal resolution (e.g. fb_var.xres).
 * @param yres Preferred vertical resolution (e.g. fb_var.yres).
 */
void splash_get_res(const char *theme, int *xres, int *yres)
{
	FILE *fp;
	char buf[512];
	int oxres, oyres;

	oxres = *xres;
	oyres = *yres;
	snprintf(buf, 512, SPL_THEME_DIR "/%s/%dx%d.cfg", theme, oxres, oyres);

	fp = fopen(buf, "r");
	if (!fp) {
		unsigned int t, tx, ty, mdist = 0xffffffff;
		struct dirent *dent;
		DIR *tdir;

		snprintf(buf, 512, SPL_THEME_DIR "/%s", theme);
		tdir = opendir(buf);
		if (!tdir) {
			*xres = 0;
			*yres = 0;
			return;
		}

		while ((dent = readdir(tdir))) {
			if (sscanf(dent->d_name, "%dx%d.cfg", &tx, &ty) != 2)
				continue;

			/* We only want configs for resolutions smaller than the current one,
			 * so that we can actually fit the image on the screen. */
			if (tx >= oxres || ty >= oyres)
				continue;

			t = (tx - oxres) * (tx - oxres) + (ty - oyres) * (ty - oyres);

			/* Penalize configs for resolutions with different aspect ratios. */
			if (oxres / oyres != tx / ty)
				t *= 10;

			if (t < mdist) {
				*xres = tx;
				*yres = ty;
				mdist = t;
			}
		}
		closedir(tdir);
	} else {
		fclose(fp);
	}
}

/**
 * Accessor function for config.theme
 *
 * @param theme The new theme setting.
 */
void splash_acc_theme_set(const char *theme)
{
	if (config.theme)
		free(config.theme);

	config.theme = strdup(theme);
}

/**
 * Accessor function for config.message
 *
 * @param msg The new message setting.
 */
void splash_acc_message_set(const char *msg)
{
	if (config.message)
		free(config.message);

	config.message = strdup(msg);
}

/**
 * Switch to silent mode.
 *
 * @param tty_prev Previous tty. If not NULL, the tty used before the switch to silent
 *                 mode will be saved in it.
 */
int splash_set_silent(int *tty_prev)
{
	struct vt_stat vtstat;

	if (tty_prev && fd_tty0 != -1) {
		if (ioctl(fd_tty0, VT_GETSTATE, &vtstat) != -1) {
			*tty_prev = vtstat.v_active;
		}
	}

#ifndef TARGET_KERNEL
	if (fd_tty0 == -1) {
		return splash_send("set mode silent\n");
	} else
#endif
	{
		return ioctl(fd_tty0, VT_ACTIVATE, config.tty_s);
	}
}

#ifndef TARGET_KERNEL
/**
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

/**
 * Clean the splash cache.
 *
 * @param profile_save A strlist of files that should be saved to the hdd
 *                     if profiling is enabled.
 */
int splash_cache_cleanup(char **profile_save)
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

	if (profile_save) {
		char buf[PATH_MAX];
		err = 0;

		while (*profile_save) {
			snprintf(buf, PATH_MAX, "/bin/mv "SPLASH_TMPDIR"/%s "SPLASH_CACHEDIR"/%s", *profile_save, *profile_save);
			err += system(buf);
			profile_save++;
		}
	}

	what = SPLASH_TMPDIR;

nosave:
	/* Clear a stale mtab entry that might have been created by the initscripts. */
	system("/bin/sed -i -e '\\#"SPLASH_CACHEDIR"# d' /etc/mtab");

	umount2(what, MNT_DETACH);
	return err;
}

/**
 * Check that the splash daemon is running.
 *
 * @param pid_daemon Will be set to the PID of the splash daemon if
 *                   it is found to be running.
 * @param verbose    Print errors if true, be silent otherwise.
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

/**
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

/**
 * Try to set the event device for the splash daemon.
 */
bool splash_set_evdev(void)
{
	char buf[128];
	FILE *fp;
	int i, j;

	char *evdev_cmds[] = {
		"/bin/grep -Hsi keyboard " PATH_SYS "/class/input/input*/name | /bin/sed -e 's#.*input\\([0-9]*\\)/name.*#event\\1#'",
		"/bin/grep -Hsi keyboard " PATH_SYS "/class/input/event*/device/driver/description | /bin/grep -o 'event[0-9]\\+'",
		"for i in " PATH_SYS "/class/input/input* ; do if [ \"$((0x$(cat $i/capabilities/ev) & 0x100002))\" = \"1048578\" ] ; then echo $i | sed -e 's#.*input\\([0-9]*\\)#event\\1#' ; fi ; done",
		"/bin/grep -s -m 1 '^H: Handlers=kbd' " PATH_PROC "/bus/input/devices | /bin/grep -o 'event[0-9]\\+'"
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

/**
 * Save splash profiling data.
 *
 * @param fmt Format of the data to be saved (printf style).
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

/**
 * Send stuff to the splash daemon using the splash FIFO.
 *
 * @param fmt Format of the data to be sent (printf style).
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

