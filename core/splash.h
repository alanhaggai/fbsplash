#ifndef __SPLASH_H
#define __SPLASH_H

#include <stdbool.h>
#include <stdio.h>

#include "config.h"

/* Adjustable settings */
#define PATH_DEV	"/dev"
#define PATH_PROC	"/proc"
#define PATH_SYS	"/sys"
#define SPLASH_DEV	PATH_DEV"/fbsplash"

#ifndef LIBDIR
	#define LIBDIR "lib"
#endif

#define SPLASH_CACHEDIR		"/"LIBDIR"/splash/cache"
#define SPLASH_TMPDIR		"/"LIBDIR"/splash/tmp"
#define SPLASH_PIDFILE		SPLASH_CACHEDIR"/daemon.pid"
#define SPLASH_PROFILE		SPLASH_CACHEDIR"/profile"
#define SPLASH_EXEC			"/sbin/splash_util.static"

#ifndef SPLASH_FIFO
	#define SPLASH_FIFO			SPLASH_CACHEDIR"/.splash"
#endif

/* Default TTYs for silent and verbose modes. */
#define TTY_SILENT		8
#define TTY_VERBOSE		1

#define DEFAULT_FONT	"luxisri.ttf"
#define DEFAULT_THEME	"default"
#define DAEMON_NAME		"splash_util"
#define TTF_DEFAULT		THEME_DIR"/"DEFAULT_FONT

#define SYSMSG_DEFAULT  "Initializing the kernel..."
#define SYSMSG_BOOTUP	"Booting the system ($progress%)... Press F2 for verbose mode."
#define SYSMSG_REBOOT	"Rebooting the system ($progress%)... Press F2 for verbose mode."
#define SYSMSG_SHUTDOWN	"Shutting down the system ($progress%)... Press F2 for verbose mode."

/* Non-adjustable settings. */
#define PROGRESS_MAX	0xffff

#if defined(TARGET_KERNEL)
	#define PATH_SYS	"/lib/splash/sys"
	#define PATH_PROC	"/lib/splash/proc"
#endif

/* Verbosity levels */
#define VERB_QUIET		0
#define VERB_NORMAL		1
#define VERB_HIGH	    2

/* Effects */
#define EFF_NONE		0
#define EFF_FADEIN		1

typedef enum sp_type { undef, bootup, reboot, shutdown } stype_t;

typedef struct
{
	char reqmode;	/* requested splash mode */
	char *theme;	/* theme */
	char *message;	/* system message */
	int kdmode;		/* KD_TEXT or KD_GRAPHICS */
	char effects;	/* fadein, etc */
	int tty_s;		/* silent tty */
	int tty_v;		/* verbose tty */
	char *pidfile;	/* pidfile */

	/* rc system data */
	bool profile;	/* enable profiling? */
	bool insane;	/* skip sanity checks? */
	bool vonerr;	/* switch to verbose on errors? */

	/* daemon data */
	bool minstances;	/* allow multiple instances of the splash daemon? */
	int progress;		/* current value of progress */
	char verbosity;		/* verbosity level */

} scfg_t;

scfg_t* splash_lib_init(stype_t type);
int splash_lib_cleanup();
int splash_init_config(scfg_t *cfg, stype_t type);
int splash_parse_kcmdline(scfg_t *cfg, bool sysmsg);
int splash_profile(const char *fmt, ...);
bool splash_is_silent(void);
int splash_set_verbose(void);
int splash_set_silent(void);
bool splash_set_evdev(void);
int splash_check_daemon(int *pid_daemon, bool verbose);
bool splash_check_sanity(void);

int splash_cache_prep(void);
int splash_cache_cleanup(void);
int splash_send(const char *fmt, ...);

#endif /* __SPLASH_H */
