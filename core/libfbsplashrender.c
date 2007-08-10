/*
 * libsplashrender.c - A library of useful splash rendering routines to be used
 *                     in programs implementing support for the splash theme files.
 *
 * Copyright (c) 2007, Michal Januszewski <spock@gentoo.org>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2.  See the file COPYING in the main directory of this archive for
 * more details.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <termios.h>

#include "util.h"

static int fd_console = -1;
static int fd_fb0 = -1;
static int fb = -1;
int fd_fb = -1;
u8 *fb_mem = NULL;
int fd_tty[MAX_NR_CONSOLES];
struct fb_data fbd;

static void obj_free(obj *o)
{
	if (!o)
		return;

	if (o->bgcache)
		free(o->bgcache);

	if (o->type == o_box) {
		box *b = o->p;
		if (b->inter)
			free(container_of(b->inter));
		if (b->curr)
			free(b->curr);
	}

	free(o);
	return;
}

static int fb_init(int tty, bool create)
{
	struct fb_con2fbmap con2fb;

	con2fb.console = config.tty_s;
	con2fb.framebuffer = 0;

	ioctl(fd_fb0, FBIOGET_CON2FBMAP, &con2fb);

	if (con2fb.framebuffer == fb)
		return 0;

	fb = con2fb.framebuffer;
	if (fd_fb != -1) {
		close(fd_fb);
		fd_fb = -1;
	}

	fd_fb = fb_open(fb, create);
	if (fd_fb < 0)
		return -1;

	if (fb_get_settings(fd_fb))
		return -1;

	if (fb_mem) {
		fb_unmap(fb_mem);
		fb_mem = NULL;
	}

	fb_mem = fb_mmap(fd_fb);
	if (!fb_mem)
		return -1;

	return 0;
}

static void fb_cmap_directcolor_set(int fd)
{
	int len, i;
	struct fb_cmap cmap;

	len = min(min(fbd.var.red.length, fbd.var.green.length), fbd.var.blue.length);

	cmap.start = 0;
	cmap.len = (1 << len);
	cmap.transp = NULL;
	cmap.red = malloc(2 * 256 * 3);
	if (!cmap.red)
		return;

	cmap.green = cmap.red + 256;
	cmap.blue = cmap.green + 256;

	for (i = 0; i < cmap.len; i++) {
		cmap.red[i] = cmap.green[i] = cmap.blue[i] = (0xffff * i)/(cmap.len-1);
	}

	ioctl(fd, FBIOPUTCMAP, &cmap);
	free(cmap.red);
}

/**
 * Init the splashrender library.
 *
 * @param create Create device nodes if they're missing.
 */
int fbsplashr_init(bool create)
{
	memset(fd_tty, -1, sizeof(fd_tty));

	fd_fb0 = fb_open(0, create);
	if (fd_fb0 < 0)
		return -1;

	if (fb_init(config.tty_s, create))
		return -2;

#if WANT_TTF
	if (TTF_Init() < 0) {
		iprint(MSG_ERROR, "Couldn't initialize TTF.\n");
		return -3;
	}
#endif
	return 0;
}

/**
 * Initialize the splash input system.
 *
 * @return 0 on success, a negative value otherwise.
 */
int fbsplashr_input_init()
{
	int err;

	fd_console = open("/dev/console", O_RDWR);
	if (fd_console == -1)
		return -1;

	err = fcntl(fd_console, F_SETOWN, getpid());
	if (err == -1) {
		close(fd_console);
		return err;
	}

	return 0;
}

/**
 * Clean up after fbsplashr_input_init().
 */
void fbsplashr_input_cleanup()
{
	if (fd_console != -1)
		close(fd_console);

	fd_console = -1;

	return;
}

/**
 * Wait for a keypress or check whether a key has been pressed.
 *
 * @param block If true, this function will block.
 */
unsigned char fbsplashr_input_getkey(bool block)
{
	unsigned char a;
	int err;
	int flags;

	if (fd_console == -1)
		return 0;

	if (block) {
		flags = O_RDWR;
	} else {
		flags = O_RDWR | O_NONBLOCK;
	}

	err = fcntl(fd_console, F_SETFL, flags);
	if (err == -1)
		return 0;

	if (read(fd_console, &a, 1) <= 0)
		return 0;

	return a;
}

/**
 * Cleanup after fbsplashr_init() and subsequent calls to any
 * libsplashrender routines.
 */
void fbsplashr_cleanup()
{
	int i;

#if WANT_TTF
	TTF_Quit();
#endif

	fb_unmap(fb_mem);

	for (i = 0; i < MAX_NR_CONSOLES; i++) {
		if (fd_tty[i] != -1) {
			close(fd_tty[i]);
			fd_tty[i] = -1;
		}
	}
}

/**
 * Render the splash screen to a buffer.
 *
 * @param theme Theme for which the screen is to be rendered.
 * @param buffer Buffer when the rendered image is to be stored.  The image
 *               will be in the same format as that used by the fb device
 *               which handles the silent splash tty.  The buffer should be
 *               long enough to contain the whole image (xres * yres * bytes per pixel)
 *               where xres and yres are the horizontal and vertical resolution
 *               of the current theme or of the video mode active on the framebuffer
 *               device.
 * @param repaint The whole screen is rendered if true, only updated parts otherwise.
 *
 * @return 0 on success, a negative value otherwise.
 */
