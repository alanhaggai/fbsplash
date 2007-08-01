/*
 * render.c - Functions for rendering boxes and icons
 *
 * Copyright (C) 2004-2007, Michal Januszewski <spock@gentoo.org>
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

/*
 * Lower level graphics fucntions
 */
inline void put_pixel(u8 a, u8 r, u8 g, u8 b, u8 *src, u8 *dst, u8 add)
{
	/* Can we use optimized code for 24/32bpp modes? */
	if (fbd.opt) {
		if (a == 0) {
			dst[fbd.ro] = src[fbd.ro];
			dst[fbd.go] = src[fbd.go];
			dst[fbd.bo] = src[fbd.bo];
		} else if (a == 255) {
			dst[fbd.ro] = r;
			dst[fbd.go] = g;
			dst[fbd.bo] = b;
		} else {
			dst[fbd.ro] = (src[fbd.ro]*(255-a) + r*a) / 255;
			dst[fbd.go] = (src[fbd.go]*(255-a) + g*a) / 255;
			dst[fbd.bo] = (src[fbd.bo]*(255-a) + b*a) / 255;
		}
	} else {
		u32 i;
		u8 tr, tg, tb;

		if (a != 255) {
			if (fbd.var.bits_per_pixel == 16) {
				i = *(u16*)src;
			} else if (fbd.var.bits_per_pixel == 24) {
				i = *(u32*)src & 0xffffff;
			} else if (fbd.var.bits_per_pixel == 32) {
				i = *(u32*)src;
			} else {
				i = *(u32*)src & ((2 << fbd.var.bits_per_pixel)-1);
			}

			tr = (( (i >> fbd.var.red.offset & ((1 << fbd.rlen)-1))
			      << (8 - fbd.rlen)) * (255 - a) + r * a) / 255;
			tg = (( (i >> fbd.var.green.offset & ((1 << fbd.glen)-1))
			      << (8 - fbd.glen)) * (255 - a) + g * a) / 255;
			tb = (( (i >> fbd.var.blue.offset & ((1 << fbd.blen)-1))
			      << (8 - fbd.blen)) * (255 - a) + b * a) / 255;
		} else {
			tr = r;
			tg = g;
			tb = b;
		}

		/* We only need to do dithering if depth is <24bpp */
		if (fbd.var.bits_per_pixel < 24) {
			tr = CLAMP(tr + add*2 + 1);
			tg = CLAMP(tg + add);
			tb = CLAMP(tb + add*2 + 1);
		}

		tr >>= (8 - fbd.rlen);
		tg >>= (8 - fbd.glen);
		tb >>= (8 - fbd.blen);

		i = (tr << fbd.var.red.offset) |
		    (tg << fbd.var.green.offset) |
		    (tb << fbd.var.blue.offset);

		if (fbd.var.bits_per_pixel == 16) {
			*(u16*)dst = i;
		} else if (fbd.var.bits_per_pixel == 24) {
			if (endianess == little) {
				*(u16*)dst = i & 0xffff;
				dst[2] = (i >> 16) & 0xff;
			} else {
				*(u16*)dst = (i >> 8) & 0xffff;
				dst[2] = i & 0xff;
			}
		} else if (fbd.var.bits_per_pixel == 32) {
			*(u32*)dst = i;
		}
	}
}

/* Converts a RGBA/RGB image to whatever format the framebuffer uses */
void rgba2fb(rgbacolor* data, u8 *bg, u8* out, int len, int y, u8 alpha)
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

		out += fbd.bytespp;
		bg += fbd.bytespp;
		add ^= 3;
	}
}

void blit(u8 *src, rect *bnd, int src_w, u8 *dst, int x, int y, int dst_w)
{
	u8 *to;
	int cy, j;

	to = dst + (y * dst_w + x) * fbd.bytespp;

	j = (bnd->x2 - bnd->x1 + 1) * fbd.bytespp;
	for (cy = bnd->y1; cy <= bnd->y2; cy++) {
		memcpy(to, src + (cy * src_w + bnd->x1) * fbd.bytespp, j);
		to += dst_w * fbd.bytespp;
	}
}

/*
 *  Rectangle-related functions
 */
