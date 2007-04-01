#ifndef __SPLASH_H
#define __SPLASH_H

#include <stdbool.h>
#include <sys/types.h>

/* Adjustable settings */
#define PATH_DEV	"/dev"
#define PATH_PROC	"/proc"
#define PATH_SYS	"/sys"
#define SPLASH_DEV	PATH_DEV"/fbsplash"

#if defined(TARGET_KERNEL)
	#define PATH_SYS	"/splash/sys"
	#define PATH_PROC	"/splash/proc"
#endif

#ifndef LIBDIR
	#define LIBDIR "lib"
#endif

#define SPLASH_CACHEDIR		"/"LIBDIR"/splash/cache"
#define SPLASH_TMPDIR		"/"LIBDIR"/splash/tmp"
#define SPLASH_FIFO			SPLASH_CACHEDIR"/.splash"
#define SPLASH_PIDFILE		SPLASH_CACHEDIR"/daemon.pid"
#define SPLASH_PROFILE		SPLASH_CACHEDIR"/profile"
#define SPLASH_EXEC			"/sbin/splash_util.static"

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

typedef enum sp_type { undef, bootup, reboot, shutdown } stype_t;

typedef struct
{
	char reqmode;	/* requested splash mode */
	char *theme;	/* theme */
	char *message;	/* system message */
	int kdmode;		/* KD_TEXT or KD_GRAPHICS */
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

int splash_config_init(scfg_t *cfg, stype_t type);
int splash_parse_kcmdline(scfg_t *cfg);
int splash_verbose(scfg_t *cfg);

void vt_cursor_enable(int fd);
void vt_cursor_disable(int fd);

int tty_set_silent(int tty, int fd);
int tty_unset_silent(int fd);


enum sp_states { st_display, st_svc_inact_start, st_svc_inact_stop, st_svc_start,
				 st_svc_started, st_svc_stop, st_svc_stopped, st_svc_stop_failed,
				 st_svc_start_failed };

/* Necessary to avoid compilation errors when fbsplash support is
   disabled. */
#if !defined(CONFIG_FBSPLASH)
	#define FB_SPLASH_IO_ORIG_USER		0
	#define FB_SPLASH_IO_ORIG_KERNEL	1
#endif

/* Useful short-named types */
typedef u_int8_t	u8;
typedef u_int16_t	u16;
typedef u_int32_t	u32;
typedef int8_t		s8;
typedef int16_t		s16;
typedef int32_t		s32;

/* Verbosity levels */
#define VERB_QUIET		0
#define VERB_NORMAL		1
#define VERB_HIGH	    2

/* Message levels */
#define MSG_CRITICAL	0
#define MSG_ERROR		1
#define MSG_WARN		2
#define MSG_INFO		3

/* Useful macros */
#define min(a,b)		((a) < (b) ? (a) : (b))
#define max(a,b)		((a) > (b) ? (a) : (b))

#define iprint(type, args...) do {				\
	if (arg_verbosity == VERB_QUIET)			\
		break;									\
												\
	if (type <= MSG_ERROR) {					\
		fprintf(stderr, ## args);				\
	} else if (arg_verbosity == VERB_HIGH) {	\
		fprintf(stdout, ## args);				\
	}											\
} while (0);

#endif /* __SPLASH_H */
