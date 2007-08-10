#ifndef __FBSPLASH_H
#define __FBSPLASH_H

#include <stdbool.h>
#include <stdio.h>
#include <linux/kd.h>

#define LIBDIR "/lib"
#define SPLASH_CACHEDIR		LIBDIR"/splash/cache"
#define SPLASH_PIDFILE		SPLASH_CACHEDIR"/daemon.pid"
#define SPLASH_PROFILE		SPLASH_CACHEDIR"/profile"
#define SPLASH_DAEMON		"/sbin/fbsplashd.static"

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

/* Verbosity levels */
#define SPL_VERB_QUIET		0
#define SPL_VERB_NORMAL		1
#define SPL_VERB_HIGH	    2

/* Splash mode flags */
#define SPL_MODE_OFF	 0x00
#define SPL_MODE_VERBOSE 0x01
#define SPL_MODE_SILENT  0x02

struct spl_theme;

typedef enum { spl_undef, spl_bootup, spl_reboot, spl_shutdown } spl_type_t;

typedef struct
{
	char reqmode;	/* a combination of SPL_MODE_ flags */
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

spl_cfg_t* fbsplash_lib_init(spl_type_t type);
int fbsplash_lib_cleanup();
int fbsplash_parse_kcmdline(bool sysmsg);
void fbsplash_get_res(const char *theme, int *xres, int *yres);
int fbsplash_profile(const char *fmt, ...);
bool fbsplash_is_silent(void);
int fbsplash_set_verbose(int old_tty);
int fbsplash_set_silent();
void fbsplash_acc_theme_set(const char *theme);
void fbsplash_acc_message_set(const char *msg);
int fbsplash_set_evdev(void);
int fbsplash_check_daemon(int *pid_daemon);
int fbsplash_check_sanity(void);
int fbsplash_cache_prep(void);
int fbsplash_cache_cleanup(char **profile_save);
int fbsplash_send(const char *fmt, ...);

/*
 * Link with libfbsplashrender if you want to use the functions
 * below.
 */
int fbsplashr_init(bool create);
void fbsplashr_cleanup();
int fbsplashr_render_buf(struct spl_theme *theme, void *buffer, bool repaint);
int fbsplashr_render_screen(struct spl_theme *theme, bool repaint, bool bgnd, char effects);
struct spl_theme *fbsplashr_theme_load();
void fbsplashr_theme_free(struct spl_theme *theme);
int fbsplashr_tty_silent_init();
int fbsplashr_tty_silent_cleanup();
int fbsplashr_tty_silent_set(int tty);
int fbsplashr_tty_silent_update();
void fbsplashr_message_set(struct spl_theme *theme, const char *msg);
void fbsplashr_progress_set(struct spl_theme *theme, int progress);

int fbsplashr_input_init();
void fbsplashr_input_cleanup();
unsigned char fbsplashr_input_getkey(bool block);

#endif /* __FBSPLASH_H */