void rect_interpolate(rect *a, rect *b, rect *c)
{
	int h = PROGRESS_MAX - config.progress;

	c->x1 = (a->x1 * h + b->x1 * config.progress) / PROGRESS_MAX;
	c->x2 = (a->x2 * h + b->x2 * config.progress) / PROGRESS_MAX;
	c->y1 = (a->y1 * h + b->y1 * config.progress) / PROGRESS_MAX;
	c->y2 = (a->y2 * h + b->y2 * config.progress) / PROGRESS_MAX;
}

/*
 * Set 'c' to a rectangle encompassing both 'a' and 'b'.
 */
void rect_bnd(rect *a, rect *b, rect *c)
{
	c->x1 = min(a->x1, b->x1);
	c->x2 = max(a->x2, b->x2);
	c->y1 = min(a->y1, b->y1);
	c->y2 = max(a->y2, b->y2);
}

void rect_min(rect *a, rect *b, rect *c)
{
	c->x1 = max(a->x1, b->x1);
	c->x2 = min(a->x2, b->x2);
	c->y1 = max(a->y1, b->y1);
	c->y2 = min(a->y2, b->y2);
}

bool rect_intersect(rect *a, rect *b)
{
	if (a->x2 < b->x1 || a->x1 > b->x2 || a->y1 > b->y2 || a->y2 < b->y1)
		return false;
	else
		return true;
}

/*
 * Returns true if rect a contains rect b.
 */
bool rect_contains(rect *a, rect *b)
{
	if (a->x1 <= b->x1 && a->y1 <= b->y1 && a->x2 >= b->x2 && a->y2 >= b->y2)
		return true;
	else
		return false;
}

void blit_add(stheme_t *theme, rect *a)
{
	rect *re = malloc(sizeof(rect));
	if (!re)
		return;

	memcpy(re, a, sizeof(rect));
	list_add(&theme->blit, re);
}

void blit_normalize(stheme_t *theme)
{
	item *i, *j, *prev;
	rect *a, *b;

	for (i = theme->blit.head; i; i = i->next) {
start:	a = i->p;
		prev = i;

		for (j = i->next; j; j = j->next) {
			b = j->p;

			if (rect_contains(a, b)) {
				list_del(&theme->blit, prev, j);
				free(b);
				j = prev;
			} else if (rect_contains(b, a)) {
				i->p = b;
				list_del(&theme->blit, prev, j);
				free(a);
				goto start;
			}
			prev = j;
		}
	}
}

void render_add(stheme_t *theme, obj *o, rect *a)
{
//	printf("blit add: %d %d %d %d %x\n ", a->x1, a->y1, a->x2, a->y2, o->type);
	return;
}

/*
 * Interpolates two boxes, based on the value of the config->progress variable.
 * This is a strange implementation of a progress bar, introduced by the
 * authors of Bootsplash.
 */
void box_interpolate(box *a, box *b, box *c)
{
	int h = PROGRESS_MAX - config.progress;

#define inter_color(clo, cl1, cl2)									\
{																	\
	clo.r = (cl1.r * h + cl2.r * config.progress) / PROGRESS_MAX;	\
	clo.g = (cl1.g * h + cl2.g * config.progress) / PROGRESS_MAX;	\
	clo.b = (cl1.b * h + cl2.b * config.progress) / PROGRESS_MAX;	\
	clo.a = (cl1.a * h + cl2.a * config.progress) / PROGRESS_MAX;	\
}
	c->attr = a->attr;

	rect_interpolate(&a->re, &b->re, &c->re);

	inter_color(c->c_ul, a->c_ul, b->c_ul);
	inter_color(c->c_ur, a->c_ur, b->c_ur);
	inter_color(c->c_ll, a->c_ll, b->c_ll);
	inter_color(c->c_lr, a->c_lr, b->c_lr);
}

