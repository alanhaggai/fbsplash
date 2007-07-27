#ifndef __SPLASH_H
#define __SPLASH_H

#include <stdbool.h>
#include <stdio.h>

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

#define THEME_DIR		"/etc/splash"
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
#define EFF_FADEOUT		2

struct fb_data;
struct stheme;

typedef enum sp_type { undef, bootup, reboot, shutdown } stype_t;

typedef struct
{
	char reqmode;	/* requested splash mode:
					 *  s - silent & verbose
					 *  t - silent only
					 *  v - verbose only
					 *  o - off
					 */
	char *theme;	/* theme */
	char *message;	/* system message */
	int kdmode;		/* KD_TEXT or KD_GRAPHICS */
	char effects;	/* fadein, etc */
	int tty_s;		/* silent tty */
	int tty_v;		/* verbose tty */
	char *pidfile;	/* pidfile */
	stype_t type;	/* bootup/reboot/shutdown? */

	/* rc system data */
	bool profile;	/* enable profiling? */
	bool insane;	/* skip sanity checks? */
	bool vonerr;	/* switch to verbose on errors? */

	/* daemon data */
	bool minstances;	/* allow multiple instances of the splash daemon? */
	int progress;		/* current value of progress */
	char verbosity;		/* verbosity level */

	struct fb_data* fbd;
} scfg_t;

scfg_t* splash_lib_init(stype_t type);
int splash_lib_cleanup();
int splash_init_config(scfg_t *cfg, stype_t type);
int splash_parse_kcmdline(scfg_t *cfg, bool sysmsg);
void splash_get_res(char *theme, int *xres, int *yres);
int splash_profile(const char *fmt, ...);
bool splash_is_silent(void);
int splash_set_verbose(void);
int splash_set_silent(void);
bool splash_set_evdev(void);
int splash_check_daemon(int *pid_daemon, bool verbose);
bool splash_check_sanity(void);

int splash_cache_prep(void);
int splash_cache_cleanup(char **profile_save);
int splash_send(const char *fmt, ...);

/* \
 * Link with libsplashrender if you want to use the functions
 * below.
 */
int splash_render_init(bool create);
void splash_render_cleanup();
int splash_render_buf(struct stheme *theme, void *buffer, bool repaint, char mode);
int splash_render_screen(struct stheme *theme, bool repaint, bool bgnd, char mode, char effects);
struct stheme *splash_theme_load();
void splash_theme_free(struct stheme *theme);
int splash_tty_silent_init();
int splash_tty_silent_cleanup();
int splash_tty_silent_set(int);

#endif /* __SPLASH_H */
