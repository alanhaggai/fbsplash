/*
 * list.c -- list utility functions
 *
 * Copyright (C) 2005, 2007 Michal Januszewski <spock@gentoo.org>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */
#include <stdlib.h>
#include "common.h"
#include "render.h"

void list_add(list *l, void *obj)
{
	if (l->tail != NULL) {
		l->tail->next = malloc(sizeof(item));
		l->tail->next->p = obj;
		l->tail->next->next = NULL;
		l->tail = l->tail->next;
	} else {
		l->head = l->tail = malloc(sizeof(item));
		l->tail->next = NULL;
		l->tail->p = obj;
	}
}

/**
 * Append an element to the end of the list while simultaneously discarding
 * the list head.
 */
void list_ringadd(list *l, void *obj)
{
	item *i = l->head;

	l->head = i->next;
	free(i->p);
	free(i);
	list_add(l, obj);
}

void list_free(list l, bool free_item)
{
	item *i, *j;

	for (i = l.head; i != NULL; ) {
		j = i->next;
		if (free_item)
			free(i->p);
		free(i);
		i = j;
	}

	list_init(l);
}

void list_del(list *l, item *prev, item *curr)
{
	prev->next = curr->next;

	if (l->tail == curr)
		l->tail = prev;

	free(curr);
}
