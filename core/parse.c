/*
 * parse.c - Functions for parsing splashutils config files
 * 
 * Copyright (C) 2004-2005, Michael Januszewski <spock@gentoo.org>
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <linux/fb.h>
#include "splash.h"

struct config_opt {
	char *name;
	enum { t_int, t_path, t_box, t_icon, t_rect } type;
	void *val;
};

list icons = { NULL, NULL };
list objs = { NULL, NULL };
list rects = { NULL, NULL };

char *cf_silentpic 	= NULL;
char *cf_pic 		= NULL;
char *cf_silentpic256 	= NULL;		/* pictures for 8bpp modes */
char *cf_pic256 	= NULL;

struct splash_config cf;

int line = 0;

/* Note that pic256 and silentpic256 have to be located before pic and 
 * silentpic or we are gonna get a parse error @ pic256/silentpic256. */
struct config_opt opts[] =
{
	{	.name = "jpeg",
		.type = t_path,
		.val = &cf_pic		},

	{	.name = "pic256",	
		.type = t_path,
		.val = &cf_pic256	},

	{	.name = "silentpic256",	
		.type = t_path,
		.val = &cf_silentpic256	},
	
	{	.name = "silentjpeg",
		.type = t_path,
		.val = &cf_silentpic	},

	{	.name = "pic",
		.type = t_path,
		.val = &cf_pic		},

	{	.name = "silentpic",
		.type = t_path,
		.val = &cf_silentpic	},
	
	{ 	.name = "bg_color",
		.type = t_int,
		.val = &cf.bg_color	},

	{	.name = "tx",
		.type = t_int,
		.val = &cf.tx		},

	{	.name = "ty",
		.type = t_int,
		.val = &cf.ty		},

	{	.name = "tw",
		.type = t_int,
		.val = &cf.tw		},

	{	.name = "th",
		.type = t_int,
		.val = &cf.th		},
	
	{	.name = "box",
		.type = t_box,
		.val = NULL		},

	{	.name = "icon",
		.type = t_icon,
		.val = NULL		},
	
	{	.name = "rect",
		.type = t_rect,
		.val = NULL		},
};

int isdigit(char c) 
{
	return (c >= '0' && c <= '9') ? 1 : 0;
}

int ishexdigit(char c) 
{
	return (isdigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) ? 1 : 0;
}

void skip_whitespace(char **buf) 
{
	while (**buf == ' ' || **buf == '\t')
		(*buf)++;
		
	return;
}

void parse_int(char *t, struct config_opt opt)
{
	if (*t != '=') {
		fprintf(stderr, "parse error @ line %d\n", line);
		return;
	}

	t++; skip_whitespace(&t);
	*(unsigned int*)opt.val = strtol(t,NULL,0);
}

void parse_path(char *t, struct config_opt opt)
{
	if (*t != '=') {
		fprintf(stderr, "parse error @ line %d\n", line);
		return;
	}

	t++; skip_whitespace(&t);
	*(char**)opt.val = get_filepath(t);
}

int parse_color(char **t, struct color *cl) 
{
	u32 h, len = 0;
	char *p;
	
	if (**t != '#') {
		return -1;
	}

	(*t)++;

	for (p = *t; ishexdigit(*p); p++, len++);

	p = *t;
	h = strtoul(*t, &p, 16);
	if (*t == p)
		return -2;

	if (len > 6) {
		cl->a = h & 0xff;
		cl->r = (h >> 24) & 0xff;
		cl->g = (h >> 16) & 0xff;
		cl->b = (h >> 8)  & 0xff;
	} else {
		cl->a = 0xff;
		cl->r = (h >> 16) & 0xff;
		cl->g = (h >> 8 ) & 0xff;
		cl->b = h & 0xff;
	}
	
	*t = p;

	return 0;
}

int is_in_svclist(char *svc, char *list)
{
	char *data = getenv(list);
	int l = strlen(svc);
	
	if (!data)
		return 0;

	skip_whitespace(&data);
	
	while (1) {

		if (data[0] == 0)
			break;
		
		if (!strncmp(data, svc, l) && (data[l] == ' ' || data[l] == 0))
			return 1;

		for ( ; data[0] != 0 && data[0] != ' '; data++);

		skip_whitespace(&data);
	}

	return 0;
}