void box_prerender(stheme_t *theme, box *b, bool force)
{
	obj *o = container_of(b);

	if (b->attr & BOX_INTER) {
		box *tb = malloc(sizeof(box));
		if (!tb)
			return;

		box_interpolate(b, b->inter, tb);

		if (!memcmp(tb, b->curr, sizeof(box)) && !force) {
			free(tb);
			return;
		}

		if (b->attr & (BOX_VGRAD | BOX_SOLID) && b->curr->re.y1 == tb->re.y1 && b->curr->re.y2 == tb->re.y2 && !force) {
			rect re;

			re.y1 = tb->re.y1;
			re.y2 = tb->re.y2;

			if (b->curr->re.x1 != tb->re.x1) {
				re.x1 = min(b->curr->re.x1, tb->re.x1);
				re.x2 = max(b->curr->re.x1, tb->re.x1);
				blit_add(theme, &re);
				render_add(theme, o, &re);
			}

			if (b->curr->re.x2 != tb->re.x2) {
				re.x1 = min(b->curr->re.x2, tb->re.x2);
				re.x2 = max(b->curr->re.x2, tb->re.x2);
				blit_add(theme, &re);
				render_add(theme, o, &re);
			}

			if (memcmp(&tb->re, &o->bnd, sizeof(rect))) {
				memcpy(&o->bnd, &tb->re, sizeof(rect));
			}

		} else if (b->attr & (BOX_HGRAD | BOX_SOLID) && b->curr->re.x1 == tb->re.x1 && b->curr->re.x2 == tb->re.x2 && !force) {
			rect re;

			re.x1 = tb->re.x1;
			re.x2 = tb->re.x2;

			if (b->curr->re.y1 != tb->re.y1) {
				re.y1 = min(b->curr->re.y1, tb->re.y1);
				re.y2 = max(b->curr->re.y1, tb->re.y1);
				blit_add(theme, &re);
				render_add(theme, o, &re);
			}

			if (b->curr->re.y2 != tb->re.y2) {
				re.y1 = min(b->curr->re.y2, tb->re.y2);
				re.y2 = max(b->curr->re.y2, tb->re.y2);
				blit_add(theme, &re);
				render_add(theme, o, &re);
			}

			if (memcmp(&tb->re, &o->bnd, sizeof(rect))) {
				memcpy(&o->bnd, &tb->re, sizeof(rect));
			}
		} else {
			if (memcmp(&tb->re, &o->bnd, sizeof(rect))) {
				blit_add(theme, &o->bnd);
				render_add(theme, o, &o->bnd);
				memcpy(&o->bnd, &tb->re, sizeof(rect));
			}

			/* TODO: add some more optimizations here? */
			blit_add(theme, &tb->re);
			render_add(theme, o, &tb->re);
		}

		free(b->curr);
		b->curr = tb;

	} else {
		blit_add(theme, &o->bnd);
		render_add(theme, o, &o->bnd);
	}
}

void box_render(stheme_t *theme, box *box, rect *re, u8 *target)
{
	int x, y, a, r, g, b;
	int add;
	u8 *pic;

	int b_width = box->re.x2 - box->re.x1 + 1;
	int b_height = box->re.y2 - box->re.y1 + 1;

	for (y = re->y1; y <= re->y2; y++) {

		int r1, r2, g1, g2, b1, b2, a1, a2;
		int h1, h2, h;
		u8  opt = 0;
		float hr, hg, hb, ha, fr, fg, fb, fa;

		pic = target + (re->x1 + y * theme->xres) * fbd.bytespp;

		/* Do a nice 2x2 ordered dithering, like it was done in bootsplash;
		 * this makes the pics in 15/16bpp modes look much nicer;
		 * the produced pattern is:
		 * 303030303..
		 * 121212121..
		 */
		add = (box->re.x1 & 1);
		add ^= (add ^ y) & 1 ? 1 : 3;

		if (box->attr & BOX_SOLID) {
			r = box->c_ul.r;
			g = box->c_ul.g;
			b = box->c_ul.b;
			a = box->c_ul.a;
			opt = 1;
		} else {
			h1 = box->re.y2 - y;
			h2 = y - box->re.y1;

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

			r = r1; fr = (float)r1 + hr * (re->x1 - box->re.x1);
			g = g1; fg = (float)g1 + hg * (re->x1 - box->re.x1);
			b = b1; fb = (float)b1 + hb * (re->x1 - box->re.x1);
			a = a1; fa = (float)a1 + ha * (re->x1 - box->re.x1);
		}

		for (x = re->x1; x <= re->x2; x++) {
			if (!opt) {
				fa += ha;
				fr += hr;
				fg += hg;
				fb += hb;

				a = (u8)fa;
				b = (u8)fb;
				g = (u8)fg;
				r = (u8)fr;
			}

			put_pixel(a, r, g, b, pic, pic, add);
			pic += fbd.bytespp;
			add ^= 3;
		}
	}
}