int fbsplashr_render_buf(stheme_t *theme, void *buffer, bool repaint)
{
	/* FIXME: 8bpp modes aren't supported yet */
	if (fbd.var.bits_per_pixel == 8)
		return -2;

	if (!(theme->modes & SPL_MODE_SILENT))
		return -1;

	if (repaint) {
		memcpy(buffer, theme->silent_img.data, theme->xres * theme->yres * fbd.bytespp);
		invalidate_all(theme);
	}

	render_objs(theme, buffer, SPL_MODE_SILENT, repaint);
	return 0;
}

/**
 * Render the splash directly to the screen.
 *
 * @param theme Theme for which the screen is to be rendered.
 * @param repaint The whole screeen is rendered if true, only updated parts otherwise.
 * @param bgnd Return immediately if true, wait for all effects to be rendered otherwise.
 *             Effects such as fadein/fadeout take some time to be fully displayed. If this
 *             parameter is set to true, they will be rendered from a separate, forked process.
 * @param effects Indicates which effects are to be used to display the image.  Valid values
 *                constants named prefixed with SPL_EFF_.
 *
 * @return 0 on success, a negative value otherwise.
 */
int fbsplashr_render_screen(stheme_t *theme, bool repaint, bool bgnd, char effects)
{
	if (!fbsplashr_render_buf(theme, theme->bgbuf, repaint)) {
		if (repaint) {
			if (effects & SPL_EFF_FADEIN) {
				fade(theme, fb_mem, theme->bgbuf, theme->silent_img.cmap, bgnd ? 1 : 0, fd_fb, 0);
			} else if (effects & SPL_EFF_FADEOUT) {
				fade(theme, fb_mem, theme->bgbuf, theme->silent_img.cmap, bgnd ? 1 : 0, fd_fb, 1);
			} else {
				if (theme->silent_img.cmap.red)
					ioctl(fd_fb, FBIOPUTCMAP, &theme->silent_img.cmap);

				put_img(theme, fb_mem, theme->bgbuf);
			}
		} else {
			paint_img(theme, fb_mem, theme->bgbuf);
		}
		return 0;
	} else {
		return -1;
	}
}

/**
 * Load a splash theme specified by config.theme.
 *
 * @return A pointer to a theme descriptor, which is then passed to any
 *         libfbsplashrender functions.
 */
stheme_t *fbsplashr_theme_load()
{
	char buf[512];
	stheme_t *st;

	if (!config.theme)
		return NULL;

	st = calloc(1, sizeof(stheme_t));
	if (!st)
		return NULL;

	st->modes = SPL_MODE_SILENT | SPL_MODE_VERBOSE;
	st->xres = fbd.var.xres;
	st->yres = fbd.var.yres;

	fbsplash_get_res(config.theme, &st->xres, &st->yres);
	if (st->xres == 0 || st->yres == 0)
		return NULL;

	snprintf(buf, 512, SPL_THEME_DIR "/%s/%dx%d.cfg", config.theme, st->xres, st->yres);

	st->xmarg = (fbd.var.xres - st->xres) / 2;
	st->ymarg = (fbd.var.yres - st->yres) / 2;

	list_init(st->blit);
	list_init(st->objs);
	list_init(st->anims);
	list_init(st->icons);
	list_init(st->fonts);
	list_init(st->rects);

	/* Parse the config file. It will also load all mng files, so
	 * we don't have to load them explicitly later. */
	parse_cfg(buf, st);

	/* Check for config file sanity for the given splash mode and
	 * load background images and icons. */
	if ((config.reqmode & SPL_MODE_VERBOSE) &&
		cfg_check_sanity(st, 'v'))
		st->modes &= ~SPL_MODE_VERBOSE;
	else
		load_images(st, 'v');

	if ((config.reqmode & SPL_MODE_SILENT) &&
		cfg_check_sanity(st, 's'))
		st->modes &= ~SPL_MODE_SILENT;
	else
		load_images(st, 's');

#if WANT_TTF
	load_fonts(st);
#endif

	invalidate_all(st);
	st->bgbuf = malloc(st->xres * st->yres * fbd.bytespp);

	/* Initialize the bouding rectangles. */
	bnd_init(st);

	return st;
}

/**
 * Free a theme descriptor struct returned by fbsplashr_theme_load().
 *
 * @param theme Theme descriptor to be freed.
 */
