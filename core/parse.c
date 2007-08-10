/*
 * parse.c - Functions for parsing splashutils cfg files
 *
 * Copyright (C) 2004-2007, Michal Januszewski <spock@gentoo.org>
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

struct cfg_opt {
	char *name;
	enum {
		t_int, t_path, t_box, t_icon, t_rect, t_color, t_fontpath,
		t_type_open, t_type_close, t_anim, t_text,
	} type;
	void *val;
};

int line = 0;

u16 text_x, text_y;
u16 text_size;
color text_color;
char *text_font;

stheme_t tmptheme;

/* Note that pic256 and silentpic256 have to be located before pic and
 * silentpic or we are gonna get a parse error @ pic256/silentpic256. */
struct cfg_opt opts[] =
{
	{	.name = "jpeg",
		.type = t_path,
		.val = &tmptheme.pic	},

	{	.name = "pic256",
		.type = t_path,
		.val = &tmptheme.pic256	},

	{	.name = "silentpic256",
		.type = t_path,
		.val = &tmptheme.silentpic256	},

	{	.name = "silentjpeg",
		.type = t_path,
		.val = &tmptheme.silentpic	},

	{	.name = "pic",
		.type = t_path,
		.val = &tmptheme.pic		},

	{	.name = "silentpic",
		.type = t_path,
		.val = &tmptheme.silentpic	},

	{	.name = "bg_color",
		.type = t_int,
		.val = &tmptheme.bg_color	},

	{	.name = "tx",
		.type = t_int,
		.val = &tmptheme.tx		},

	{	.name = "ty",
		.type = t_int,
		.val = &tmptheme.ty		},

	{	.name = "tw",
		.type = t_int,
		.val = &tmptheme.tw		},

	{	.name = "th",
		.type = t_int,
		.val = &tmptheme.th		},

	{	.name = "box",
		.type = t_box,
		.val = NULL		},

	{	.name = "icon",
		.type = t_icon,
		.val = NULL		},

	{	.name = "rect",
		.type = t_rect,
		.val = NULL		},

	{	.name = "<type",
		.type = t_type_open,
		.val = NULL		},

	{	.name = "</type>",
		.type = t_type_close,
		.val = NULL		},

#if WANT_MNG
	{	.name = "anim",
		.type = t_anim,
		.val = NULL		},
#endif
#if WANT_TTF
	{	.name = "text_x",
		.type = t_int,
		.val = &text_x	},

	{	.name = "text_y",
		.type = t_int,
		.val = &text_y	},

	{	.name = "text_size",
		.type = t_int,
		.val = &text_size	},

	{	.name = "text_color",
		.type = t_color,
		.val = &text_color	},

	{	.name = "text_font",
		.type = t_fontpath,
		.val = &text_font	},

	{	.name = "text",
		.type = t_text,
		.val = NULL		},
#endif /* TTF */
};


