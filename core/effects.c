/*
 * effects.c - miscellaneous graphical effects for splashutils
 *
 * Copyright (C) 2004-2005, Michal Januszewski <spock@gentoo.org>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "util.h"

#define FADEIN_STEPS	64
#define FADEIN_STEPS_DC 256

/*
 * Copy the data from the background buffer to the framebuffer.
 * The bg buffer dimensions need not match those of the current
 * video mode.
 */
void put_img(stheme_t *theme, u8 *dst, u8 *src)
{
	int y, i;
	u8 *to = dst;

	to += theme->xmarg * config.fbd->bytespp + theme->ymarg * config.fbd->fix.line_length;
	i = theme->xres * config.fbd->bytespp;

	for (y = 0; y < theme->yres; y++) {
		memcpy(to, src + i*y, i);
		to += config.fbd->fix.line_length;
	}
}

void paint_rect(stheme_t *theme, u8 *dst, u8 *src, int x1, int y1, int x2, int y2)
{
	u8 *to;
	int y, j;

	j = (x2 - x1 + 1) * config.fbd->bytespp;
	for (y = y1; y <= y2; y++) {
		to = dst + (y + theme->ymarg) * config.fbd->fix.line_length + (x1 + theme->xmarg) * config.fbd->bytespp;
		memcpy(to, src + (y * theme->xres + x1) * config.fbd->bytespp, j);
	}
}

void paint_img(stheme_t *theme, u8 *dst, u8 *src)
{
	item *ti;

	for (ti = theme->rects.head ; ti != NULL; ti = ti->next) {
		rect *re;
		re = (rect*)ti->p;
		paint_rect(theme, dst, src, re->x1, re->y1, re->x2, re->y2);
	}

	for (ti = theme->objs.head; ti != NULL; ti = ti->next) {
		obj *o;
		o = (obj*)ti->p;

		switch (o->type) {
		case o_box:
		{
			box *b = (box*)o->p;
			if (!(b->attr & BOX_INTER) || ti->next == NULL)
				continue;

			if (((obj*)ti->next->p)->type != o_box)
				continue;

			b = (box*)((obj*)ti->next->p)->p;
			paint_rect(theme, dst, src, b->x1, b->y1, b->x2, b->y2);
			break;
		}
#if WANT_TTF
		case o_text:
		{
			int x, y;
			text *t = (text*)o->p;
			text_get_xy(t, &x, &y);
			paint_rect(theme, dst, src, x, y, x + t->last_width, y + t->font->font->height);
			break;
		}
#endif
		default:
			continue;
		}
	}

#if WANT_TTF
	paint_rect(theme, dst, src,
			theme->text_x, theme->text_y,
			theme->text_x + boot_msg_width,
			theme->text_y + theme->main_font->height);
#endif
}

/*
 * @type = 0 (fadein) or 1 (fadeout)
 */
void fade_directcolor(stheme_t *theme, u8 *dst, u8 *image, int fd, char type)
{
	int len, i, step;
	struct fb_cmap cmap;

	len = min(min(config.fbd->var.red.length, config.fbd->var.green.length), config.fbd->var.blue.length);

	cmap.start = 0;
	cmap.len = (1 << len);
	cmap.transp = NULL;
	cmap.red = malloc(2 * 256 * 3);
	if (!cmap.red)
		return;

	cmap.green = cmap.red + 256;
	cmap.blue = cmap.green + 256;

	for (i = 0; i < cmap.len; i++) {
		cmap.red[i] = cmap.green[i] = cmap.blue[i] = 0;
	}

	ioctl(fd, FBIOPUTCMAP, &cmap);
	put_img(theme, dst, image);

	for (step = 1; step < FADEIN_STEPS_DC+1; step++) {
		for (i = 0; i < cmap.len; i++) {
			if (type)
				cmap.red[i] = cmap.green[i] = cmap.blue[i] = (0xffff * i * (FADEIN_STEPS_DC+1-step))/
							((cmap.len-1)*FADEIN_STEPS_DC);
			else
				cmap.red[i] = cmap.green[i] = cmap.blue[i] = (0xffff * i * step)/((cmap.len-1)*FADEIN_STEPS_DC);
		}
		ioctl(fd, FBIOPUTCMAP, &cmap);
		usleep(7500);
	}
}

/*
 * @type = 0 (fadein) or 1 (fadeout)
 */
