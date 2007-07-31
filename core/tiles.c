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
		if (b->curr)
			free(b->curr);
	}

	free(o);
	return;
}