#define parse_error(msg, args...)											\
do {																		\
	iprint(MSG_ERROR, "Parse error at line %d: " msg "\n", line, ## args);	\
} while (0)

static char *get_filepath(char *path)
{
	char buf[512];

	if (path[0] == '/')
		return strdup(path);

	snprintf(buf, 512, SPL_THEME_DIR "/%s/%s", config.theme, path);
	return strdup(buf);
}

static int ishexdigit(char c)
{
	return (isdigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) ? 1 : 0;
}

static void skip_whitespace(char **buf)
{
	while (**buf == ' ' || **buf == '\t')
		(*buf)++;

	return;
}

static void skip_nonwhitespace(char **buf)
{
	while (**buf != ' ' && **buf != '\t')
		(*buf)++;

	return;
}

static void parse_int(char *t, struct cfg_opt opt)
{
	if (*t != '=') {
		parse_error("expected '=' instead of '%c'", *t);
		return;
	}

	t++; skip_whitespace(&t);
	*(u16*)opt.val = strtol(t,NULL,0);
}

static void parse_path(char *t, struct cfg_opt opt)
{
	if (*t != '=') {
		parse_error("expected '=' instead of'%c'", *t);
		return;
	}

	t++; skip_whitespace(&t);
	*(char**)opt.val = get_filepath(t);
}

static char *get_fontpath(char *t)
{
	char buf[512];
	char buf2[512];
	struct stat st1, st2;

	if (t[0] == '/') {
		return strdup(t);
	}

	snprintf(buf, 512, "%s/%s/%s", SPL_THEME_DIR, config.theme, t);
	snprintf(buf2, 512, "%s/%s", SPL_THEME_DIR, t);

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

static void parse_fontpath(char *t, struct cfg_opt opt)
{
	if (*t != '=') {
		parse_error("expected '=' instead of '%c'", *t);
		return;
	}

	t++; skip_whitespace(&t);
	*(char**)opt.val = get_fontpath(t);
}

static int parse_color(char **t, struct color *cl)
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

static int parse_4vec(char **t, rect *r)
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

void obj_add(void *x) {
	obj *o = container_of(x);

	if (tmptheme.objs.tail)
		o->id = ((obj*)(tmptheme.objs.tail->p))->id + 1;
	else
		o->id =0;

	list_add(&tmptheme.objs, o);
}

static void parse_icon(char *t)
{
	char *filename = NULL;
	char *p;
	obj *o;
	icon *cic = obj_alloc(icon, SPL_MODE_SILENT);
	icon_img *cim;
	item *ti;
	int i;

	if (!cic) {
		iprint(MSG_ERROR, "%s: failed to allocate memory.\n", __func__);
		return;
	}

	o = container_of(cic);
	cic->svc = NULL;

	skip_whitespace(&t);
	for (i = 0; t[i] != ' ' && t[i] != '\t' && t[i] != '\0'; i++);
	t[i] = 0;

	filename = get_filepath(t);
	t += (i+1);

	skip_whitespace(&t);
	cic->x = strtol(t,&p,0);
	if (t == p) {
		parse_error("expected a number instead of '%s'", t);
		goto pi_err;
	}

	t = p; skip_whitespace(&t);
	cic->y = strtol(t,&p,0);
	if (t == p) {
		parse_error("expected a number instead of '%s'", t);
		goto pi_err;

	}
	t = p; skip_whitespace(&t);

	/* Do we need to crop this icon? */
	if (!strncmp(t, "crop", 4)) {
		t += 4;
		skip_whitespace(&t);

		if (parse_4vec(&t, &cic->crop_from)) {
			parse_error("expected a 4-tuple instead of '%s'", t);
			goto pi_err;
		}

		skip_whitespace(&t);

		if (parse_4vec(&t, &cic->crop_to)) {
			parse_error("expected a 4-tuple instead of '%s'", t);
			goto pi_err;
		}

		skip_whitespace(&t);
		cic->crop = true;
		rect_interpolate(&cic->crop_from, &cic->crop_to, &cic->crop_curr);
	} else {
		cic->crop = false;
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

	if (cic->type != e_display) {
		o->visible = false;
		cic->svc = strdup(t);
	}

	for (ti = tmptheme.icons.head ; ti != NULL; ti = ti->next) {
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
	list_add(&tmptheme.icons, cim);
	cic->img = cim;

pi_end:
	obj_add(cic);
	return;

pi_err:
pi_out:
	if (filename)
		free(filename);
	if (cic->svc)
		free(cic->svc);
	free(container_of(cic));
	return;
pi_outm:
	iprint(MSG_ERROR, "%s: failed to allocate memory\n", __func__);
	goto pi_out;
}

static void parse_rect(char *t)
{
	char *p;
	rect *crect = malloc(sizeof(rect));

	if (!crect) {
		iprint(MSG_ERROR, "%s: failed to allocate memory\n", __func__);
		return;
	}

	skip_whitespace(&t);

	while (!isdigit(*t)) {
		t++;
	}

	crect->x1 = strtol(t,&p,0);
	if (t == p) {
		parse_error("expected a number instead of '%s'", t);
		goto pr_err;
	}
	t = p; skip_whitespace(&t);

	crect->y1 = strtol(t,&p,0);
	if (t == p) {
		parse_error("expected a number instead of '%s'", t);
		goto pr_err;
	}
	t = p; skip_whitespace(&t);

	crect->x2 = strtol(t,&p,0);
	if (t == p) {
		parse_error("expected a number instead of '%s'", t);
		goto pr_err;
	}
	t = p; skip_whitespace(&t);

	crect->y2 = strtol(t,&p,0);
	if (t == p) {
		parse_error("expected a number instead of '%s'", t);
		goto pr_err;
	}
	t = p; skip_whitespace(&t);

	/* sanity checks */
	if (crect->x1 >= tmptheme.xres)
		crect->x1 = tmptheme.xres-1;
	if (crect->x2 >= tmptheme.xres)
		crect->x2 = tmptheme.xres-1;
	if (crect->y1 >= tmptheme.yres)
		crect->y1 = tmptheme.yres-1;
	if (crect->y2 >= tmptheme.yres)
		crect->y2 = tmptheme.yres-1;

	list_add(&tmptheme.rects, crect);
	return;
pr_err:
	free(crect);
	return;
}

#if WANT_MNG
static void parse_anim(char *t)
{
	char *p;
	char *filename;
	anim *canim = obj_alloc(anim, SPL_MODE_SILENT);
	obj *o;
	int i;

	if (!canim) {
		iprint(MSG_ERROR, "%s: failed to allocate memory\n", __func__);
		return;
	}

	o = container_of(canim);
	obj_setmode(container_of(canim), SPL_MODE_SILENT);

	skip_whitespace(&t);
	canim->flags = 0;

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
		parse_error("an anim has to be flagged with one of the following:\n"
					"'once', 'loop' or 'proportional' (got '%s' instead)", t);
		goto pa_err;
	}

	skip_whitespace(&t);

	filename = t;
	skip_nonwhitespace(&t);
	*t = '\0';
	t++;

	skip_whitespace(&t);

	canim->x = strtol(t,&p,0);
	if (t == p) {
		parse_error("expected a number instead of '%s'", t);
		goto pa_err;
	}
	t = p; skip_whitespace(&t);

	canim->y = strtol(t,&p,0);
	if (t == p) {
		parse_error("expected a number instead of '%s'", t);
		goto pa_err;
	}
	t = p; skip_whitespace(&t);

	/* sanity checks */
	if (canim->x >= tmptheme.xres)
		canim->x = tmptheme.xres-1;
	if (canim->y >= tmptheme.yres)
		canim->y = tmptheme.yres-1;

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

	if (canim->type != e_display) {
		canim->svc = strdup(t);
		o->visible = false;
	}

	filename = get_filepath(filename);

	canim->mng = mng_load(filename, &canim->w, &canim->h);
	if (!canim->mng) {
		free(filename);
		iprint(MSG_ERROR, "%s: failed to allocate memory for mng\n", __func__);
		goto pa_out;
	}

	free(filename);

	list_add(&tmptheme.anims, canim);
	obj_add(canim);
	return;
pa_err:
pa_out:
	free(container_of(canim));
	return;
}
#endif /* CONFIG_MNG */

static box* parse_box(char *t)
{
	char *p;
	int ret;
	box *cbox = obj_alloc(box, SPL_MODE_VERBOSE);

	if (!cbox)
		return NULL;

	skip_whitespace(&t);
	cbox->attr = 0;
	cbox->curr = NULL;

	while (!isdigit(*t)) {
		if (!strncmp(t,"noover",6)) {
			cbox->attr |= BOX_NOOVER;
			t += 6;
		} else if (!strncmp(t, "inter", 5)) {
			cbox->attr |= BOX_INTER;
			t += 5;
		} else if (!strncmp(t, "silent", 6)) {
			obj_setmode(container_of(cbox), SPL_MODE_SILENT);
			t += 6;
		} else {
			parse_error("expected 'noover', 'inter' or 'silent' instead of '%s'", t);
			goto pb_err;
		}

		skip_whitespace(&t);
	}

	cbox->re.x1 = strtol(t,&p,0);
	if (t == p) {
		parse_error("expected a number instead of '%s'", t);
		goto pb_err;
	}
	t = p; skip_whitespace(&t);

	cbox->re.y1 = strtol(t,&p,0);
	if (t == p) {
		parse_error("expected a number instead of '%s'", t);
		goto pb_err;
	}
	t = p; skip_whitespace(&t);

	cbox->re.x2 = strtol(t,&p,0);
	if (t == p) {
		parse_error("expected a number instead of '%s'", t);
		goto pb_err;
	}
	t = p; skip_whitespace(&t);

	cbox->re.y2 = strtol(t,&p,0);
	if (t == p) {
		parse_error("expected a number instead of '%s'", t);
		goto pb_err;
	}
	t = p; skip_whitespace(&t);

	/* sanity checks */
	if (cbox->re.x1 >= tmptheme.xres)
		cbox->re.x1 = tmptheme.xres-1;
	if (cbox->re.x2 >= tmptheme.xres)
		cbox->re.x2 = tmptheme.xres-1;
	if (cbox->re.y1 >= tmptheme.yres)
		cbox->re.y1 = tmptheme.yres-1;
	if (cbox->re.y2 >= tmptheme.yres)
		cbox->re.y2 = tmptheme.yres-1;

#define zero_color(cl) *(u32*)(&cl) = 0;
#define is_zero_color(cl) (*(u32*)(&cl) == 0)
#define assign_color(c1, c2) *(u32*)(&c1) = *(u32*)(&c2);

	zero_color(cbox->c_ul);
	zero_color(cbox->c_ur);
	zero_color(cbox->c_ll);
	zero_color(cbox->c_lr);

	if (parse_color(&t, &cbox->c_ul)) {
		parse_error("expected a color instead of '%s'", t);
		goto pb_err;
	}

	skip_whitespace(&t);

	ret = parse_color(&t, &cbox->c_ur);

	if (ret == -1) {
		assign_color(cbox->c_ur, cbox->c_ul);
		assign_color(cbox->c_lr, cbox->c_ul);
		assign_color(cbox->c_ll, cbox->c_ul);
		goto pb_end;
	} else if (ret == -2) {
		parse_error("failed to parse color");
		goto pb_err;
	}

	skip_whitespace(&t);

	if (parse_color(&t, &cbox->c_ll)) {
		parse_error("expected a color instead of '%s'", t);
		goto pb_err;
	}

	skip_whitespace(&t);

	if (parse_color(&t, &cbox->c_lr)) {
		parse_error("expected a color instead of '%s'", t);
		goto pb_err;
	}
pb_end:
	return cbox;

pb_err:
	free(container_of(cbox));
	return NULL;
}

static char *parse_quoted_string(char *t, u8 keepvar)
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
static void parse_text(char *t)
{
	char *p, *fontname = NULL, *fpath = NULL;
	int ret, fontsize;
	text *ct = obj_alloc(text, 0);
	item *ti;
	font_e *fe;
	int mode = 0;

	if (!ct) {
		iprint(MSG_ERROR, "%s: failed to allocate memory\n", __func__);
		return;
	}

	skip_whitespace(&t);
	ct->flags = 0;
	ct->hotspot = 0;
	ct->style = TTF_STYLE_NORMAL;
	ret = 1;

	while (!isdigit(*t) && ret) {
		if (!strncmp(t, "silent", 6)) {
			mode |= SPL_MODE_SILENT;
			t += 6;
			ret = 1;
		} else if (!strncmp(t, "verbose", 7)) {
			mode |= SPL_MODE_VERBOSE;
			t += 7;
			ret = 1;
		} else {
			ret = 0;
		}

		skip_whitespace(&t);
	}

	if (!mode)
		mode = SPL_MODE_SILENT | SPL_MODE_VERBOSE;

	obj_setmode(container_of(ct), mode);

	if (!isdigit(*t)) {
		p = t;
		skip_nonwhitespace(&p);
		fontname = t;
		*p = 0;
		t = p+1;
	}
	skip_whitespace(&t);

	/* Parse the style selector */
	while (!isdigit(*t)) {
		if (*t == 'b') {
			ct->style |= TTF_STYLE_BOLD;
		} else if (*t == 'i') {
			ct->style |= TTF_STYLE_ITALIC;
		} else if (*t == 'u') {
			ct->style |= TTF_STYLE_UNDERLINE;
		} else if (*t != ' ' && *t != '\t') {
			parse_error("expected a style specifier instead of '%s'", t);
			goto pt_err;
		}
		t++;
	}

	/* Parse font size */
	fontsize = strtol(t,&p,0);
	if (t == p) {
		parse_error("expected font size (a number) instead of '%s'", t);
		goto pt_err;
	}

	t = p; skip_whitespace(&t);

	/* Parse x position */
	ct->x = strtol(t,&p,0);
	if (t == p) {
		parse_error("expected x position (a number) instead of '%s'", t);
		goto pt_err;
	}
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
			parse_error("expected 'left', 'right' or 'middle' instead of '%s'", t);
			goto pt_err;
		}

		skip_whitespace(&t);
	} else {
		ct->hotspot |= F_HS_LEFT;
	}

	/* Parse y position */
	ct->y = strtol(t,&p,0);
	if (t == p) {
		parse_error("expected y position (a number) instead of '%s'", t);
		goto pt_err;
	}
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
	if (ct->x >= tmptheme.xres) {
		parse_error("the x position is invalid (larger than x resolution)");
		goto pt_err;
	}

	if (ct->x < 0)
		ct->x = 0;

	if (ct->y >= tmptheme.yres) {
		parse_error("the y position is invalid (larger than y resolution)");
		goto pt_err;
	}

	if (ct->y < 0)
		ct->y = 0;

	zero_color(ct->col);

	if (parse_color(&t, &ct->col)) {
		parse_error("expected a color instead of '%s'", t);
		goto pt_err;
	}

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
	if (!ct->val) {
		parse_error("failed to parse a quoted string: '%s'", t);
		goto pt_err;
	}

	if (strstr(ct->val, "$progress")) {
		ct->curr_progress = config.progress;
	} else {
		ct->curr_progress = -1;
	}

	if (!fontname)
		fontname = DEFAULT_FONT;

	fpath = get_fontpath(fontname);

	for (ti = tmptheme.fonts.head ; ti != NULL; ti = ti->next) {
		fe = (font_e*) ti->p;

		if (!strcmp(fe->file, fpath) && fe->size == fontsize) {
			ct->font = fe;
			goto pt_end;
		}
	}

	/* Allocate a new entry in the fonts list */
	fe = malloc(sizeof(font_e));
	if (!fe) {
		iprint(MSG_ERROR, "%s: failed to allocate memory\n", __func__);
		goto pt_err;
	}
	fe->file = fpath;
	fe->size = fontsize;
	fe->font = NULL;

	list_add(&tmptheme.fonts, fe);
	ct->font = fe;

pt_end:
	obj_add(ct);
	return;

pt_err:
	free(container_of(ct));
	if (fpath)
		free(fpath);
	return;
}

void add_main_msg()
{
	char *fpath;
	text *ct = obj_alloc(text, SPL_MODE_SILENT);
	item *ti;
	font_e *fe;

	if (!ct) {
		iprint(MSG_ERROR, "%s: failed to allocate memory\n", __func__);
		return;
	}

	ct->hotspot = F_HS_LEFT | F_HS_TOP;
	ct->style = TTF_STYLE_NORMAL;
	ct->x = text_x;
	ct->y = text_y;
	ct->col = text_color;
	ct->val = strdup(config.message);
	ct->curr_progress = config.progress;
	ct->flags = F_TXT_EVAL;

	fpath = text_font;

	for (ti = tmptheme.fonts.head ; ti != NULL; ti = ti->next) {
		fe = (font_e*) ti->p;

		if (!strcmp(fe->file, fpath) && fe->size == text_size) {
			ct->font = fe;
			goto mm_end;
		}
	}

	/* Allocate a new entry in the fonts list */
	fe = malloc(sizeof(font_e));
	if (!fe) {
		iprint(MSG_ERROR, "%s: failed to allocate memory\n", __func__);
		goto mm_err;
	}
	fe->file = fpath;
	fe->size = text_size;
	fe->font = NULL;

	list_add(&tmptheme.fonts, fe);
	ct->font = fe;

mm_end:
	obj_add(ct);
	return;

mm_err:
	free(container_of(ct));
	return;
}



#endif	/* TTF */

int parse_cfg(char *cfgfile, stheme_t *theme)
{
	FILE* cfgfp;
	char buf[1024];
	char *t;
	int len, i;
	bool ignore = false;
	box *bprev = NULL;

	if ((cfgfp = fopen(cfgfile,"r")) == NULL) {
		iprint(MSG_ERROR, "Can't open cfg file %s.\n", cfgfile);
		return 1;
	}

	memcpy(&tmptheme, theme, sizeof(tmptheme));

	while (fgets(buf, sizeof(buf), cfgfp)) {

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

		for (i = 0; i < sizeof(opts) / sizeof(struct cfg_opt); i++)
		{
			if (!strncmp(opts[i].name, t, strlen(opts[i].name))) {

				if (ignore && opts[i].type != t_type_close)
					continue;

				t += strlen(opts[i].name);
				skip_whitespace(&t);

				switch (opts[i].type) {

				case t_path:
					parse_path(t, opts[i]);
					break;

				case t_fontpath:
					parse_fontpath(t, opts[i]);
					break;

				case t_color:
				{
					if (*t != '=') {
						parse_error("expected '=' instead of '%c'", *t);
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
				{
					box *tbox = parse_box(t);
					if (tbox->attr & BOX_INTER) {
						bprev = tbox;
						goto box_post;
					} else if (bprev != NULL) {
						bprev->inter = tbox;
						bprev->curr = malloc(sizeof(box));
						box_interpolate(bprev, tbox, bprev->curr);
						if (!bprev->curr) {
							free(container_of(tbox));
							free(container_of(bprev));
							iprint(MSG_ERROR, "Failed to allocate cache for an interpolated box.\n");
						} else {
							obj_add(bprev);

							if (!memcmp(&bprev->c_ul, &bprev->c_ur, sizeof(color))) {
								if (!memcmp(&bprev->c_ll, &bprev->c_lr, sizeof(color))) {
									if (!memcmp(&bprev->c_ll, &bprev->c_ul, sizeof(color)))
										bprev->attr |= BOX_SOLID;
									else
										bprev->attr |= BOX_VGRAD;
								}
							} else if (!memcmp(&bprev->c_ul, &bprev->c_ll, sizeof(color)) &&
									   !memcmp(&bprev->c_ur, &bprev->c_lr, sizeof(color))) {
								bprev->attr |= BOX_HGRAD;
							}
						}
						bprev = NULL;
					} else {
						obj_add(tbox);
					}
					break;
				}

				case t_icon:
					parse_icon(t);
					break;

				case t_rect:
					parse_rect(t);
					break;

				case t_type_open:
					ignore = true;
					while (*t != '>') {
						skip_whitespace(&t);
						if (!strncmp(t, "bootup", 6)) {
							if (config.type == spl_bootup)
								ignore = false;
							t += 6;
						} else if (!strncmp(t, "reboot", 6)) {
							if (config.type == spl_reboot)
								ignore = false;
							t += 6;
						} else if (!strncmp(t, "shutdown", 8)) {
							if (config.type == spl_shutdown)
								ignore = false;
							t += 8;
						} else if (!strncmp(t, "other", 5)) {
							if (config.type == spl_undef)
								ignore = false;
							t += 5;
						} else {
							parse_error("expected 'other', 'bootup', 'reboot' or 'shutdown' instead of '%s'", t);
							break;
						}
					}
					break;

				case t_type_close:
					ignore = false;
					break;

#if WANT_MNG
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

		if (bprev) {
			parse_error("an 'inter' box must be directly followed by another box");
			free(container_of(bprev));
			bprev = NULL;
		}

box_post:
		;
	}

#if WANT_TTF
	add_main_msg();
#endif
	memcpy(theme, &tmptheme, sizeof(tmptheme));

	fclose(cfgfp);
	return 0;
}


