/*
 * splash_parse.c - Functions for parsing bootsplash config files
 * 
 * Copyright (C) 2004-2005, Michal Januszewski <spock@gentoo.org>
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include <stdlib.h>
#include <string.h>
//#include <linux/types.h>
#include <stdio.h>
//#include <ctype.h>
#include <linux/fb.h>
#include "splash.h"

struct config_opt {
	char *name;
	enum { t_int, t_path, t_box, t_icon, t_rect } type;
	void *val;
};

char *cf_silentpic = NULL;
char *cf_pic = NULL;
char *cf_silentpic256 = NULL;	/* these are pictures for 8bpp modes */
char *cf_pic256 = NULL;

struct rect cf_rects[MAX_RECTS];
int cf_rects_cnt = 0;

struct splash_box cf_boxes[MAX_BOXES];
int cf_boxes_cnt = 0;
struct splash_config cf;

struct splash_icon cf_icons[MAX_ICONS];
int cf_icons_cnt = 0;

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
	char *p;
	struct splash_icon icon;
	int i;
	
	skip_whitespace(&t);
	for (i = 0; t[i] != ' ' && t[i] != '\t' && t[i] != '\0'; i++);
	t[i] = 0;

	icon.filename = get_filepath(t);
	t += (i+1);
	
	skip_whitespace(&t);	
	icon.x = strtol(t,&p,0);
	if (t == p)
		return;

	t = p; skip_whitespace(&t);
	icon.y = strtol(t,&p,0);
	if (t == p)
		return;
	
	t = p; skip_whitespace(&t);
	
	if (!strncmp(t, "svc_inactive_start", 18)) {
		icon.type = e_svc_inact_start; t += 18;
	} else if (!strncmp(t, "svc_inactive_stop", 17)) {
		icon.type = e_svc_inact_stop; t += 17;
	} else if (!strncmp(t, "svc_started", 11)) {
		icon.type = e_svc_started; t += 11;
	} else if (!strncmp(t, "svc_stopped", 11)) {
		icon.type = e_svc_stopped; t += 11;
	} else if (!strncmp(t, "svc_start_failed", 17)) {
		icon.type = e_svc_start_failed; t += 17;
	} else if (!strncmp(t, "svc_stop_failed",  16)) {
		icon.type = e_svc_stop_failed; t += 16;
	} else if (!strncmp(t, "svc_stop", 8)) {
		icon.type = e_svc_stop; t += 8;
	} else if (!strncmp(t, "svc_start", 9)) {
		icon.type = e_svc_start; t += 9;
	} else {
		icon.type = e_display; 
		goto out;
	}
	
	skip_whitespace(&t);
	for (i = 0; t[i] != ' ' && t[i] != '\t' && t[i] != '\0'; i++);
	t[i] = 0;

	i = 0;
	
	switch (icon.type) {
		
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
		/* we should never get here */
		break;
	}

	if (!i)
		return;
	
	icon.svc = strdup(t);

out:
	if (cf_icons_cnt >= MAX_ICONS)
		return;
	cf_icons[cf_icons_cnt++] = icon;
	return;
}

void parse_rect(char *t)
{
	char *p;	
	
	struct rect crect;
	skip_whitespace(&t);

	while (!isdigit(*t)) {
		t++;
	}

	crect.x1 = strtol(t,&p,0);
	if (t == p)
		return;
	t = p; skip_whitespace(&t);
	crect.y1 = strtol(t,&p,0);
	if (t == p)
		return;
	t = p; skip_whitespace(&t);
	crect.x2 = strtol(t,&p,0);
	if (t == p)
		return;
	t = p; skip_whitespace(&t);
	crect.y2 = strtol(t,&p,0);
	if (t == p)
		return;
	t = p; skip_whitespace(&t);

	/* sanity checks */
	if (crect.x1 >= fb_var.xres)
		crect.x1 = fb_var.xres-1;
	if (crect.x2 >= fb_var.xres)
		crect.x2 = fb_var.xres-1;
	if (crect.y1 >= fb_var.yres)
		crect.y1 = fb_var.yres-1;
	if (crect.y2 >= fb_var.yres)
		crect.y2 = fb_var.yres-1;
	
	if (cf_boxes_cnt >= MAX_RECTS)
		return;
	cf_rects[cf_rects_cnt++] = crect;
	return;
}

void parse_box(char *t)
{
	char *p;	
	int ret;
	
	struct splash_box cbox;
	
	skip_whitespace(&t);
	cbox.attr = 0;

	while (!isdigit(*t)) {
	
		if (!strncmp(t,"noover",6)) {
			cbox.attr |= BOX_NOOVER;
			t += 6;
		} else if (!strncmp(t, "inter", 5)) {
			cbox.attr |= BOX_INTER;
			t += 5;
		} else if (!strncmp(t, "silent", 6)) {
			cbox.attr |= BOX_SILENT;
			t += 6;
		} else {
			fprintf(stderr, "parse error @ line %d\n", line);
			return;
		}

		skip_whitespace(&t);
	}	
	
	cbox.x1 = strtol(t,&p,0);
	if (t == p)
		return;
	t = p; skip_whitespace(&t);
	cbox.y1 = strtol(t,&p,0);
	if (t == p)
		return;
	t = p; skip_whitespace(&t);
	cbox.x2 = strtol(t,&p,0);
	if (t == p)
		return;
	t = p; skip_whitespace(&t);
	cbox.y2 = strtol(t,&p,0);
	if (t == p)
		return;
	t = p; skip_whitespace(&t);

	/* sanity checks */
	if (cbox.x1 >= fb_var.xres)
		cbox.x1 = fb_var.xres-1;
	if (cbox.x2 >= fb_var.xres)
		cbox.x2 = fb_var.xres-1;
	if (cbox.y1 >= fb_var.yres)
		cbox.y1 = fb_var.yres-1;
	if (cbox.y2 >= fb_var.yres)
		cbox.y2 = fb_var.yres-1;
	
#define zero_color(cl) *(u32*)(&cl) = 0;
#define is_zero_color(cl) (*(u32*)(&cl) == 0)
#define assign_color(c1, c2) *(u32*)(&c1) = *(u32*)(&c2);
	
	zero_color(cbox.c_ul);
	zero_color(cbox.c_ur);
	zero_color(cbox.c_ll);
	zero_color(cbox.c_lr);
	
	if (parse_color(&t, &cbox.c_ul)) 
		goto pb_err;

	skip_whitespace(&t);

	ret = parse_color(&t, &cbox.c_ur);
	
	if (ret == -1) {
		assign_color(cbox.c_ur, cbox.c_ul);
		assign_color(cbox.c_lr, cbox.c_ul);
		assign_color(cbox.c_ll, cbox.c_ul);
		goto pb_end;
	} else if (ret == -2)
		goto pb_err;

	skip_whitespace(&t);

	if (parse_color(&t, &cbox.c_ll))
		goto pb_err;
	
	skip_whitespace(&t);

	if (parse_color(&t, &cbox.c_lr))
		goto pb_err;
pb_end:	
	if (cf_boxes_cnt >= MAX_BOXES)
		return;
	cf_boxes[cf_boxes_cnt++] = cbox;
	return;
pb_err:
	fprintf(stderr, "parse error @ line %d\n", line);
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


