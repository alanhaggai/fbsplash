/*
 * render.c - Functions for rendering boxes and icons
 *
 * Copyright (C) 2004-2005, Michal Januszewski <spock@gentoo.org>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

/*
 * HACK WARNING:
 * This is necessary to get FD_SET and FD_ZERO on platforms other than x86.
 */

#ifdef TARGET_KERNEL
#define __KERNEL__
#include <linux/posix_types.h>
#undef __KERNEL__
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "util.h"

/* Converts a RGBA/RGB image to whatever format the framebuffer uses */
void rgba2fb (rgbacolor* data, u8 *bg, u8* out, int len, int y, u8 alpha)
{
	int i, add = 0;
	rgbcolor* rgb = (rgbcolor*)data;

	add ^= (0 ^ y) & 1 ? 1 : 3;

	for (i = 0; i < len; i++) {
		if (alpha) {
			put_pixel(data->a, data->r, data->g, data->b, bg, out, add);
			data++;
		} else {
			put_pixel(255, rgb->r, rgb->g, rgb->b, bg, out, add);
			rgb++;
		}

		out += bytespp;
		bg += bytespp;
		add ^= 3;
	}
}

void render_icon(icon *ticon, u8 *target)
{
	int y, yi, xi, wi, hi;
	int bytespp = (fb_var.bits_per_pixel + 7) >> 3;
	u8 *out = NULL;
	u8 *in = NULL;

	int h = PROGRESS_MAX - config->progress;
	rect rct;

	/* Interpolate a cropping rectangle if necessary. */
	if (ticon->crop) {
		rct.x1 = (ticon->crop_from.x1 * h + ticon->crop_to.x1 * config->progress) / PROGRESS_MAX;
		rct.x2 = (ticon->crop_from.x2 * h + ticon->crop_to.x2 * config->progress) / PROGRESS_MAX;
		rct.y1 = (ticon->crop_from.y1 * h + ticon->crop_to.y1 * config->progress) / PROGRESS_MAX;
		rct.y2 = (ticon->crop_from.y2 * h + ticon->crop_to.y2 * config->progress) / PROGRESS_MAX;

		xi = min(rct.x1, rct.x2);
		yi = min(rct.y1, rct.y2);
		wi = abs(rct.x1 - rct.x2) + 1;
		hi = abs(rct.y1 - rct.y2) + yi + 1;
	} else {
		xi = yi = 0;
		wi = ticon->img->w;
		hi = ticon->img->h;
	}

	for (y = ticon->y + yi; yi < hi; yi++, y++) {
		out = target + (ticon->x + xi + y * cf.xres) * bytespp;
		in = ticon->img->picbuf + (xi + yi * ticon->img->w) * 4;
		rgba2fb((rgbacolor*)in, out, out, wi, y, 1);
	}
}

