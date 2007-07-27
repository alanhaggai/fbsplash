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
#include <limits.h>

#include "splash.h"

/* XXX:
 * It should be perfectly OK to include sys/vt.h when building the kernel
 * helper, but we don't do that to avoid problems with broken klibc versions.
 */
#ifdef TARGET_KERNEL
	#include <linux/vt.h>
#else
	#include <sys/vt.h>
#endif

/*
 * HACK WARNING:
 * -------------
 * Using the trick below we try to make sure that we always compile
 * against a single set of kernel headers -- the one against which
 * klibc was compiled. To do that, we skip the 'linux/' part of the
 * path when compiling the userspace utilities. This ensures that
 * fb.h and console_splash.h are pulled from /usr/lib/klibc/include/linux
 * and not /usr/include.
 */
#ifdef TARGET_KERNEL
	#include <linux/fb.h>
	#ifdef CONFIG_FBSPLASH
		#include <linux/console_splash.h>
	#endif
#else
	#include <fb.h>
	#ifdef CONFIG_FBSPLASH
		#include <console_splash.h>
	#endif
#endif

/* Necessary to avoid compilation errors when fbsplash support is
   disabled. */
#if !defined(CONFIG_FBSPLASH)
	#define FB_SPLASH_IO_ORIG_USER		0
	#define FB_SPLASH_IO_ORIG_KERNEL	1
#else
	extern int fd_fbsplash;
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

/* Useful macros */
#define min(a,b)		((a) < (b) ? (a) : (b))
#define max(a,b)		((a) > (b) ? (a) : (b))

#define iprint(type, args...) do {				\
	if (config.verbosity == VERB_QUIET)			\
		break;									\
												\
	if (type <= MSG_ERROR) {					\
		fprintf(stderr, ## args);				\
	} else if (config.verbosity == VERB_HIGH) {	\
		fprintf(stdout, ## args);				\
	}											\
} while (0);

#define CLAMP(x)		((x) > 255 ? 255 : (x))
#define DEBUG(x...)

#define WANT_TTF	((defined(CONFIG_TTF_KERNEL) && defined(TARGET_KERNEL)) || (defined(CONFIG_TTF) && !defined(TARGET_KERNEL)))

/* ************************************************************************
 *				Lists
 * ************************************************************************ */
typedef struct item {
	void *p;
	struct item *next;
} item;

typedef struct {
	item *head, *tail;
} list;

#define list_init(list)		{ list.head = list.tail = NULL; }

/* ************************************************************************
 *				Enums
 * ************************************************************************ */

enum TASK { getres, setpic, init, on, off, setcfg, getcfg, getstate, none, paint,
	    setmode, getmode, repaint, start_daemon };
enum ESVC { e_display, e_svc_inact_start, e_svc_inact_stop, e_svc_start,
	    e_svc_started, e_svc_stop, e_svc_stopped, e_svc_stop_failed,
	    e_svc_start_failed };

/* ************************************************************************
 *				Structures
 * ************************************************************************ */
enum otype { o_box, o_icon, o_text, o_anim };
typedef struct {
	char *filename;
	unsigned int w, h;
	u8 *picbuf;
} icon_img;

typedef struct {
	int x1, x2, y1, y2;
} rect;

typedef struct {
	int x, y;
	icon_img *img;
	char *svc;
	enum ESVC type;
	u8 status;
	u8 crop;
	rect crop_from, crop_to;
} icon;

typedef struct {
	int id;					/* monotonically increased ID */
	enum otype type;		/* object type */
	void *p;				/* pointer to the type-specific data */
	rect bnd;				/* bounding rectangle */
	u8 *bgcache;			/* bgnd cache, directly from the bg picture if NULL */
} obj;

typedef struct color {
	u8 r, g, b, a;
} __attribute__ ((packed)) color;

struct colorf {
	float r, g, b, a;
};

#if WANT_TTF
#define F_TXT_SILENT  	1
#define F_TXT_VERBOSE	2
#define F_TXT_EXEC 	4
#define F_TXT_EVAL	8	

#define F_HS_HORIZ_MASK	7
#define F_HS_VERT_MASK	56

#define F_HS_TOP	8
#define F_HS_VMIDDLE	16
#define F_HS_BOTTOM	32

#define F_HS_LEFT	1
#define F_HS_HMIDDLE	2
#define F_HS_RIGHT	4

#include "ttf.h"
typedef struct {
	char *file;
	int size;
	TTF_Font *font;
} font_e;

typedef struct {
	int x, y, last_width;
	u8 hotspot;
	color col;
	u8 flags;
	u8 style;
	char *val;
	font_e *font;
} text;

void text_get_xy(text *ct, int *x, int *y);

#endif /* TTF */

#define MODE_VERBOSE 0x01
#define MODE_SILENT  0x02

typedef struct stheme {
	u8 bg_color;
	u16 tx;
	u16 ty;
	u16 tw;
	u16 th;
	u16 text_x, text_y;
	u16 text_size;
	color text_color;
	char *text_font;

	u8 modes;				/* bitmask of supported splash modes */

	char *silentpic;		/* background pictures */
	char *pic;
	char *silentpic256;		/* pictures for 8bpp modes */
	char *pic256;

	/* Loaded background images */
	struct fb_image verbose_img;
	struct fb_image silent_img;

	u8 *bgbuf;				/* background buffer */

	/* A list of all objects used in the theme config file. */
	list objs;

	/* A list of anims used in the theme config file.  Anims from this
	 * list are also members of the objs list. */
	list anims;

	/* A list of icon_img's. */
	list icons;

	/* A list of fonts used in the theme. */
	list fonts;

	/* A list of rectangles. Rectangles are not on the objs list as they
	 * only describe a special area on the screen and are not renderable. */
	list rects;

#if WANT_TTF
	TTF_Font *main_font;
#endif

	int xres;		/* Resolution for which this theme has been designed. */
	int yres;
	int xmarg;		/* Margins. Non-zero only if using a config file
					 * designed for a different resolution than the one
					 * currently in use. */
	int ymarg;
} stheme_t;

#if defined(CONFIG_MNG) && !defined(TARGET_KERNEL)
#include "mng_splash.h"

#if 0
#define F_ANIM_SILENT		1
#define F_ANIM_VERBOSE		2
#endif

#define F_ANIM_METHOD_MASK	12
#define F_ANIM_ONCE			0
#define F_ANIM_LOOP			4
#define F_ANIM_PROPORTIONAL	8
#define F_ANIM_DISPLAY		16

#define F_ANIM_STATUS_DONE 1

typedef struct {
	int x, y, w, h;
	mng_handle mng;
	char *svc;
	enum ESVC type;
	u8 status;
	u8 flags;
} anim;
#endif	/* CONFIG_MNG */

typedef struct box {
	int x1, x2, y1, y2;
	struct color c_ul, c_ur, c_ll, c_lr;	/* upper left, upper right,
											   lower left, lower right */
	u8 attr;
	struct box *curr;						/* current interpolated box */
} box;

typedef struct {
	u8 r, g, b, a;
} __attribute__ ((packed)) rgbacolor;

typedef struct {
	u8 r, g, b;
} __attribute__ ((packed)) rgbcolor;

#define BOX_NOOVER 0x01
#define BOX_INTER 0x02
#define BOX_SILENT 0x04

struct fb_data {
	struct fb_var_screeninfo   var;
	struct fb_fix_screeninfo   fix;
	int bytespp;			/* bytes per pixel */
	bool opt;				/* can we use optimized 24/32bpp routines? */
	u8 ro, go, bo;			/* red, green, blue offset */
	u8 rlen, glen, blen;	/* red, green, blue length */
};


/* ************************************************************************
 *				Functions
 * ************************************************************************ */

/* common.c */
int fb_get_settings(int);
int do_getpic(unsigned char, unsigned char, char);
int cfg_check_sanity(stheme_t *theme, u8 mode);

int dev_create(char *fn, char *sys);
int fb_open(int fb, bool create);
void fb_unmap(u8 *fb);
void* fb_mmap(int fb);

int tty_open(int tty);

/* parse.c */
#define obj_alloc(__type) ({							\
	u8* __mptr = malloc(sizeof(obj) + sizeof(__type));	\
	(__mptr) ? ({										\
		((obj*)__mptr)->p = __mptr + sizeof(obj);		\
		((obj*)__mptr)->type = o_##__type;				\
		(__type*)(__mptr + sizeof(obj));				\
		}) : NULL; })

