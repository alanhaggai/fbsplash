#ifndef __UTIL_H
#define __UTIL_H

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <linux/kd.h>
#include <linux/tty.h>
#include <linux/input.h>
#include <linux/fb.h>
#include <limits.h>

#include "fbsplash.h"

/* XXX:
 * It should be perfectly OK to include sys/vt.h when building the kernel
 * helper, but we don't do that to avoid problems with broken klibc versions.
 */
#ifdef TARGET_KERNEL
	#include <linux/vt.h>
#else
	#include <sys/vt.h>
#endif

/* Common paths */
#define PATH_DEV	"/dev"
#define PATH_PROC	"/proc"
#define PATH_SYS	"/sys"

#if defined(TARGET_KERNEL)
	#define PATH_SYS	FBSPLASH_DIR"/sys"
	#define PATH_PROC	FBSPLASH_DIR"/proc"
#endif

/* Useful short-named types */
typedef u_int8_t	u8;
typedef u_int16_t	u16;
typedef u_int32_t	u32;
typedef int8_t		s8;
typedef int16_t		s16;
typedef int32_t		s32;

typedef enum { little, big } sendian_t;

/* Message levels */
#define MSG_CRITICAL	0
#define MSG_ERROR		1
#define MSG_WARN		2
#define MSG_INFO		3

#define SYSMSG_DEFAULT  "Initializing the kernel..."
#define SYSMSG_BOOTUP	"Booting the system ($progress%)... Press F2 for verbose mode."
#define SYSMSG_REBOOT	"Rebooting the system ($progress%)... Press F2 for verbose mode."
#define SYSMSG_SHUTDOWN	"Shutting down the system ($progress%)... Press F2 for verbose mode."

#define DEFAULT_FONT	"luxisri.ttf"
#define TTF_DEFAULT		FBSPL_THEME_DIR"/"DEFAULT_FONT
#define DAEMON_NAME		"fbsplashd"

/* Default TTYs for silent and verbose modes. */
#define TTY_SILENT		16
#define TTY_VERBOSE		1

/* Useful macros */
#define min(a,b)		((a) < (b) ? (a) : (b))
#define max(a,b)		((a) > (b) ? (a) : (b))

#define iprint(type, args...) do {				\
	if (config.verbosity == FBSPL_VERB_QUIET)		\
		break;									\
												\
	if (type <= MSG_ERROR) {					\
		fprintf(stderr, ## args);				\
	} else if (config.verbosity == FBSPL_VERB_HIGH) {	\
		fprintf(stdout, ## args);				\
	}											\
} while (0);

#define CLAMP(x)		((x) > 255 ? 255 : (x))
#define DEBUG(x...)

#define WANT_TTF	((defined(CONFIG_TTF_KERNEL) && defined(TARGET_KERNEL)) || (defined(CONFIG_TTF) && !defined(TARGET_KERNEL)))
#define WANT_MNG	(defined(CONFIG_MNG) && !defined(TARGET_KERNEL))

#endif /* __UTIL_H */
