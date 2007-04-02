#ifndef __UTIL_H
#define __UTIL_H

#include "config.h"
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <linux/kd.h>
#include <linux/tty.h>
#include <linux/input.h>

#include "splash.h"

/* FIXME:
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
#endif

/* Useful short-named types */
typedef u_int8_t	u8;
typedef u_int16_t	u16;
typedef u_int32_t	u32;
typedef int8_t		s8;
typedef int16_t		s16;
typedef int32_t		s32;

typedef enum endianess { little, big } sendian_t;

extern sendian_t endianess;
extern scfg_t *config;

/* Message levels */
#define MSG_CRITICAL	0
#define MSG_ERROR		1
#define MSG_WARN		2
#define MSG_INFO		3

/* Useful macros */
#define min(a,b)		((a) < (b) ? (a) : (b))
#define max(a,b)		((a) > (b) ? (a) : (b))

#define iprint(type, args...) do {				\
	if (config->verbosity == VERB_QUIET)		\
		break;									\
												\
	if (type <= MSG_ERROR) {					\
		fprintf(stderr, ## args);				\
	} else if (config->verbosity == VERB_HIGH) {	\
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
 * 				Enums
 * ************************************************************************ */

enum TASK { setpic, init, on, off, setcfg, getcfg, getstate, none, paint, 
	    setmode, getmode, repaint, start_daemon };
enum ESVC { e_display, e_svc_inact_start, e_svc_inact_stop, e_svc_start, 
	    e_svc_started, e_svc_stop, e_svc_stopped, e_svc_stop_failed, 
	    e_svc_start_failed };

/* ************************************************************************
 * 				Structures
 * ************************************************************************ */

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

typedef struct obj {
	enum { o_box, o_icon, o_text, o_anim } type;
	void *p;
} obj;

typedef struct color {
	u8 r, g, b, a;
} __attribute__ ((packed)) color;

struct colorf {
	float r, g, b, a;
};

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

typedef struct {
	int x1, x2, y1, y2;
	struct color c_ul, c_ur, c_ll, c_lr; 	/* upper left, upper right,
											   lower left, lower right */
	u8 attr;
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

struct splash_config {
	u8 bg_color;
	u16 tx;
	u16 ty;
	u16 tw;
	u16 th;
	u16 text_x, text_y;
	u16 text_size;
	color text_color;
	char *text_font;
} __attribute__ ((packed));

/* ************************************************************************
 * 				Functions 
 * ************************************************************************ */

/* common.c */
void detect_endianess(sendian_t *end);
int get_fb_settings(int fb_num);
char *get_cfg_file(char *theme);
int do_getpic(unsigned char, unsigned char, char);
int cfg_check_sanity(unsigned char mode);
char *get_filepath(char *path);
int open_fb();
int open_tty(int);
void vt_cursor_enable(int fd);
void vt_cursor_disable(int fd);
int tty_silent_set(int tty, int fd);
int tty_silent_unset(int fd);

/* parse.c */
int parse_cfg(char *cfgfile);
int parse_svc_state(char *t, enum ESVC *state);

/* dev.c */
int create_dev(char *fn, char *sys, int flag);
int remove_dev(char *fn, int flag);

#define open_cr(fd, dev, sysfs, outlabel, flag)	\
	create_dev(dev, sysfs, flag);		\
	fd = open(dev, O_RDWR);				\
	if (fd == -1) {						\
		remove_dev(dev, flag);			\
		goto outlabel;					\
	}

#define close_del(fd, dev, flag)		\
	close(fd);				\
	remove_dev(dev, flag);

/* render.c */
void render_objs(u8 *target, u8 *bgnd, char mode, unsigned char origin);
inline void put_pixel (u8 a, u8 r, u8 g, u8 b, u8 *src, u8 *dst, u8 add);
void rgba2fb (rgbacolor* data, u8* bg, u8* out, int len, int y, u8 alpha);

/* image.c */
int load_images(char mode);

/* cmd.c */
int cmd_setstate(unsigned int state, unsigned char origin);
int cmd_setpic(struct fb_image *img, unsigned char origin);
int cmd_setcfg(unsigned char origin);
int cmd_getcfg();

/* daemon.c */
void daemon_start();
void do_paint(u8 *dst, u8 *src);
void do_repaint(u8 *dst, u8 *src);
	
/* list.c */
void list_add(list *l, void *obj);

/* effects.c */
void put_img(u8 *dst, u8 *src);
void fade_in(u8 *dst, u8 *image, struct fb_cmap cmap, u8 bgnd, int fd);
void set_directcolor_cmap(int fd);

extern char *cf_pic;
extern char *cf_silentpic;
extern char *cf_pic256;
extern char *cf_silentpic256;

/* common.c */
extern struct fb_var_screeninfo   fb_var;
extern struct fb_fix_screeninfo   fb_fix;

extern enum TASK arg_task;
extern int arg_fb;
extern int arg_vc;
extern char *arg_pidfile;
#ifndef TARGET_KERNEL
extern char *arg_export;
extern u8 theme_loaded;
#endif 

extern char *config_file;

extern list icons;
extern list objs;
extern list rects;
extern list fonts;
extern list anims;

extern u8 *bg_buffer;
extern int bytespp;

extern struct fb_image verbose_img;
extern struct fb_image silent_img;

extern struct splash_config cf;

/* common.c */
extern u8 fb_opt;
extern u8 fb_ro, fb_go, fb_bo;
extern u8 fb_rlen, fb_glen, fb_blen;

#endif /* __UTIL_H */