#define container_of(x) (obj*)((u8*)(x) - sizeof(obj))


int parse_svc_state(char *t, enum ESVC *state);
int parse_cfg(char *cfgfile, stheme_t *st);

/* render.c */
void rgba2fb(rgbacolor* data, u8 *bg, u8* out, int len, int y, u8 alpha);
void put_pixel(u8 a, u8 r, u8 g, u8 b, u8 *src, u8 *dst, u8 add);
void render_objs(stheme_t *theme, u8 *target, char mode, unsigned char origin);
void prep_bgnds(stheme_t *theme, u8 *target, u8 *bgnd, char mode);

/* image.c */
int load_images(stheme_t *theme, char mode);

/* fbsplash.c */
int fbsplash_open(bool create);
int fbsplash_setstate(unsigned char origin, int vc, unsigned int state);
int fbsplash_getstate(unsigned char origin, int vc);
int fbsplash_setpic(unsigned char origin, int vc, stheme_t *theme);
int fbsplash_setcfg(unsigned char origin, int vc, stheme_t *theme);
int fbsplash_getcfg(int vc);

/* daemon.c */
void daemon_start();

/* list.c */
void list_add(list *l, void *obj);
void list_free(list l, bool free_item);

/* effects.c */
void paint_rect(stheme_t *theme, u8 *dst, u8 *src, int x1, int y1, int x2, int y2);
void put_img(stheme_t *theme, u8 *dst, u8 *src);
void paint_img(stheme_t *theme, u8 *dst, u8 *src);
void fade(stheme_t *theme, u8 *dst, u8 *image, struct fb_cmap cmap, u8 bgnd, int fd, char type);
void set_directcolor_cmap(int fd);


/* tiles.c */
void obj_free(obj *o);

/* common.c */

extern enum TASK arg_task;
extern int arg_fb;
extern char *arg_pidfile;

extern int fd_fb;
extern int fd_tty0;
extern int fd_fbsplash;
extern sendian_t endianess;
extern scfg_t config;

/* libsplashrender */
extern int fd_tty[MAX_NR_CONSOLES];
extern struct fb_data fbd;

#endif /* __UTIL_H */
