/*
 * tiles.c -- Dirty tiles support for splashutils
 *
 * Copyright (c) 2007, Michal Januszewski <spock@gentoo.org>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include "util.h"

void obj_free(obj *o)
{
	if (!o)
		return;

	if (o->bgcache)
		free(o->bgcache);

	if (o->type == o_box) {
		box *b = o->p;
		if (b->inter)
			free(container_of(b->inter));
	}

	free(o);
	return;
}

bool rect_intersect(rect *a, rect *b)
{
	if (a->x2 < b->x1 || a->x1 > b->x2 || a->y1 > b->y2 || a->y2 < b->y1)
		return false;
	else
		return true;
}

int tiles_init(stheme_t *theme)
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
				a->bgcache = malloc(fbd.bytespp * (a->bnd.x2 - a->bnd.x1) * (a->bnd.y2 - a->bnd.y1));
			//	blit(theme->bgbuf, &a->bnd, a->bgcache, 0, 0);
				break;
			}
		}

		render_obj(theme, theme->bgbuf, 's', FB_SPLASH_IO_ORIG_USER, a);
	}

	return 0;
}