inline void put_pixel (u8 a, u8 r, u8 g, u8 b, u8 *src, u8 *dst, u8 add)
{
	/* Can we use optimized code for 24/32bpp modes? */
	if (fb_opt) {
		if (a == 0) {
			dst[fb_ro] = src[fb_ro];
			dst[fb_go] = src[fb_go];
			dst[fb_bo] = src[fb_bo];
		} else if (a == 255) {
			dst[fb_ro] = r;
			dst[fb_go] = g;
			dst[fb_bo] = b;
		} else {
			dst[fb_ro] = (src[fb_ro]*(255-a) + r*a) / 255;
			dst[fb_go] = (src[fb_go]*(255-a) + g*a) / 255;
			dst[fb_bo] = (src[fb_bo]*(255-a) + b*a) / 255;
		}
	} else {
		u32 i;
		u8 tr, tg, tb;

		if (a != 255) {
			if (fb_var.bits_per_pixel == 16) {
				i = *(u16*)src;
			} else if (fb_var.bits_per_pixel == 24) {
				i = *(u32*)src & 0xffffff;
			} else if (fb_var.bits_per_pixel == 32) {
				i = *(u32*)src;
			} else {
				i = *(u32*)src & ((2 << fb_var.bits_per_pixel)-1);
			}

			tr = (( (i >> fb_var.red.offset & ((1 << fb_rlen)-1))
			      << (8 - fb_rlen)) * (255 - a) + r * a) / 255;
			tg = (( (i >> fb_var.green.offset & ((1 << fb_glen)-1))
			      << (8 - fb_glen)) * (255 - a) + g * a) / 255;
			tb = (( (i >> fb_var.blue.offset & ((1 << fb_blen)-1))
			      << (8 - fb_blen)) * (255 - a) + b * a) / 255;
		} else {
			tr = r;
			tg = g;
			tb = b;
		}

		/* We only need to do dithering if depth is <24bpp */
		if (fb_var.bits_per_pixel < 24) {
			tr = CLAMP(tr + add*2 + 1);
			tg = CLAMP(tg + add);
			tb = CLAMP(tb + add*2 + 1);
		}

		tr >>= (8 - fb_rlen);
		tg >>= (8 - fb_glen);
		tb >>= (8 - fb_blen);

		i = (tr << fb_var.red.offset) |
		    (tg << fb_var.green.offset) |
		    (tb << fb_var.blue.offset);

		if (fb_var.bits_per_pixel == 16) {
			*(u16*)dst = i;
		} else if (fb_var.bits_per_pixel == 24) {
			if (endianess == little) {
				*(u16*)dst = i & 0xffff;
				dst[2] = (i >> 16) & 0xff;
			} else {
				*(u16*)dst = (i >> 8) & 0xffff;
				dst[2] = i & 0xff;
			}
		} else if (fb_var.bits_per_pixel == 32) {
			*(u32*)dst = i;
		}
	}
}

void render_box(box *box, u8 *target)
{
	int x, y, a, r, g, b;
	int add;
	u8 *pic;
	u8 solid = 0;

	int b_width = box->x2 - box->x1 + 1;
	int b_height = box->y2 - box->y1 + 1;

	if (!memcmp(&box->c_ul, &box->c_ur, sizeof(color)) &&
	    !memcmp(&box->c_ul, &box->c_ll, sizeof(color)) &&
	    !memcmp(&box->c_ul, &box->c_lr, sizeof(color))) {
		solid = 1;
	}

	for (y = box->y1; y <= box->y2; y++) {

		int r1, r2, g1, g2, b1, b2, a1, a2;
		int h1, h2, h;
		u8  opt = 0;
		float hr, hg, hb, ha, fr, fg, fb, fa;

		pic = target + (box->x1 + y * cf.xres) * bytespp;

		/* Do a nice 2x2 ordered dithering, like it was done in bootsplash;
		 * this makes the pics in 15/16bpp modes look much nicer;
		 * the produced pattern is:
		 * 303030303..
		 * 121212121..
		 */
		add = (box->x1 & 1);
		add ^= (add ^ y) & 1 ? 1 : 3;

		if (solid) {
			r = box->c_ul.r;
			g = box->c_ul.g;
			b = box->c_ul.b;
			a = box->c_ul.a;
			opt = 1;
		} else {
			h1 = box->y2 - y;
			h2 = y - box->y1;

			if (b_height > 1)
				h = b_height -1;
			else
				h = 1;

			r1 = (h1 * box->c_ul.r + h2 * box->c_ll.r)/h;
			r2 = (h1 * box->c_ur.r + h2 * box->c_lr.r)/h;

			g1 = (h1 * box->c_ul.g + h2 * box->c_ll.g)/h;
			g2 = (h1 * box->c_ur.g + h2 * box->c_lr.g)/h;

			b1 = (h1 * box->c_ul.b + h2 * box->c_ll.b)/h;
			b2 = (h1 * box->c_ur.b + h2 * box->c_lr.b)/h;

			a1 = (h1 * box->c_ul.a + h2 * box->c_ll.a)/h;
			a2 = (h1 * box->c_ur.a + h2 * box->c_lr.a)/h;

			if (r1 == r2 && g1 == g2 && b1 == b2 && a1 == a2) {
				opt = 1;
			} else {
				r2 -= r1;
				g2 -= g1;
				b2 -= b1;
				a2 -= a1;

				hr = 1.0/b_width * r2;
				hg = 1.0/b_width * g2;
				hb = 1.0/b_width * b2;
				ha = 1.0/b_width * a2;
			}

			r = r1; fr = (float)r1;
			g = g1; fg = (float)g1;
			b = b1; fb = (float)b1;
			a = a1; fa = (float)a1;
		}

		for (x = box->x1; x <= box->x2; x++) {
			if (!opt) {
				fa += ha;
				fr += hr;
				fg += hg;
				fa += hb;

				a = (u8)fa;
				b = (u8)fb;
				g = (u8)fg;
				r = (u8)fr;
			}

			put_pixel(a, r, g, b, pic, pic, add);
			pic += bytespp;
			add ^= 3;
		}
	}
}