void parse_icon(char *t)
{
	char *filename = NULL;
	char *p;
	icon *cic = malloc(sizeof(icon));
	icon_img *cim;
	item *ti;
	obj *cobj;
	int i;

	if (!cic)
		return;
	
	cic->svc = NULL;
	
	skip_whitespace(&t);
	for (i = 0; t[i] != ' ' && t[i] != '\t' && t[i] != '\0'; i++);
	t[i] = 0;

	filename = get_filepath(t);
	t += (i+1);
	
	skip_whitespace(&t);	
	cic->x = strtol(t,&p,0);
	if (t == p)
		goto pi_err;

	t = p; skip_whitespace(&t);
	cic->y = strtol(t,&p,0);
	if (t == p)
		goto pi_err;
	
	t = p; skip_whitespace(&t);
	
	if (!strncmp(t, "svc_inactive_start", 18)) {
		cic->type = e_svc_inact_start; t += 18;
	} else if (!strncmp(t, "svc_inactive_stop", 17)) {
		cic->type = e_svc_inact_stop; t += 17;
	} else if (!strncmp(t, "svc_started", 11)) {
		cic->type = e_svc_started; t += 11;
	} else if (!strncmp(t, "svc_stopped", 11)) {
		cic->type = e_svc_stopped; t += 11;
	} else if (!strncmp(t, "svc_start_failed", 17)) {
		cic->type = e_svc_start_failed; t += 17;
	} else if (!strncmp(t, "svc_stop_failed",  16)) {
		cic->type = e_svc_stop_failed; t += 16;
	} else if (!strncmp(t, "svc_stop", 8)) {
		cic->type = e_svc_stop; t += 8;
	} else if (!strncmp(t, "svc_start", 9)) {
		cic->type = e_svc_start; t += 9;
	} else {
		cic->type = e_display; 
	}
	
	skip_whitespace(&t);
	for (i = 0; t[i] != ' ' && t[i] != '\t' && t[i] != '\0'; i++);
	t[i] = 0;
	i = 0;

	if (arg_task != start_daemon) {
	
		switch (cic->type) {
		
		case e_svc_inact_start:
			if (is_in_svclist(t, "SPL_SVC_INACTIVE_START"))
				i = 1;
			break;
	
		case e_svc_inact_stop:
			if (is_in_svclist(t, "SPL_SVC_INACTIVE_STOP"))
				i = 1;
			break;
		
		case e_svc_started:
			if (is_in_svclist(t, "SPL_SVC_STARTED"))
				i = 1;
			break;	

		case e_svc_stopped:
			if (is_in_svclist(t, "SPL_SVC_STOPPED"))
				i = 1;
			break;

		case e_svc_start_failed:
			if (is_in_svclist(t, "SPL_SVC_START_FAILED"))
				i = 1;
			break;

		case e_svc_stop_failed:
			if (is_in_svclist(t, "SPL_SVC_STOP_FAILED"))
				i = 1;
			break;

		case e_svc_stop:
			if (is_in_svclist(t, "SPL_SVC_STOP"))
				i = 1;
			break;

		case e_svc_start:
			if (is_in_svclist(t, "SPL_SVC_START"))
				i = 1;
			break;

		case e_display:
			i = 1;
			break;
		}

		if (!i)
			goto pi_out;

		cic->status = 1;
	} else {

		if (cic->type == e_display) 
			cic->status = 1;
		else 
			cic->status = 0;
	}

	if (cic->type != e_display) {
		cic->svc = strdup(t);
	}
		
	for (ti = icons.head ; ti != NULL; ti = ti->next) {
		icon_img *ii = (icon_img*) ti->p;
	
		if (!strcmp(ii->filename, filename)) {
			cic->img = ii;
			goto pi_end;
		}
	}

	/* Allocate a new entry in the icons list */
	cim = malloc(sizeof(icon_img));	
	if (!cim)
		goto pi_outm;	
	cim->filename = filename;
	cim->w = cim->h	= 0;
	cim->picbuf = NULL;
	list_add(&icons, cim);
	cic->img = cim;

pi_end:
	cobj = malloc(sizeof(obj));
	if (!cobj) {
pi_outm:	fprintf(stderr, "Cannot allocate memory (parse_icon)!");
		goto pi_out;
	}
	cobj->type = o_icon;
	cobj->p = cic;
	
	list_add(&objs, cobj);
	return;

pi_err:	fprintf(stderr, "parse error @ line %d\n", line);
pi_out:	if (filename)
		free(filename);
	if (cic->svc)
		free(cic->svc);
	free(cic);
	return;
}

void parse_rect(char *t)
{
	char *p;	
	rect *crect = malloc(sizeof(rect));
	
	if (!crect)
		return;
	
	skip_whitespace(&t);

	while (!isdigit(*t)) {
		t++;
	}

	crect->x1 = strtol(t,&p,0);
	if (t == p)
		goto pr_err;
	t = p; skip_whitespace(&t);
	crect->y1 = strtol(t,&p,0);
	if (t == p)
		goto pr_err;
	t = p; skip_whitespace(&t);
	crect->x2 = strtol(t,&p,0);
	if (t == p)
		goto pr_err;
	t = p; skip_whitespace(&t);
	crect->y2 = strtol(t,&p,0);
	if (t == p)
		goto pr_err;
	t = p; skip_whitespace(&t);

	/* sanity checks */
	if (crect->x1 >= fb_var.xres)
		crect->x1 = fb_var.xres-1;
	if (crect->x2 >= fb_var.xres)
		crect->x2 = fb_var.xres-1;
	if (crect->y1 >= fb_var.yres)
		crect->y1 = fb_var.yres-1;
	if (crect->y2 >= fb_var.yres)
		crect->y2 = fb_var.yres-1;

	list_add(&rects, crect);
	return;
pr_err:
	fprintf(stderr, "parse error @ line %d\n", line);
	free(crect);
	return;
}

