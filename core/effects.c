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
void put_img(u8 *dst, u8 *src)
{
	int y, i;
	u8 *to = dst;

	to += cf.xmarg * bytespp + cf.ymarg * fb_fix.line_length;
	i = cf.xres * bytespp;

	for (y = 0; y < cf.yres; y++) {
		memcpy(to, src + i*y, i);
		to += fb_fix.line_length;
	}
}

/*
 * @type = 0 (fadein) or 1 (fadeout)
 */
void fade_directcolor(u8 *dst, u8 *image, int fd, char type)
{
	int len, i, step;
	struct fb_cmap cmap;

	len = min(min(fb_var.red.length,fb_var.green.length),fb_var.blue.length);

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
	put_img(dst, image);

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
void fade_truecolor(u8 *dst, u8 *image, char type)
{
	int rlen, glen, blen;
	int i, step, h, x, y;
	u8 *t, *p, *pic;
	int r, g, b, rt, gt, bt;
	int rl8, gl8, bl8;
	int clut[256][FADEIN_STEPS];

	rlen = fb_var.red.length;
	glen = fb_var.green.length;
	blen = fb_var.blue.length;

	rl8 = 8 - rlen;
	gl8 = 8 - glen;
	bl8 = 8 - blen;

	t = malloc(cf.xres * cf.yres * 3);
	if (!t) {
		put_img(dst, image);
		return;
	}

	pic = image;

	/* Decode the image into a table where each color component
	 * takes exatly one byte */
	for (i = 0; i < cf.xres * cf.yres; i++) {

		if (bytespp == 2) {
			h = *(u16*)pic;
		} else if (bytespp == 3) {
			h = *(u32*)pic & 0xffffff;
		} else if (bytespp == 4) {
			h = *(u32*)pic;
		}

		pic += bytespp;

		r = ((h >> fb_var.red.offset & ((1 << rlen)-1)) << rl8);
		g = ((h >> fb_var.green.offset & ((1 << glen)-1)) << gl8);
		b = ((h >> fb_var.blue.offset & ((1 << blen)-1)) << bl8);

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
		memset(dst, 0, fb_var.yres * fb_fix.line_length);

	for (step = 0; step < FADEIN_STEPS; step++) {

		pic = dst + fb_fix.line_length * cf.ymarg + cf.xmarg * bytespp;
		p = t;

		for (y = 0; y < cf.yres; y++) {

			for (x = 0; x < cf.xres; x++) {

				r = *p; p++;
				g = *p; p++;
				b = *p; p++;

				rt = clut[r][step];
				gt = clut[g][step];
				bt = clut[b][step];

				if (bytespp == 2) {
					rt >>= rl8;
					gt >>= gl8;
					bt >>= bl8;
				}

				h = (rt << fb_var.red.offset) |
				    (gt << fb_var.green.offset) |
				    (bt << fb_var.blue.offset);

				if (bytespp == 2) {
					*(u16*)pic = h;
					pic += 2;
				} else if (bytespp == 3) {
					if (endianess == little) {
						*(u16*)pic = h & 0xffff;
						pic[2] = (h >> 16) & 0xff;
					} else {
						*(u16*)pic = (h >> 8) & 0xffff;
						pic[2] = h & 0xff;
					}
					pic += 3;
				} else if (bytespp == 4) {
					*(u32*)pic = h;
					pic += 4;
				}
			}

			pic += fb_fix.line_length - cf.xres * bytespp;
		}
	}

	free(t);
}

void fade(u8 *dst, u8 *image, struct fb_cmap cmap, u8 bgnd, int fd, char type)
{
	if (bgnd) {
		if (fork())
			return;
	}

	/* FIXME: We need to handle 8bpp modes */
	if (cmap.red) {
		put_img(dst, image);
		return;
	}

	if (fb_fix.visual == FB_VISUAL_DIRECTCOLOR) {
		fade_directcolor(dst, image, fd, type);
	} else {
		fade_truecolor(dst, image, type);
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

	len = min(min(fb_var.red.length,fb_var.green.length),fb_var.blue.length);

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