/* Interpolates two boxes, based on the value of the config->progress variable.
 * This is a strange implementation of a progress bar, introduced by the
 * authors of Bootsplash. */
void interpolate_box(box *a, box *b)
{
	int h = PROGRESS_MAX - config->progress;

	if (config->progress == 0)
		return;

#define inter_color(cl1, cl2)										\
{																	\
	cl1.r = (cl1.r * h + cl2.r * config->progress) / PROGRESS_MAX;	\
	cl1.g = (cl1.g * h + cl2.g * config->progress) / PROGRESS_MAX;	\
	cl1.b = (cl1.b * h + cl2.b * config->progress) / PROGRESS_MAX;	\
	cl1.a = (cl1.a * h + cl2.a * config->progress) / PROGRESS_MAX;	\
}

	a->x1 = (a->x1 * h + b->x1 * config->progress) / PROGRESS_MAX;
	a->x2 = (a->x2 * h + b->x2 * config->progress) / PROGRESS_MAX;
	a->y1 = (a->y1 * h + b->y1 * config->progress) / PROGRESS_MAX;
	a->y2 = (a->y2 * h + b->y2 * config->progress) / PROGRESS_MAX;

	inter_color(a->c_ul, b->c_ul);
	inter_color(a->c_ur, b->c_ur);
	inter_color(a->c_ll, b->c_ll);
	inter_color(a->c_lr, b->c_lr);
}

char *get_program_output(char *prg, unsigned char origin)
{
	char *buf = malloc(1024);
	fd_set rfds;
	struct timeval tv;
	int pfds[2];
	pid_t pid;
	int i;

	if (!buf)
		return NULL;

	pipe(pfds);
	pid = fork();
	buf[0] = 0;

	if (pid == 0) {
		if (origin != FB_SPLASH_IO_ORIG_KERNEL) {
			/* Only play with stdout if we are NOT the kernel helper.
			 * Otherwise, things will break horribly and we'll end up
			 * with a deadlock. */
			close(1);
		}
		dup(pfds[1]);
		close(pfds[0]);
		execlp("sh", "sh", "-c", prg, NULL);
	} else {
		FD_ZERO(&rfds);
		FD_SET(pfds[0], &rfds);
		tv.tv_sec = 0;
		tv.tv_usec = 250000;
		i = select(pfds[0]+1, &rfds, NULL, NULL, &tv);
		if (i != -1 && i != 0) {
			i = read(pfds[0], buf, 1024);
			if (i > 0)
				buf[i] = 0;
		}

		close(pfds[0]);
		close(pfds[1]);
	}

	return buf;
}

