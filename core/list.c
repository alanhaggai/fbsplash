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

