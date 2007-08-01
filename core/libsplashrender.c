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
#include <termios.h>

#if !defined(MNT_DETACH)
	#define MNT_DETACH 2
#endif

#include "util.h"
#include "splash.h"

/* If we're not a Gentoo system, define eerror() and ewarn() */
#if !defined(CONFIG_GENTOO) || defined(TARGET_UTIL) || defined(TARGET_KERNEL)
	#if !defined(eerror)
		#define eerror(args...)		fprintf(stderr, ## args); fprintf(stdout, "\n");
	#endif
	#if !defined(ewarn)
		#define ewarn(args...)		fprintf(stdout, ## args); fprintf(stdout, "\n");
	#endif
#else
	#include <einfo.h>
#endif

static int fd_fb0 = -1;
static int fb = -1;
int fd_fb = -1;
u8 *fb_mem = NULL;
int fd_tty[MAX_NR_CONSOLES];
struct fb_data fbd;

#ifdef CONFIG_FBSPLASH
int fd_fbsplash = -1;
#endif

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

int splashr_init(bool create)
{
#ifdef CONFIG_FBSPLASH
	fd_fbsplash = fbsplash_open(create);
#endif

	memset(fd_tty, -1, sizeof(fd_tty));

	fd_fb0 = fb_open(0, create);
	if (fd_fb0 < 0)
		return -1;

	if (fb_init(config.tty_s, create))
		return -2;

#ifdef WANT_TTF
	if (TTF_Init() < 0) {
		eerror("Couldn't initialize TTF.");
		return -3;
	}
#endif
	return 0;
}

void splashr_cleanup()
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

#ifdef CONFIG_FBSPLASH
	if (fd_fbsplash != -1) {
		close(fd_fbsplash);
		fd_fbsplash = -1;
	}
#endif
}

int splashr_render_buf(stheme_t *theme, void *buffer, bool repaint, char mode)
{
	u8 *img;

	/* FIXME: 8bpp modes aren't supported yet */
	if (fbd.var.bits_per_pixel == 8)
		return 0;

	if (mode == 'v') {
		if (!(theme->modes & MODE_VERBOSE))
			return -1;
		img = (u8*) theme->verbose_img.data;
	} else {
		if (!(theme->modes & MODE_SILENT))
			return -1;
		img = (u8*) theme->silent_img.data;
	}

	if (repaint) {
		memcpy(buffer, img, theme->xres * theme->yres * fbd.bytespp);
		invalidate_all(theme);
	}

	render_objs(theme, buffer, (mode == 'v') ? MODE_VERBOSE : MODE_SILENT, repaint);
	return 0;
}

int splashr_render_screen(stheme_t *theme, bool repaint, bool bgnd, char mode, char effects)
{
	if (!splashr_render_buf(theme, theme->bgbuf, repaint, mode)) {
		if (repaint) {
			if (effects & EFF_FADEIN) {
				fade(theme, fb_mem, theme->bgbuf, theme->silent_img.cmap, bgnd ? 1 : 0, fd_fb, 0);
			} else if (effects & EFF_FADEOUT) {
				fade(theme, fb_mem, theme->bgbuf, theme->silent_img.cmap, bgnd ? 1 : 0, fd_fb, 1);
			} else {
				if (fbd.fix.visual == FB_VISUAL_DIRECTCOLOR)
					set_directcolor_cmap(fd_fb);

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

stheme_t *splashr_theme_load()
{
	char buf[512];
	stheme_t *st;

	if (!config.theme)
		return NULL;

	st = calloc(1, sizeof(stheme_t));
	if (!st)
		return NULL;

	st->modes = MODE_SILENT | MODE_VERBOSE;
	st->xres = fbd.var.xres;
	st->yres = fbd.var.yres;

	splash_get_res(config.theme, &st->xres, &st->yres);
	snprintf(buf, 512, THEME_DIR "/%s/%dx%d.cfg", config.theme, st->xres, st->yres);

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
	if ((config.reqmode == 'v' || config.reqmode == 's') &&
		cfg_check_sanity(st, 'v'))
		st->modes &= ~MODE_VERBOSE;
	else
		load_images(st, 'v');

	if ((config.reqmode == 's' || config.reqmode == 't') &&
		cfg_check_sanity(st, 's'))
		st->modes &= ~MODE_SILENT;
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

void splashr_theme_free(stheme_t *theme)
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

int splashr_tty_silent_init()
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

	return 0;
}

int splashr_tty_silent_cleanup()
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

int splashr_tty_silent_set(int tty)
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

void splashr_message_set(stheme_t *theme, char *msg)
{
	obj *o;
	text *t;

	splash_message_set(msg);

	o = theme->objs.tail->p;
	t = o->p;

	/* FIXME: invalidate the main message, reinit the bg cache */
	o->invalid = true;
	t->val = config.message;
}

void splashr_progress_set(stheme_t *theme, int progress)
{
	if (progress < 0)
		progress = 0;

	if (progress > PROGRESS_MAX)
		progress = PROGRESS_MAX;

	config.progress = progress;
	invalidate_progress(theme);
}
