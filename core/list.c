/*
 * list.c - list utility functions 
 * 
 * Copyright (C) 2005 Michal Januszewski <spock@gentoo.org>
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */
#include <stdlib.h>
#include "splash.h"

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

