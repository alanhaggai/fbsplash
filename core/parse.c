/*
 * parse.c - Functions for parsing splashutils config files
 * 
 * Copyright (C) 2004-2005, Michal Januszewski <spock@gentoo.org>
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <ctype.h>
#include "util.h"

struct config_opt {
	char *name;
	enum { 
		t_int, t_path, t_box, t_icon, t_rect, t_color, t_fontpath,
#if defined(CONFIG_MNG) && !defined(TARGET_KERNEL)
		t_anim,
#endif
#if WANT_TTF
		t_text,	
#endif
	} type;
	void *val;
};

list anims = { NULL, NULL };
list fonts = { NULL, NULL };
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
	
	{	.name = "bg_color",
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

#if defined(CONFIG_MNG) && !defined(TARGET_KERNEL)
	{	.name = "anim",
		.type = t_anim,
		.val = NULL		},
#endif
#if WANT_TTF
	{	.name = "text_x",
		.type = t_int,
		.val = &cf.text_x	},

	{	.name = "text_y",
		.type = t_int,
		.val = &cf.text_y	},

	{	.name = "text_size",
		.type = t_int,
		.val = &cf.text_size	},

	{	.name = "text_color",
		.type = t_color,
		.val = &cf.text_color	},

	{	.name = "text_font",
		.type = t_fontpath,
		.val = &cf.text_font	},

	{	.name = "text",
		.type = t_text,
		.val = NULL		},
#endif /* TTF */
};

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

