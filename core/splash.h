#ifndef __SPLASH_H
#define __SPLASH_H

#include <stdbool.h>
#include <stdio.h>
#include <linux/kd.h>

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

#define SPL_THEME_DIR		"/etc/splash"
#define SPL_DEFAULT_THEME	"default"
#define SPL_PROGRESS_MAX	0xffff

/* Effects */
#define SPL_EFF_NONE		0
#define SPL_EFF_FADEIN		1
#define SPL_EFF_FADEOUT		2

struct spl_theme;

typedef enum { spl_undef, spl_bootup, spl_reboot, spl_shutdown } spl_type_t;

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
	spl_type_t type;	/* bootup/reboot/shutdown? */

	/* rc system data */
	bool profile;	/* enable profiling? */
	bool insane;	/* skip sanity checks? */
	bool vonerr;	/* switch to verbose on errors? */

	/* daemon data */
	bool minstances;	/* allow multiple instances of the splash daemon? */
	int progress;		/* current value of progress */
	char verbosity;		/* verbosity level */
} spl_cfg_t;

spl_cfg_t* splash_lib_init(spl_type_t type);
int splash_lib_cleanup();
int splash_init_config(spl_type_t type);
int splash_parse_kcmdline(bool sysmsg);
void splash_get_res(char *theme, int *xres, int *yres);
int splash_profile(const char *fmt, ...);
bool splash_is_silent(void);
int splash_set_verbose(void);
void splash_theme_set(char *theme);
void splash_message_set(char *msg);
int splash_set_silent(void);
bool splash_set_evdev(void);
int splash_check_daemon(int *pid_daemon, bool verbose);
bool splash_check_sanity(void);
int splash_cache_prep(void);
int splash_cache_cleanup(char **profile_save);
int splash_send(const char *fmt, ...);

/*
 * Link with libsplashrender if you want to use the functions
 * below.
 */
int splashr_init(bool create);
void splashr_cleanup();
int splashr_render_buf(struct spl_theme *theme, void *buffer, bool repaint, char mode);
int splashr_render_screen(struct spl_theme *theme, bool repaint, bool bgnd, char mode, char effects);
struct spl_theme *splashr_theme_load();
void splashr_theme_free(struct spl_theme *theme);
int splashr_tty_silent_init();
int splashr_tty_silent_cleanup();
int splashr_tty_silent_set(int);
bool splashr_tty_silent_update();
void splashr_message_set(struct spl_theme *theme, char *msg);
void splashr_progress_set(struct spl_theme *theme, int progress);

#endif /* __SPLASH_H */