void parse_box(char *t)
{
	char *p;	
	int ret;
	box *cbox = malloc(sizeof(box));
	obj *cobj = NULL;

	if (!cbox)
		return;
	
	skip_whitespace(&t);
	cbox->attr = 0;

	while (!isdigit(*t)) {
	
		if (!strncmp(t,"noover",6)) {
			cbox->attr |= BOX_NOOVER;
			t += 6;
		} else if (!strncmp(t, "inter", 5)) {
			cbox->attr |= BOX_INTER;
			t += 5;
		} else if (!strncmp(t, "silent", 6)) {
			cbox->attr |= BOX_SILENT;
			t += 6;
		} else {
			goto pb_err;
		}

		skip_whitespace(&t);
	}	

	cbox->x1 = strtol(t,&p,0);
	if (t == p)
		goto pb_err;
	t = p; skip_whitespace(&t);
	cbox->y1 = strtol(t,&p,0);
	if (t == p)
		goto pb_err;
	t = p; skip_whitespace(&t);
	cbox->x2 = strtol(t,&p,0);
	if (t == p)
		goto pb_err;
	t = p; skip_whitespace(&t);
	cbox->y2 = strtol(t,&p,0);
	if (t == p)
		goto pb_err;
	t = p; skip_whitespace(&t);

	/* sanity checks */
	if (cbox->x1 >= fb_var.xres)
		cbox->x1 = fb_var.xres-1;
	if (cbox->x2 >= fb_var.xres)
		cbox->x2 = fb_var.xres-1;
	if (cbox->y1 >= fb_var.yres)
		cbox->y1 = fb_var.yres-1;
	if (cbox->y2 >= fb_var.yres)
		cbox->y2 = fb_var.yres-1;

#define zero_color(cl) *(u32*)(&cl) = 0;
#define is_zero_color(cl) (*(u32*)(&cl) == 0)
#define assign_color(c1, c2) *(u32*)(&c1) = *(u32*)(&c2);
	
	zero_color(cbox->c_ul);
	zero_color(cbox->c_ur);
	zero_color(cbox->c_ll);
	zero_color(cbox->c_lr);
	
	if (parse_color(&t, &cbox->c_ul)) 
		goto pb_err;

	skip_whitespace(&t);

	ret = parse_color(&t, &cbox->c_ur);
	
	if (ret == -1) {
		assign_color(cbox->c_ur, cbox->c_ul);
		assign_color(cbox->c_lr, cbox->c_ul);
		assign_color(cbox->c_ll, cbox->c_ul);
		goto pb_end;
	} else if (ret == -2)
		goto pb_err;

	skip_whitespace(&t);

	if (parse_color(&t, &cbox->c_ll))
		goto pb_err;
	
	skip_whitespace(&t);

	if (parse_color(&t, &cbox->c_lr))
		goto pb_err;
pb_end:
	cobj = malloc(sizeof(obj));
	if (!cobj) {
		free(cbox);
		fprintf(stderr, "Cannot allocate memory (parse_box)!");
		return;
	}
	cobj->type = o_box;
	cobj->p = cbox;
	
	list_add(&objs, cobj);
	return;

pb_err:
	fprintf(stderr, "parse error @ line %d\n", line);
	free(cbox);
	return;
}

int parse_cfg(char *cfgfile)
{
	FILE* cfg;
	char buf[1024];
	char *t;
	int len, i;

	if ((cfg = fopen(cfgfile,"r")) == NULL) {
		fprintf(stderr, "Can't open config file %s.\n", cfgfile);
		return 1;
	}
	
	while (fgets(buf, sizeof(buf), cfg)) {

		line++;

		len = strlen(buf);
			
		if (len == 0 || len == sizeof(buf)-1)
			continue;

		buf[len-1] = 0;		/* get rid of \n */
		
		t = buf;	
		skip_whitespace(&t);
		
		/* skip comments */
		if (*t == '#')
			continue;
			
		for (i = 0; i < sizeof(opts) / sizeof(struct config_opt); i++) 
		{
			if (!strncmp(opts[i].name, t, strlen(opts[i].name))) {
	
				t += strlen(opts[i].name); 
				skip_whitespace(&t);

				switch(opts[i].type) {
				
				case t_path:
					parse_path(t, opts[i]);
					break;

				case t_int:
					parse_int(t, opts[i]);
					break;
				
				case t_box:
					parse_box(t);
					break;

				case t_icon:
					parse_icon(t);
					break;
				
				case t_rect:
					parse_rect(t);
					break;
				}
			}
		}
	}

	fclose(cfg);
	return 0;
}