void skip_nonwhitespace(char **buf)
{
	while (**buf != ' ' && **buf != '\t')
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
	*(u16*)opt.val = strtol(t,NULL,0);
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

char *get_fontpath(char *t)
{
	char buf[512];
	char buf2[512];
	struct stat st1, st2;

	if (t[0] == '/') {
		return strdup(t);
	}
		
	snprintf(buf, 512, "%s/%s/%s", THEME_DIR, config->theme, t);
	snprintf(buf2, 512, "%s/%s", THEME_DIR, t);
	
	stat(buf, &st1);
	stat(buf2, &st2);
	
	if (S_ISREG(st1.st_mode) || S_ISLNK(st1.st_mode)) {
		return strdup(buf);
	} else if (S_ISREG(st2.st_mode) || S_ISLNK(st2.st_mode)) {
		return strdup(buf2);
	} else {
		return strdup(buf);
	}

	return NULL;
}

void parse_fontpath(char *t, struct config_opt opt)
{
	if (*t != '=') {
		fprintf(stderr, "parse error @ line %d\n", line);
		return;
	}

	t++; skip_whitespace(&t);
	*(char**)opt.val = get_fontpath(t);
}

int parse_color(char **t, struct color *cl) 
{
	u32 h, len = 0;
	char *p;
	
	if (**t != '#') {
		if (strncmp(*t, "0x", 2))
			return -1;
		else
			(*t) += 2;
	} else  {
		(*t)++;
	}

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

int parse_4vec(char **t, rect *r)
{
	char *p;

	if (**t != '<')
		return -1;
	(*t)++;

	skip_whitespace(t);

	p = *t;
	r->x1 = strtol(p, t, 0);
	if (p == *t)
		return -1;
	skip_whitespace(t);
	if (**t != ',')
		return -1;
	(*t)++;

	p = *t;
	r->y1 = strtol(p, t, 0);
	if (p == *t)
		return -1;
	skip_whitespace(t);
	if (**t != ',')
		return -1;
	(*t)++;

	p = *t;
	r->x2 = strtol(p, t, 0);
	if (p == *t)
		return -1;
	skip_whitespace(t);
	if (**t != ',')
		return -1;
	(*t)++;

	p = *t;
	r->y2 = strtol(p, t, 0);
	if (p == *t)
		return -1;

	skip_whitespace(t);
	if (**t != '>')
		return -1;

	(*t)++;
	return 0;
}

int parse_svc_state(char *t, enum ESVC *state)
{
	if (!strncmp(t, "svc_inactive_start", 18)) {
		*state = e_svc_inact_start; return 18;
	} else if (!strncmp(t, "svc_inactive_stop", 17)) {
		*state = e_svc_inact_stop; return 17;
	} else if (!strncmp(t, "svc_started", 11)) {
		*state = e_svc_started; return 11;
	} else if (!strncmp(t, "svc_stopped", 11)) {
		*state = e_svc_stopped; return 11;
	} else if (!strncmp(t, "svc_start_failed", 17)) {
		*state = e_svc_start_failed; return 17;
	} else if (!strncmp(t, "svc_stop_failed",  16)) {
		*state = e_svc_stop_failed; return 16;
	} else if (!strncmp(t, "svc_stop", 8)) {
		*state = e_svc_stop; return 8;
	} else if (!strncmp(t, "svc_start", 9)) {
		*state = e_svc_start; return 9;
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

	/* Do we need to crop this icon? */
	if (!strncmp(t, "crop", 4)) {
		t += 4;
		skip_whitespace(&t);
		
		if (parse_4vec(&t, &cic->crop_from))
			goto pi_err;

		skip_whitespace(&t);

		if (parse_4vec(&t, &cic->crop_to))
			goto pi_err;

		skip_whitespace(&t);
		cic->crop = 1;
	} else {
		cic->crop = 0;
	}

	i = parse_svc_state(t, &cic->type);

	if (i)
		t += i;
	else
		cic->type = e_display;

	/* Find the service name */
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
	if (crect->x1 >= cf.xres)
		crect->x1 = cf.xres-1;
	if (crect->x2 >= cf.xres)
		crect->x2 = cf.xres-1;
	if (crect->y1 >= cf.yres)
		crect->y1 = cf.yres-1;
	if (crect->y2 >= cf.yres)
		crect->y2 = cf.yres-1;

	list_add(&rects, crect);
	return;
pr_err:
	fprintf(stderr, "parse error @ line %d\n", line);
	free(crect);
	return;
}

#if defined(CONFIG_MNG) && !defined(TARGET_KERNEL)
void parse_anim(char *t)
{
	char *p;	
	char *filename;
	obj *cobj = NULL;
	anim *canim = malloc(sizeof(anim));
	int i;

	if (!canim)
		return;
	
	skip_whitespace(&t);
	canim->flags = 0;

#if 0
	while (1) {
		if (!strncmp(t, "verbose", 7)) {
			canim->flags |= F_ANIM_VERBOSE;
			t += 7;
		} else if (!strncmp(t, "silent", 6)) {
			canim->flags |= F_ANIM_SILENT;
			t += 6;
		} else {
			skip_whitespace(&t);
			break;
		}

		skip_whitespace(&t);
	}

	if (canim->flags == 0)
	    canim->flags = F_ANIM_SILENT | F_ANIM_VERBOSE;
#endif

	if (!strncmp(t, "once", 4)) {
		canim->flags |= F_ANIM_ONCE;
		t += 4;
	} else if (!strncmp(t, "loop", 4)) {
		canim->flags |= F_ANIM_LOOP;
		t += 4;
	} else if (!strncmp(t, "proportional", 12)) {
		canim->flags |= F_ANIM_PROPORTIONAL;
		t += 12;
	} else {
		goto pa_err;
	}

	skip_whitespace(&t);

	filename = t;
	skip_nonwhitespace(&t);
	*t = '\0';
	t++;

	skip_whitespace(&t);

	canim->x = strtol(t,&p,0);
	if (t == p)
		goto pa_err;
	t = p; skip_whitespace(&t);

	canim->y = strtol(t,&p,0);
	if (t == p)
		goto pa_err;
	t = p; skip_whitespace(&t);

	/* sanity checks */
	if (canim->x >= cf.xres)
		canim->x = cf.xres-1;
	if (canim->y >= cf.yres)
		canim->y = cf.yres-1;

	canim->status = 0;

	i = parse_svc_state(t, &canim->type);
	if (i)
		t += i;
	else
		canim->type = e_display;

	/* Find the service name */
	skip_whitespace(&t);
	for (i = 0; t[i] != ' ' && t[i] != '\t' && t[i] != '\0'; i++);
	t[i] = 0;
	i = 0;

	if (canim->type == e_display)
		canim->flags |= F_ANIM_DISPLAY;

	if (canim->type != e_display)
		canim->svc = strdup(t);

	filename = get_filepath(filename);

	canim->mng = mng_load(filename, &canim->w, &canim->h);
	if (!canim->mng) {
		free(filename);
		iprint(MSG_ERROR,"Cannot allocate memory for mng (parse_anim)!\n");
		goto pa_out;
	}

	free(filename);

	cobj = malloc(sizeof(obj));
	if (!cobj) {
		iprint(MSG_ERROR,"Cannot allocate memory (parse_anim)!\n");
		goto pa_out;
	}
	cobj->type = o_anim;
	cobj->p = canim;
	list_add(&objs, cobj);
	list_add(&anims, canim);
	return;
pa_err:
	fprintf(stderr, "parse error @ line %d\n", line);
pa_out:
	free(canim);
	return;
}
#endif /* CONFIG_MNG */

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
	if (cbox->x1 >= cf.xres)
		cbox->x1 = cf.xres-1;
	if (cbox->x2 >= cf.xres)
		cbox->x2 = cf.xres-1;
	if (cbox->y1 >= cf.yres)
		cbox->y1 = cf.yres-1;
	if (cbox->y2 >= cf.yres)
		cbox->y2 = cf.yres-1;

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

char *parse_quoted_string(char *t, u8 keepvar)
{
	char *p, *out;
	int cnt = 0;
	int len = 0;
	int i;

	if (*t != '"')
		return NULL;

	t++;
	p = t;

	while ((*p != '"' || *(p-1) == '\\') && *p != 0) {
		if (*p == '\\') {
			if (!keepvar)
				cnt++;
			p++;
			if (*p == 0)
				break;
		}
		p++;
	}

	if (*p != '"')
		return NULL;

	len = p-t;
	out = malloc(len - cnt + 1);
	if (!out) {
		iprint(MSG_ERROR, "Failed to allocate memory for a quoted string.\n");
		return NULL;
	}

	for (i = 0; i < len; i++, t++) {
		if (*t == '\\' && i < len-1) {
			if (!keepvar) {
				t++;
			} 
		}
		out[i] = t[0];	
	}

	out[len-cnt] = 0;
	return out;
}

#if WANT_TTF
void parse_text(char *t)
{
	char *p, *fontname = NULL, *fpath = NULL;
	int ret, fontsize;
	text *ct = malloc(sizeof(text));
	obj *cobj = NULL;
	item *ti;
	font_e *fe;
	
	if (!ct)
		return;
	
	skip_whitespace(&t);
	ct->flags = 0;
	ct->hotspot = 0;
	ct->style = TTF_STYLE_NORMAL;
	ret = 1;
	
	while (!isdigit(*t) && ret) {
	
		if (!strncmp(t, "silent", 6)) {
			ct->flags |= F_TXT_SILENT;
			t += 6;
			ret = 1;
		} else if (!strncmp(t, "verbose", 7)) {
			ct->flags |= F_TXT_VERBOSE;
			t += 7;
			ret = 1;
		} else {
			ret = 0;
		}

		skip_whitespace(&t);
	}

	if (ct->flags == 0)
		ct->flags = F_TXT_VERBOSE | F_TXT_SILENT;

	if (!isdigit(*t)) {
		p = t;
		skip_nonwhitespace(&p);
		fontname = t;
		*p = 0;
		t = p+1;
	}
	skip_whitespace(&t);		

	/* Parse the style selector */
	while(!isdigit(*t)) {
		if (*t == 'b') {
			ct->style |= TTF_STYLE_BOLD;
		} else if (*t == 'i') {
			ct->style |= TTF_STYLE_ITALIC;
		} else if (*t == 'u') {
			ct->style |= TTF_STYLE_UNDERLINE;
		} else if (*t != ' ' && *t != '\t') {
			goto pt_err;
		}
		t++;
	}
	
	/* Parse font size */
	fontsize = strtol(t,&p,0);
	if (t == p)
		goto pt_err;
	
	t = p; skip_whitespace(&t);

	/* Parse x position */
	ct->x = strtol(t,&p,0);
	if (t == p)
		goto pt_err;
	t = p; skip_whitespace(&t);
	
	if (!isdigit(*t)) {
		if (!strncmp(t, "left", 4)) {
			ct->hotspot |= F_HS_LEFT;
			t += 4;
		} else if (!strncmp(t, "right", 5)) {
			ct->hotspot |= F_HS_RIGHT;
			t += 5;
		} else if (!strncmp(t, "middle", 6)) { 
			ct->hotspot |= F_HS_HMIDDLE;
			t += 6;
		} else {
			goto pt_err;
		}

		skip_whitespace(&t);
	} else {
		ct->hotspot |= F_HS_LEFT;
	}

	/* Parse y position */
	ct->y = strtol(t,&p,0);
	if (t == p)
		goto pt_err;
	t = p; skip_whitespace(&t);

	if (!strncmp(t, "top", 3)) {
		ct->hotspot |= F_HS_TOP;
		t += 3;
	} else if (!strncmp(t, "bottom", 6)) {
		ct->hotspot |= F_HS_BOTTOM;
		t += 6;
	} else if (!strncmp(t, "middle", 6)) { 
		ct->hotspot |= F_HS_VMIDDLE;
		t += 6;
	} else {
		ct->hotspot |= F_HS_TOP;
	}

	skip_whitespace(&t);

	/* Sanity checks */
	if (ct->x >= cf.xres)
		goto pt_err;

	if (ct->x < 0)
		ct->x = 0;

	if (ct->y >= cf.yres)
		goto pt_err;

	if (ct->y < 0)
		ct->y = 0;
	
	zero_color(ct->col);

	if (parse_color(&t, &ct->col)) 
		goto pt_err;

again:
	skip_whitespace(&t);
	if (!strncmp(t, "exec", 4)) {
		ct->flags |= F_TXT_EXEC;
		t += 4;
		goto again;
	} else if (!strncmp(t, "eval", 4)) {
		ct->flags |= F_TXT_EVAL;
		t += 4;
		goto again;
	}


	skip_whitespace(&t);
	ct->val = parse_quoted_string(t, (ct->flags & F_TXT_EVAL) ? 1 : 0);	
	if (!ct->val)
		goto pt_err;

	if (!fontname)
		fontname = DEFAULT_FONT;
	
	fpath = get_fontpath(fontname);
	
	for (ti = fonts.head ; ti != NULL; ti = ti->next) {
		fe = (font_e*) ti->p;

		if (!strcmp(fe->file, fpath) && fe->size == fontsize) {
			ct->font = fe;
			goto pt_end;
		}
	}

	/* Allocate a new entry in the fonts list */
	fe = malloc(sizeof(font_e));	
	if (!fe)
		goto pt_outm;	
	fe->file = fpath;
	fe->size = fontsize;
	fe->font = NULL;
	
	list_add(&fonts, fe);
	ct->font = fe;

pt_end:	cobj = malloc(sizeof(obj));
	if (!cobj) {
pt_outm:	iprint(MSG_ERROR, "Cannot allocate memory (parse_text)!\n");
		goto pt_out;
	}
	cobj->type = o_text;
	cobj->p = ct;
	list_add(&objs, cobj);
	return;

pt_err:
	iprint(MSG_ERROR, "parse error @ line %d\n", line);
pt_out:	free(ct);
	if (fpath)
		free(fpath);
	return;
}
#endif	/* TTF */

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

				case t_fontpath:
					parse_fontpath(t, opts[i]);
					break;

				case t_color:
				{
					if (*t != '=') {
						fprintf(stderr, "parse error @ line %d\n", line);
						break;	
					}
					
					t++;
					skip_whitespace(&t);
					parse_color(&t, opts[i].val);
					break;
				}
					
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

#if defined(CONFIG_MNG) && !defined(TARGET_KERNEL)
				case t_anim:
					parse_anim(t);
					break;
#endif
#if WANT_TTF
				case t_text:
					parse_text(t);
					break;
#endif
				}
			}
		}
	}

#ifndef TARGET_KERNEL
	theme_loaded = 1;
#endif	
	fclose(cfg);
	return 0;
}