char *eval_text(char *txt)
{
	char *p, *t, *ret, *d;
	int len, i;

	i = len = strlen(txt);
	p = txt;

	while ((t = strstr(p, "$progress")) != NULL) {
		len += 3;
		p = t+1;
	}

	ret = malloc(len+1);

	p = txt;
	d = ret;

	while (*p != 0) {
		if (*p == '\\') {
			/* to allow literal "$progress" i.e. \$progress */
			p++;

			/* might have reached end of string */
			if (*p == 0)
				break;

			if (*p == 'n')
				*d = '\n';
			else
				*d = *p;
			p++;
			d++;
			continue;
		}

		*d = *p;

		if (!strncmp(p, "$progress", 9)) {
			d += sprintf(d, "%d", config->progress * 100 / PROGRESS_MAX);
			p += 9;
		} else {
			p++;
			d++;
		}
	}

	*d = 0; /* NULL-terminate */

	return ret;
}

void prep_bgnd(u8 *target, u8 *src, int x, int y, int w, int h)
{
	u8 *t, *s;
	int j, i;

	/* Sanity checks */
	if (!w || !h)
		return;
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (x + w > cf.xres)
		w = cf.xres - x;
	if (y + h > cf.yres)
		h = cf.yres - y;

	t = target + (y * cf.xres + x) * bytespp;
	s = src    + (y * cf.xres + x) * bytespp;
	j = w * bytespp;
	i = cf.xres * bytespp;

	for (y = 0; y < h; y++) {
		memcpy(t, s, j);
		t += i;
		s += i;
	}
}

/* Prepares the backgroud underneath objects that will be rendered in
 * render_objs() */
void prep_bgnds(u8 *target, u8 *bgnd, char mode)
{
	item *i;

	for (i = objs.head; i != NULL; i = i->next) {
		obj *o = (obj*)i->p;

		if (o->type == o_box) {
			box *b, *n;
			b = (box*)o->p;

			if (b->attr & BOX_SILENT && mode != 's')
				continue;

			if (!(b->attr & BOX_SILENT) && mode != 'v')
				continue;

			if ((b->attr & BOX_INTER) && i->next != NULL) {
				if (((obj*)i->next->p)->type == o_box) {
					n = (box*)((obj*)i->next->p)->p;
					prep_bgnd(target, bgnd, n->x1, n->y1, n->x2 - n->x1 + 1, n->y2 - n->y1 + 1);
				}
			}
		} else if (o->type == o_icon && mode == 's') {
			icon *c = (icon*)o->p;

			if (c->status == 0)
				continue;

			if (!c->img || !c->img->picbuf)
				continue;

			if (c->img->w > cf.xres - c->x || c->img->h > cf.yres - c->y)
				continue;

			prep_bgnd(target, bgnd, c->x, c->y, c->img->w, c->img->h);
		}
#if defined(CONFIG_MNG) && !defined(TARGET_KERNEL)
		else if (o->type == o_anim) {
			u8 render_it = 0;
			anim *a = (anim*)o->p;

			/* We only support animations in the silent mode. */
			if (mode != 's')
				continue;

			if ((a->flags & F_ANIM_METHOD_MASK) == F_ANIM_ONCE &&
				(a->status == F_ANIM_STATUS_DONE)) {
				render_it = 1;
			} else if ((a->flags & F_ANIM_METHOD_MASK) == F_ANIM_PROPORTIONAL) {
				render_it = 1;
			}

			if (render_it)
				prep_bgnd(target, bgnd, a->x, a->y, a->w, a->h);
		}
#endif /* CONFIG_MNG */
#if WANT_TTF
		else if (o->type == o_text) {
			text *ct = (text*)o->p;
			int x, y;

			if (mode == 's' && !(ct->flags & F_TXT_SILENT))
				continue;

			if (mode == 'v' && !(ct->flags & F_TXT_VERBOSE))
				continue;

			/* FIXME: this shouldn't even be in the list! */
			if (!ct->font || !ct->font->font)
				continue;

			text_get_xy(ct, &x, &y);
			prep_bgnd(target, bgnd, x, y, ct->last_width, ct->font->font->height);
		}
#endif
	}

#if WANT_TTF
	if (mode == 's') {
		prep_bgnd(target, bgnd, cf.text_x, cf.text_y, boot_msg_width, global_font->height);
	}
#endif
}

