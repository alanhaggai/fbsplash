#ifndef __RENDER_H
#define __RENDER_H

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
	bool crop;
	rect crop_from, crop_to, crop_curr;
} icon;

typedef struct {
	int id;					/* monotonically increased ID */
	enum otype type;		/* object type */
	void *p;				/* pointer to the type-specific data */
	rect bnd;				/* bounding rectangle */
	u8 *bgcache;			/* bgnd cache, directly from the bg picture if NULL */
	u8 modes;
	bool invalid;			/* indicates whether this object has to be repainted */
	bool visible;			/* is this object to be rendered? */
	u8 opacity;				/* global object opacity */
	u16 op_tstep;			/* opacity time step in ms */
	short op_step;			/* opacity step */
	short wait_msecs;		/* time to wait till the next step */
	u16 blendin;			/* blend-in time in ms, 0 if disabled */
	u16 blendout;			/* blend-out time in ms, 0 if disabled */
} obj;

typedef struct {
	rect re;
	obj *o;
} orect;

typedef struct color {
	u8 r, g, b, a;
} __attribute__ ((packed)) color;

struct colorf {
	float r, g, b, a;
};

#if WANT_TTF
#define F_TXT_EXEC		1
#define F_TXT_EVAL		2
#define F_TXT_MSGLOG	4

#define F_HS_HORIZ_MASK	7
#define F_HS_VERT_MASK	56

#define F_HS_TOP		8
#define F_HS_VMIDDLE	16
#define F_HS_BOTTOM		32

#define F_HS_LEFT		1
#define F_HS_HMIDDLE	2
#define F_HS_RIGHT		4

#include "ttf.h"
typedef struct {
	char *file;
	int size;
	TTF_Font *font;
} font_e;

typedef struct text {
	int x, y, last_width;
	u8 hotspot;
	color col;
	u8 flags;
	u8 style;
	char *val;
	u16 *cache;
	font_e *font;
	int curr_progress;		/* if this string uses the $progress variable,
							 * curr_progress holds its currently used value
							 * (i.e. the value for which the text has been
							 * rendered), otherwise = -1 */
	int log_last;
} text;

#endif /* TTF */

typedef struct fbspl_theme {
	u8 bg_color;
	u16 tx;
	u16 ty;
	u16 tw;
	u16 th;

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

	/* A list of objects forming a textbox.  Objects from this list
	 * are also memebers of the objs list. */
	list textbox;

	/* A list of objects with currently active special effects. */
	list fxobjs;

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

	int xres;		/* Resolution for which this theme has been designed. */
	int yres;
	int xmarg;		/* Margins. Non-zero only if using a config file
					 * designed for a different resolution than the one
					 * currently in use. */
	int ymarg;

	int log_cnt;
	int log_lines, log_cols;
	list msglog;

	list blit;		/* List of rectangular regions (rect's) that need to be re-blit to the
					   screen. */
	list render;	/* List of rectangular regions (orect's) that need to be re-rendered
					   to update the screen image. */
} stheme_t;

#if defined(CONFIG_MNG) && !defined(TARGET_KERNEL)
#include "mng_splash.h"

#define F_ANIM_METHOD_MASK	12
#define F_ANIM_ONCE			0
#define F_ANIM_LOOP			4
#define F_ANIM_PROPORTIONAL	8

#define F_ANIM_STATUS_DONE 1

typedef struct anim {
	int x, y, w, h;
	mng_handle mng;
	char *svc;
	char *filename;
	enum ESVC type;
	int curr_progress;
	u8 status;
	u8 flags;
} anim;
#endif	/* CONFIG_MNG */

typedef struct box {
	rect re;
	struct color c_ul, c_ur, c_ll, c_lr;	/* upper left, upper right,
											   lower left, lower right */
	u8 attr;
	struct box *curr;						/* current interpolated box */
	struct box *inter;
} box;

typedef struct {
	u8 r, g, b, a;
} __attribute__ ((packed)) rgbacolor;

typedef struct {
	u8 r, g, b;
} __attribute__ ((packed)) rgbcolor;

#define BOX_NOOVER 0x01
#define BOX_INTER 0x02

/* internal attributes */
#define BOX_SOLID  0x80
#define BOX_VGRAD  0x40
#define BOX_HGRAD  0x20

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
#define obj_alloc(__type, __mode) ({					\
	u8* __mptr = calloc(1, sizeof(obj) + sizeof(__type));	\
	(__mptr) ? ({										\
		((obj*)__mptr)->p = __mptr + sizeof(obj);		\
		((obj*)__mptr)->modes = __mode;					\
		((obj*)__mptr)->visible = true;					\
		((obj*)__mptr)->opacity = 0xff;					\
		((obj*)__mptr)->invalid = true;					\
		((obj*)__mptr)->type = o_##__type;				\
		(__type*)(__mptr + sizeof(obj));				\
		}) : NULL; })

#define container_of(x) (obj*)((u8*)(x) - sizeof(obj))

#define obj_setmode(__obj, __mode) { ((obj*)__obj)->modes = __mode; }

int parse_svc_state(char *t, enum ESVC *state);
int parse_cfg(char *cfgfile, stheme_t *st);

/* render.c */
void rgba2fb(rgbacolor* data, u8 *bg, u8* out, int len, int y, u8 alpha, u8 opacity);
void put_pixel(u8 a, u8 r, u8 g, u8 b, u8 *src, u8 *dst, u8 add);
void invalidate_all(stheme_t *theme);
void invalidate_service(stheme_t *theme, char *svc, enum ESVC state);
void invalidate_progress(stheme_t *theme);
void invalidate_textbox(stheme_t *theme, bool active);
void rect_interpolate(rect *a, rect *b, rect *c);
bool rect_intersect(rect *a, rect *b);
void rect_sanitize(stheme_t *theme, rect *re);
void box_interpolate(box *a, box *b, box *c);
void render_objs(stheme_t *theme, u8 *target, u8 mode, bool force);
void bnd_init(stheme_t *theme);
void blit_add(stheme_t *theme, rect *a);
void render_add(stheme_t *theme, obj *o, rect *a);

/* image.c */
int load_images(stheme_t *theme, char mode);

/* fbcon_decor.c */
int fbcon_decor_open(bool create);
int fbcon_decor_setstate(unsigned char origin, int vc, unsigned int state);
int fbcon_decor_getstate(unsigned char origin, int vc);
int fbcon_decor_setpic(unsigned char origin, int vc, stheme_t *theme);
int fbcon_decor_setcfg(unsigned char origin, int vc, stheme_t *theme);
int fbcon_decor_getcfg(int vc);

/* daemon.c */
void daemon_start();

/* list.c */
void list_add(list *l, void *obj);
void list_free(list l, bool free_item);
void list_del(list *l, item *prev, item *curr);

/* effects.c */
void paint_rect(stheme_t *theme, u8 *dst, u8 *src, int x1, int y1, int x2, int y2);
void put_img(stheme_t *theme, u8 *dst, u8 *src);
void paint_img(stheme_t *theme, u8 *dst, u8 *src);
void fade(stheme_t *theme, u8 *dst, u8 *image, struct fb_cmap cmap, u8 bgnd, int fd, char type);
void set_directcolor_cmap(int fd);

/* common.c */

extern char *arg_pidfile;

extern int fd_fb;
extern int fd_tty0;
extern int fd_fbcondecor;
extern sendian_t endianess;
extern fbspl_cfg_t config;

/* libsplashrender */
extern int fd_tty[MAX_NR_CONSOLES];
extern struct fb_data fbd;



#endif /* __RENDER_H */