void icon_render(stheme_t *theme, icon *ticon, rect *re, u8 *target)
{
//	rect crop, work;

	int y, yi, xi, wi, hi;
	u8 *out = NULL;
	u8 *in = NULL;

#if 0
	/* Interpolate a cropping rectangle if necessary. */
	if (ticon->crop) {
		memcpy(&crop, &ticon->crop_curr, sizeof(rect));
		crop.x1 += ticon->x;
		crop.x2 += ticon->x;
		crop.y1 += ticon->y;
		crop.y2 += ticon->y;
	} else {
		crop.x1 = ticon->x;
		crop.y1 = ticon->y;
		crop.x2 = crop.x1 + ticon->img->w - 1;
		crop.y2 = crop.y1 + ticon->img->h = 1;
	}
#endif

//	rect_min(&crop, re, &work);

	xi = re->x1 - ticon->x;
	yi = re->y1 - ticon->y;
	wi = re->x2 - re->x1 + 1;
	hi = yi + re->y2 - re->y1 + 1;

	for (y = ticon->y + yi; yi < hi; yi++, y++) {
		out = target + (ticon->x + xi + y * theme->xres) * fbd.bytespp;
		in = ticon->img->picbuf + (xi + yi * ticon->img->w) * 4;
		rgba2fb((rgbacolor*)in, out, out, wi, y, 1);
	}
}

void icon_prerender(stheme_t *theme, icon *c, bool force)
{
	obj *o = container_of(c);

	if (c->status == 0)
		return;

	if (!c->img || !c->img->picbuf)
		return;

	if (c->img->w > theme->xres - c->x || c->img->h > theme->yres - c->y) {
		iprint(MSG_WARN,"Icon %s does not fit on the screen - ignoring it.", c->img->filename);
		return;
	}

	if (c->crop) {
		rect crn;

		/* If the cropping rectangle is unchanged, don't render anything */
		rect_interpolate(&c->crop_from, &c->crop_to, &crn);
		if (!memcmp(&crn, &c->crop_curr, sizeof(rect)) && !force)
			return;

		/* TODO: add optimization: repaint only a part of the icon */
		memcpy(&c->crop_curr, &crn, sizeof(rect));

		crn.x1 += c->x;
		crn.x2 += c->x;
		crn.y1 += c->y;
		crn.y2 += c->y;

		blit_add(theme, &o->bnd);
		render_add(theme, o, &o->bnd);

		blit_add(theme, &crn);
		render_add(theme, o, &crn);

		memcpy(&o->bnd, &crn, sizeof(rect));
	} else {
		blit_add(theme, &o->bnd);
		render_add(theme, o, &o->bnd);
	}
}

void obj_render(stheme_t *theme, obj *o, rect *re, u8 *tg)
{
	switch (o->type) {

	case o_icon:
		icon_render(theme, o->p, re, tg);
		break;

	case o_box:
	{
		box *t = o->p;
		if (t->curr)
			box_render(theme, t->curr, re, tg);
		else
			box_render(theme, t, re, tg);
		break;
	}

#if WANT_TTF
	case o_text:
		text_render(theme, o->p, re, tg);
		break;
#endif
#if WANT_MNG
	case o_anim:
		anim_render(theme, o->p, re, tg);
		break;
#endif
	default:
		break;
	}
}

void obj_prerender(stheme_t *theme, obj *o, bool force)
{
	switch (o->type) {

	case o_icon:
		icon_prerender(theme, o->p, force);
		break;

	case o_box:
		box_prerender(theme, o->p, force);
		break;

#if WANT_TTF
	case o_text:
		text_prerender(theme, o->p, force);
		break;
#endif
#if WANT_MNG
	case o_anim:
		anim_prerender(theme, o->p, force);
		break;
#endif
	default:
		break;
	}
}

void render_objs(stheme_t *theme, u8 *target, u8 mode, bool force)
{
	item *i, *j;

	/*
	 * First pass: mark rectangles for reblitting and rerendering
	 * via object specific rendering routines.
	 */
	for (i = theme->objs.head; i != NULL; i = i->next) {
		obj *o = i->p;
		if (!(o->modes & mode))
			continue;

		if (o->invalid) {
			obj_prerender(theme, o, force);
			o->invalid = false;
		}
	}

	blit_normalize(theme);

	for (i = theme->blit.head; i != NULL; i = i->next) {
		rect *re = i->p;

		blit((u8*)theme->silent_img.data, re, theme->xres, target, re->x1, re->y1, theme->xres);

		for (j = theme->objs.head; j != NULL; j = j->next) {
			obj *o = j->p;
			rect tmp;

			if (!(o->modes & mode))
				continue;

			rect_min(&o->bnd, re, &tmp);

			if (tmp.x2 < tmp.x1 || tmp.y2 < tmp.y1)
				continue;

			obj_render(theme, o, &tmp, target);
		}
	}
}