void render_objs(u8 *target, u8 *bgnd, char mode, unsigned char origin)
{
	item *i;

	if (fb_var.bits_per_pixel == 8)
		return;

	if (bgnd)
		prep_bgnds(target, bgnd, mode);

	for (i = objs.head; i != NULL; i = i->next) {
		obj *o = (obj*)i->p;

		if (o->type == o_box) {
			box tmp, *b, *n;
			b = (box*)o->p;

			if ((b->attr & BOX_SILENT) && mode != 's')
				continue;

			if (!(b->attr & BOX_SILENT) && mode != 'v')
				continue;

			if ((b->attr & BOX_INTER) && i->next != NULL) {
				if (((obj*)i->next->p)->type == o_box) {
					n = (box*)((obj*)i->next->p)->p;
					tmp = *b;
					interpolate_box(&tmp, n);
					render_box(&tmp, target);
					i = i->next;
				}
			} else {
				render_box(b, target);
			}
		/* Icons are only allowed in silent mode. */
		} else if (o->type == o_icon && mode == 's') {
			icon *c;
			c = (icon*)o->p;

			if (c->status == 0)
				continue;

			if (!c->img || !c->img->picbuf)
				continue;

			if (c->img->w > cf.xres - c->x || c->img->h > cf.yres - c->y) {
				iprint(MSG_WARN,"Icon %s does not fit on the screen - ignoring it.", c->img->filename);
				continue;
			}

			render_icon(c, target);
		}
#if defined(CONFIG_MNG) && !defined(TARGET_KERNEL)
		else if (o->type == o_anim) {
			u8 render_it = 0;
			anim *a = (anim*)o->p;

			/* We only support animations in the silent mode. */
			if (mode != 's')
				continue;

			if ((a->flags & F_ANIM_METHOD_MASK) == F_ANIM_ONCE &&
				(a->status == F_ANIM_STATUS_DONE)) {
				render_it = 1;
			} else if ((a->flags & F_ANIM_METHOD_MASK) == F_ANIM_PROPORTIONAL) {
				int ret = mng_render_proportional(a->mng, config->progress);
				if (ret == MNG_NEEDTIMERWAIT || ret == MNG_NOERROR)
					render_it = 1;
			}

			if (render_it && (a->flags & F_ANIM_DISPLAY)) {
				if (bgnd)
					mng_display_buf(a->mng, bgnd, target, a->x, a->y, cf.xres * bytespp, cf.xres * bytespp);
				else
					mng_display_buf(a->mng, target, target, a->x, a->y, cf.xres * bytespp, cf.xres * bytespp);
			}
		}
#endif /* CONFIG_MNG */
#if WANT_TTF
		else if (o->type == o_text) {

			text *ct = (text*)o->p;
			char *txt;

			if (mode == 's' && !(ct->flags & F_TXT_SILENT))
				continue;

			if (mode == 'v' && !(ct->flags & F_TXT_VERBOSE))
				continue;

			if (!ct->font || !ct->font->font)
				continue;

			txt = ct->val;

			if (ct->flags & F_TXT_EXEC) {
				txt = get_program_output(ct->val, origin);
			}

			if (ct->flags & F_TXT_EVAL) {
				txt = eval_text(txt);
			}

			if (txt) {
				TTF_Render(target, txt, ct->font->font,
				           ct->style, ct->x, ct->y, ct->col,
					   ct->hotspot, &ct->last_width);
				if ((ct->flags & F_TXT_EXEC) || (ct->flags & F_TXT_EVAL))
					free(txt);
			}
		}
#endif /* TTF */
	}

#if WANT_TTF
	if (mode == 's') {
		char *t;
		t = eval_text(config->message);
		TTF_Render(target, t, global_font, TTF_STYLE_NORMAL,
			   cf.text_x, cf.text_y, cf.text_color,
			   F_HS_LEFT | F_HS_TOP, &boot_msg_width);
		free(t);
	}
#endif /* TTF */
}

