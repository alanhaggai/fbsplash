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



#define obj_add(x) list_add(&tmptheme.objs, container_of(x))

void obj_free(obj *o)
{
	if (!o)
		return;

	if (obj->bgcache)
		free(obj->bgcache)

	free(obj);
	return;
}

bool rect_intersect(rect a, rect b)
{
	if (a.x2 < b.x1 || a.x1 > b.x2 || a.y1 > b.y2 || a.y2 < b.y1)
		return false;
	else
		return true;
}

int tiles_init()
{




	/*
	 * If we intersect another object, we will need to allocate a background
	 * buffer to avoid unnecessary redrawing later.  If we don't intersect
	 * anything, there's no need for a buffer and will get our background
	 * directly from silent_img
	 */
	for (i = objs.head; i = i->next; i != NULL) {
		if (rect_intersect((obj*)(i->p)->bnd, bnd)) {
			o->bgcache = malloc(fbd.bytespp * (bnd.x2 - bnd.x1) * (bnd.y2 - bnd.y1));
			if (!o->bgcache) {
				free(o);
				return NULL;
			}
			break;
		}
	}