void fade_truecolor(stheme_t *theme, u8 *dst, u8 *image, char type)
{
	int rlen, glen, blen;
	int i, step, h, x, y;
	u8 *t, *p, *pic;
	int r, g, b, rt, gt, bt;
	int rl8, gl8, bl8;
	int clut[256][FADEIN_STEPS];

	rlen = config.fbd->var.red.length;
	glen = config.fbd->var.green.length;
	blen = config.fbd->var.blue.length;

	rl8 = 8 - rlen;
	gl8 = 8 - glen;
	bl8 = 8 - blen;

	t = malloc(theme->xres * theme->yres * 3);
	if (!t) {
		put_img(theme, dst, image);
		return;
	}

	pic = image;

	/* Decode the image into a table where each color component
	 * takes exatly one byte */
	for (i = 0; i < theme->xres * theme->yres; i++) {

		if (config.fbd->bytespp == 2) {
			h = *(u16*)pic;
		} else if (config.fbd->bytespp == 3) {
			h = *(u32*)pic & 0xffffff;
		} else if (config.fbd->bytespp == 4) {
			h = *(u32*)pic;
		}

		pic += config.fbd->bytespp;

		r = ((h >> config.fbd->var.red.offset & ((1 << rlen)-1)) << rl8);
		g = ((h >> config.fbd->var.green.offset & ((1 << glen)-1)) << gl8);
		b = ((h >> config.fbd->var.blue.offset & ((1 << blen)-1)) << bl8);

		t[i*3] = r;
		t[i*3+1] = g;
		t[i*3+2] = b;
	}

	/* Compute the color look-up table */
	for (step = 0; step < FADEIN_STEPS; step++) {
		for (i = 0; i < 256; i++) {
			if (type)
				clut[i][step] = (FADEIN_STEPS-1-step) * i / FADEIN_STEPS;
			else
				clut[i][step] = (step+1) * i / FADEIN_STEPS;
		}
	}

	if (type == 0)
		memset(dst, 0, config.fbd->var.yres * config.fbd->fix.line_length);

	for (step = 0; step < FADEIN_STEPS; step++) {

		pic = dst + config.fbd->fix.line_length * theme->ymarg + theme->xmarg * config.fbd->bytespp;
		p = t;

		for (y = 0; y < theme->yres; y++) {

			for (x = 0; x < theme->xres; x++) {

				r = *p; p++;
				g = *p; p++;
				b = *p; p++;

				rt = clut[r][step];
				gt = clut[g][step];
				bt = clut[b][step];

				if (config.fbd->bytespp == 2) {
					rt >>= rl8;
					gt >>= gl8;
					bt >>= bl8;
				}

				h = (rt << config.fbd->var.red.offset) |
				    (gt << config.fbd->var.green.offset) |
				    (bt << config.fbd->var.blue.offset);

				if (config.fbd->bytespp == 2) {
					*(u16*)pic = h;
					pic += 2;
				} else if (config.fbd->bytespp == 3) {
					if (endianess == little) {
						*(u16*)pic = h & 0xffff;
						pic[2] = (h >> 16) & 0xff;
					} else {
						*(u16*)pic = (h >> 8) & 0xffff;
						pic[2] = h & 0xff;
					}
					pic += 3;
				} else if (config.fbd->bytespp == 4) {
					*(u32*)pic = h;
					pic += 4;
				}
			}

			pic += config.fbd->fix.line_length - theme->xres * config.fbd->bytespp;
		}
	}

	free(t);
}

void fade(stheme_t *theme, u8 *dst, u8 *image, struct fb_cmap cmap, u8 bgnd, int fd, char type)
{
	if (bgnd) {
		if (fork())
			return;
	}

	/* FIXME: We need to handle 8bpp modes */
	if (cmap.red) {
		put_img(theme, dst, image);
		return;
	}

	if (config.fbd->fix.visual == FB_VISUAL_DIRECTCOLOR) {
		fade_directcolor(theme, dst, image, fd, type);
	} else {
		fade_truecolor(theme, dst, image, type);
	}

	if (bgnd) {
		exit(0);
	}

	return;
}

void set_directcolor_cmap(int fd)
{
	int len, i;
	struct fb_cmap cmap;

	len = min(min(config.fbd->var.red.length, config.fbd->var.green.length), config.fbd->var.blue.length);

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