void fbsplashr_theme_free(stheme_t *theme)
{
	item *i, *j;

	if (!theme)
		return;

	free(theme->bgbuf);

	for (i = theme->objs.head ; i != NULL; ) {
		obj *o = (obj*)i->p;
		j = i->next;
		obj_free(o);
		free(i);
		i = j;
	}

	for (i = theme->icons.head; i != NULL; ) {
		icon_img *ii = (icon_img*) i->p;
		j = i->next;
		if (ii->filename)
			free(ii->filename);
		if (ii->picbuf)
			free(ii->picbuf);
		free(ii);
		free(i);
		i = j;
	}

	list_free(theme->anims, false);
	list_free(theme->rects, true);

	/* Free background pictures */
	if (theme->verbose_img.data)
		free((u8*)theme->verbose_img.data);
	if (theme->verbose_img.cmap.red)
		free(theme->verbose_img.cmap.red);

	if (theme->silent_img.data)
		free((u8*)theme->silent_img.data);
	if (theme->silent_img.cmap.red)
		free(theme->silent_img.cmap.red);

#if WANT_TTF
	free_fonts(theme);
#endif

	free(theme);
}

static void vt_cursor_disable(int fd)
{
	write(fd, "\e[?25l\e[?1c", 11);
}

static void vt_cursor_enable(int fd)
{
	write(fd, "\e[?25h\e[?0c", 11);
}

/**
 * Prepare the silent tty (config.tty_s) for displaying the silent splash screen.
 */
int fbsplashr_tty_silent_init()
{
	struct termios w;
	int fd;

	if (fd_tty[config.tty_s] == -1)
		fd_tty[config.tty_s] = tty_open(config.tty_s);

	fd = fd_tty[config.tty_s];
	if (!fd)
		return -1;

	tcgetattr(fd, &w);
	w.c_lflag &= ~(ICANON|ECHO);
	w.c_cc[VTIME] = 0;
	w.c_cc[VMIN] = 1;
	tcsetattr(fd, TCSANOW, &w);
	vt_cursor_disable(fd);

	/* Clear display */
	write(fd, "\e[H\e[2J", 7);

	return 0;
}

/**
 * Restore the silent tty to its previous settings after a call to fbsplashr_tty_silent_init().
 */
int fbsplashr_tty_silent_cleanup()
{
	struct termios w;
	int fd;

	if (fd_tty[config.tty_s] == -1)
		fd_tty[config.tty_s] = tty_open(config.tty_s);

	fd = fd_tty[config.tty_s];
	if (!fd)
		return -1;

	tcgetattr(fd, &w);
	w.c_lflag &= (ICANON|ECHO);
	w.c_cc[VTIME] = 0;
	w.c_cc[VMIN] = 1;
	tcsetattr(fd, TCSANOW, &w);
	vt_cursor_enable(fd);

	return 0;
}

/**
 * Set a new silent tty.
 *
 * @param tty The new silent tty.
 */
int fbsplashr_tty_silent_set(int tty)
{
	if (tty < 0 || tty > MAX_NR_CONSOLES)
		return -1;

	if (fd_tty[tty] == -1)
		fd_tty[tty] = tty_open(tty);

	if (fb_init(tty, false))
		return -1;

	config.tty_s = tty;

	return 0;
}

/**
 * Update all internal settings related to the silent tty.
 *
 * To be called when switching to the silent tty.
 *
 * @return 0 if all settings have been updated, 1 if the theme has to be reloaded.
 */
int fbsplashr_tty_silent_update()
{
	struct fb_var_screeninfo old_var;
	struct fb_fix_screeninfo old_fix;
	int ret = 0;

	memcpy(&old_fix, &fbd.fix, sizeof(old_fix));
	memcpy(&old_var, &fbd.var, sizeof(old_var));

	fb_get_settings(fd_fb);

	old_var.yoffset = fbd.var.yoffset;
	old_var.xoffset = fbd.var.xoffset;

	/*
	 * Has the video mode changed? If it has, we'll have to reload
	 * the theme.
	 */
	if (memcmp(&fbd.fix, &old_fix, sizeof(struct fb_fix_screeninfo)) ||
	    memcmp(&fbd.var, &old_var, sizeof(struct fb_var_screeninfo))) {

		munmap(fb_mem, old_fix.line_length * old_var.yres);
		fb_mem = fb_mmap(fd_fb);
		ret = 1;
	}

	/* Update CMAP if we're in a DIRECTCOLOR mode. */
	if (fbd.fix.visual == FB_VISUAL_DIRECTCOLOR)
		fb_cmap_directcolor_set(fd_fb);

	return ret;
}

/**
 * Set a new main message.
 *
 * @param theme Theme descriptor.
 * @param msg The new main message.
 */
void fbsplashr_message_set(stheme_t *theme, const char *msg)
{
	fbsplash_acc_message_set(msg);

#if WANT_TTF
	obj *o;
	text *t;

	o = theme->objs.tail->p;
	t = o->p;

	if (t->val)
		free(t->val);

	o->invalid = true;
	t->val = strdup(config.message);
#endif
}

/**
 * Set a new progress value.
 *
 * @param theme Theme descriptor.
 * @param progress The new progress value.
 */
void fbsplashr_progress_set(stheme_t *theme, int progress)
{
	if (progress < 0)
		progress = 0;

	if (progress > SPL_PROGRESS_MAX)
		progress = SPL_PROGRESS_MAX;

	config.progress = progress;
	invalidate_progress(theme);
}