void invalidate_all(stheme_t *theme)
{
	item *i;

	for (i = theme->objs.head; i != NULL; i = i->next) {
		obj *o = i->p;
		o->invalid = true;
	}
}

void invalidate_progress(stheme_t *theme)
{
	item *i;

	for (i = theme->objs.head; i != NULL; i = i->next) {
		obj *o = i->p;

		switch (o->type) {

		case o_box:
		{
			box *b = o->p;

			if (b->inter)
				o->invalid = true;
			break;
		}

		case o_icon:
		{
			icon *ic = o->p;

			if (ic->crop)
				o->invalid = true;
			break;
		}
#if WANT_TTF
		case o_text:
		{
			text *t = o->p;

			if (t->curr_progress >= 0)
				o->invalid = true;
			break;
		}
#endif
#if WANT_MNG
		case o_anim:
		{
			anim *t = o->p;

			if ((t->flags & F_ANIM_METHOD_MASK) == F_ANIM_PROPORTIONAL)
				o->invalid = true;
			break;
		}
#endif
		}
	}
}

void bnd_init(stheme_t *theme)
{
	item *i;

	/* Initialize the bounding rectangles */
	for (i = theme->objs.head; i != NULL; i = i->next) {
		obj *a = i->p;

		switch (a->type) {

		case o_box:
		{
			box *t = a->p;

			if (t->inter) {
				rect_bnd(&t->re, &t->inter->re, &a->bnd);
			} else {
				memcpy(&a->bnd, &t->re, sizeof(rect));
			}

			break;
		}

		case o_icon:
		{
			icon *t = a->p;

			if (t->crop) {
				rect_bnd(&t->crop_from, &t->crop_to, &a->bnd);
				a->bnd.x1 += t->x;
				a->bnd.x2 += t->x;
				a->bnd.y1 += t->y;
				a->bnd.y2 += t->y;
			} else {
				a->bnd.x1 = t->x;
				a->bnd.y1 = t->y;
				a->bnd.x2 = a->bnd.x1 + t->img->w - 1;
				a->bnd.y2 = a->bnd.y1 + t->img->h - 1;
			}
			break;
		}
#if WANT_TTF
		case o_text:
			text_bnd(theme, a->p, &a->bnd);
			break;
#endif
#if WANT_MNG
		case o_anim:
		{
			anim *t = a->p;
			a->bnd.x1 = t->x;
			a->bnd.y1 = t->y;
			a->bnd.x2 = t->x + t->w - 1;
			a->bnd.y2 = t->y + t->h - 1;
			break;
		}
#endif
		default:
			break;
		}
	}
}

#if 0
int bgcache_init(stheme_t *theme)
{
	item *i, *j;

	/* Prepare the background image */
	memcpy(theme->bgbuf, theme->silent_img.data, theme->xres * theme->yres * fbd.bytespp);

	/*
	 * If we intersect another object, we will need to allocate a background
	 * buffer to avoid unnecessary redrawing later.  If we don't intersect
	 * anything, there's no need for a buffer as we will get our background
	 * directly from silent_img
	 */
	for (i = theme->objs.head; i != NULL; i = i->next) {
		obj *a = i->p;

		for (j = theme->objs.head; j != i; j = j->next) {
			obj *b = j->p;

			if (rect_intersect(&a->bnd, &b->bnd)) {
				a->bgcache = malloc(fbd.bytespp * (a->bnd.x2 - a->bnd.x1 + 1) * (a->bnd.y2 - a->bnd.y1 + 1));
				blit(theme->bgbuf, &a->bnd, theme->xres, a->bgcache, 0, 0, (a->bnd.x2 - a->bnd.x1 + 1));
				break;
			}
		}

		render_obj(theme, theme->bgbuf, 's', a, true);
	}

	return 0;
}
#endif


