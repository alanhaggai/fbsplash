#include "config.h"
#include <linux/types.h>

/* Adjustable settings */
#define MAX_RECTS 	32
#define MAX_BOXES 	256
#define MAX_ICONS 	512
#define SPLASH_DEV	"/dev/fbsplash"

/* Settings that shouldn't be changed */
#define PROGRESS_MAX 	0xffff

#define u8 __u8
#define u16 __u16
#define u32 __u32

#define printerr(args...)	fprintf(stderr, ## args);
#define min(a,b)		((a) < (b) ? (a) : (b))
#define CLAMP(x) 		((x) > 255 ? 255 : (x))
#define DEBUG(x...)

/* ************************************************************************
 * 				Structures 
 * ************************************************************************ */
struct splash_icon {
	int x, y;
	char *filename;
	char *svc;
	enum { e_display, e_svc_inact_start, e_svc_inact_stop, e_svc_start, 
		e_svc_started, e_svc_stop, e_svc_stopped, e_svc_stop_failed, 
		e_svc_start_failed 
	     } type;
};

struct color {
	u8 r, g, b, a;
} __attribute__ ((packed));

struct colorf {
	double r, g, b, a;
};

struct rect {
	int x1, x2, y1, y2;
};

struct splash_box {
	int x1, x2, y1, y2;
	struct color c_ul, c_ur, c_ll, c_lr; 	/* upper left, upper right, 
						   lower left, lower right */
	u8 attr;
};

#define BOX_NOOVER 0x01
#define BOX_INTER 0x02
#define BOX_SILENT 0x04

struct splash_config {
	u8 bg_color;
	u16 tx;
	u16 ty;
	u16 tw;
	u16 th;
} __attribute__ ((packed));

enum ENDIANESS { little, big };
enum TASK { setpic, init, on, off, setcfg, getcfg, getstate, none, paint, 
	    setmode, getmode, repaint, start_daemon };

/* ************************************************************************
 * 				Functions 
 * ************************************************************************ */

/* common.c */
void detect_endianess(void);
int get_fb_settings(int fb_num);
char *get_cfg_file(char *theme);
int do_getpic(unsigned char, unsigned char, char);
int do_config(unsigned char);
char *get_filepath(char *path);

/* parse.c */
int parse_cfg(char *cfgfile);

/* dev.c */
int create_dev(char *fn, char *sys, int flag);
int remove_dev(char *fn, int flag);

#define open_cr(fd, dev, sysfs, outlabel, flag)	\
	create_dev(dev, sysfs, flag);		\
	fd = open(dev, O_WRONLY);		\
	if (fd == -1) {				\
		remove_dev(dev, flag);		\
		goto outlabel;			\
	}					

#define close_del(fd, dev, flag)		\
	close(fd);				\
	remove_dev(dev, flag);

/* render.c */
void draw_boxes(u8 *data, char, unsigned char);
void draw_icons(u8 *data);

#ifdef CONFIG_PNG
int draw_icon(struct splash_icon ic, u8 *data);
int load_png(char *filename, struct fb_image *img, char mode);
int is_png(char *filename);
#endif

int decompress_jpeg(char *filename, struct fb_image *img);

/* cmd.c */
void cmd_setstate(unsigned int state, unsigned char origin);
void cmd_setpic(struct fb_image *img,unsigned char origin);
void cmd_setcfg(unsigned char origin);
void cmd_getcfg();

/* daemon.c */
void daemon_start();



extern char *cf_pic;
extern char *cf_silentpic;
extern char *cf_pic256;
extern char *cf_silentpic256;

extern struct fb_var_screeninfo   fb_var;
extern struct fb_fix_screeninfo   fb_fix;

extern enum ENDIANESS endianess;
extern enum TASK arg_task;
extern int arg_fb;
extern int arg_vc;
extern char *arg_theme;
extern char arg_mode;
extern u16 arg_progress;

extern char *config_file;

extern struct fb_image pic;
extern char *pic_file;

extern struct splash_box cf_boxes[MAX_BOXES];
extern int cf_boxes_cnt;

extern struct splash_icon cf_icons[MAX_ICONS];
extern int cf_icons_cnt;

extern struct rect cf_rects[MAX_RECTS];
extern int cf_rects_cnt;

extern struct splash_config cf;

