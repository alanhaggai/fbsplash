/*
 * tty.c - support for True Type fonts
 *
 * Splashutils adaptations:
 *   Copyright (C) 2004-2005 Michal Januszewski <spock@gentoo.org>
 *
 * Fbtruetype code:
 *   (w) by stepan@suse.de
 *
 * Original code comes from SDL_ttf.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#ifndef TARGET_KERNEL
#include <math.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <ft2build.h>
#include <freetype/ftoutln.h>
#include <freetype/ttnameid.h>

#include "common.h"
#include "render.h"

int boot_msg_width = 0;

#define DEFAULT_PTSIZE  18
#define NUM_GRAYS       256

#ifdef TARGET_KERNEL
int ceil(float a)
{
	int h = (int)a;
	if (a - h >= 0.5)
		return h+1;
	else
		return h;
}
#endif

static void Flush_Glyph(c_glyph* glyph)
{
	glyph->stored = 0;
	glyph->index = 0;
	if(glyph->bitmap.buffer) {
		free(glyph->bitmap.buffer);
		glyph->bitmap.buffer = 0;
	}
	if(glyph->pixmap.buffer) {
		free(glyph->pixmap.buffer);
		glyph->pixmap.buffer = 0;
	}
	glyph->cached = 0;
}

static void Flush_Cache(TTF_Font* font)
{
	int i;
	int size = sizeof(font->cache) / sizeof(font->cache[0]);

	for(i = 0; i < size; ++i) {
		if(font->cache[i].cached) {
			Flush_Glyph(&font->cache[i]);
		}

	}
	if(font->scratch.cached) {
		Flush_Glyph(&font->scratch);
	}
}

static FT_Error Load_Glyph(TTF_Font* font, unsigned short ch, c_glyph* cached, int want)
{
	FT_Face face;
	FT_Error error;
	FT_GlyphSlot glyph;
	FT_Glyph_Metrics* metrics;
	FT_Outline* outline;

	assert(font);
	assert(font->face);

	face = font->face;

	/* Load the glyph */
	if (! cached->index) {
		cached->index = FT_Get_Char_Index(face, ch);
	}
	error = FT_Load_Glyph(face, cached->index, FT_LOAD_DEFAULT);
	if(error) {
		return error;
	}
	/* Get our glyph shortcuts */
	glyph = face->glyph;
	metrics = &glyph->metrics;
	outline = &glyph->outline;

	/* Get the glyph metrics if desired */
	if ((want & CACHED_METRICS) && !(cached->stored & CACHED_METRICS)) {
		/* Get the bounding box */
		cached->minx = FT_FLOOR(metrics->horiBearingX);
		cached->maxx = cached->minx + FT_CEIL(metrics->width);
		cached->maxy = FT_FLOOR(metrics->horiBearingY);
		cached->miny = cached->maxy - FT_CEIL(metrics->height);
		cached->yoffset = font->ascent - cached->maxy;
		cached->advance = FT_CEIL(metrics->horiAdvance);

		/* Adjust for bold and italic text */
		if(font->style & TTF_STYLE_BOLD) {
			cached->maxx += font->glyph_overhang;
		}
		if(font->style & TTF_STYLE_ITALIC) {
			cached->maxx += (int)ceil(font->glyph_italics);
		}
		cached->stored |= CACHED_METRICS;
	}

	if (((want & CACHED_BITMAP) && !(cached->stored & CACHED_BITMAP)) ||
	     ((want & CACHED_PIXMAP) && !(cached->stored & CACHED_PIXMAP))) {
		int mono = (want & CACHED_BITMAP);
		int i;
		FT_Bitmap* src;
		FT_Bitmap* dst;

		/* Handle the italic style */
		if(font->style & TTF_STYLE_ITALIC) {
			FT_Matrix shear;

			shear.xx = 1 << 16;
			shear.xy = (int) (font->glyph_italics * (1 << 16))/ font->height;
			shear.yx = 0;
			shear.yy = 1 << 16;

			FT_Outline_Transform(outline, &shear);
		}

		/* Render the glyph */
		if (mono) {
			error = FT_Render_Glyph(glyph, ft_render_mode_mono);
		} else {
			error = FT_Render_Glyph(glyph, ft_render_mode_normal);
		}
		if(error) {
			return error;
		}

		/* Copy over information to cache */
		src = &glyph->bitmap;
		if (mono) {
			dst = &cached->bitmap;
		} else {
			dst = &cached->pixmap;
		}
		memcpy(dst, src, sizeof(*dst));
		if (mono) {
			dst->pitch *= 8;
		}

		/* Adjust for bold and italic text */
		if(font->style & TTF_STYLE_BOLD) {
			int bump = font->glyph_overhang;
			dst->pitch += bump;
			dst->width += bump;
		}
		if(font->style & TTF_STYLE_ITALIC) {
			int bump = (int)ceil(font->glyph_italics);
			dst->pitch += bump;
			dst->width += bump;
		}

		if (dst->rows != 0) {
			dst->buffer = malloc(dst->pitch * dst->rows);
			if(!dst->buffer) {
				return FT_Err_Out_Of_Memory;
			}
			memset(dst->buffer, 0, dst->pitch * dst->rows);

			for(i = 0; i < src->rows; i++) {
				int soffset = i * src->pitch;
				int doffset = i * dst->pitch;
				if (mono) {
					unsigned char *srcp = src->buffer + soffset;
					unsigned char *dstp = dst->buffer + doffset;
					int j;
					for (j = 0; j < src->width; j += 8) {
						unsigned char ch = *srcp++;
						*dstp++ = (ch&0x80) >> 7;
						ch <<= 1;
						*dstp++ = (ch&0x80) >> 7;
						ch <<= 1;
						*dstp++ = (ch&0x80) >> 7;
						ch <<= 1;
						*dstp++ = (ch&0x80) >> 7;
						ch <<= 1;
						*dstp++ = (ch&0x80) >> 7;
						ch <<= 1;
						*dstp++ = (ch&0x80) >> 7;
						ch <<= 1;
						*dstp++ = (ch&0x80) >> 7;
						ch <<= 1;
						*dstp++ = (ch&0x80) >> 7;
					}
				} else {
					memcpy(dst->buffer+doffset,
					       src->buffer+soffset,src->pitch);
				}
			}
		}

		/* Handle the bold style */
		if (font->style & TTF_STYLE_BOLD) {
			int row;
			int col;
			int offset;
			int pixel;
			unsigned char* pixmap;

			/* The pixmap is a little hard, we have to add and clamp */
			for(row = dst->rows - 1; row >= 0; --row) {
				pixmap = (unsigned char*) dst->buffer + row * dst->pitch;
				for(offset=1; offset <= font->glyph_overhang; ++offset) {
					for(col = dst->width - 1; col > 0; --col) {
						pixel = (pixmap[col] + pixmap[col-1]);
						if(pixel > NUM_GRAYS - 1) {
							pixel = NUM_GRAYS - 1;
						}
						pixmap[col] = (unsigned char) pixel;
					}
				}
			}
		}

		/* Mark that we rendered this format */
		if (mono) {
			cached->stored |= CACHED_BITMAP;
		} else {
			cached->stored |= CACHED_PIXMAP;
		}
	}

	/* We're done, mark this glyph cached */
	cached->cached = ch;

	return 0;
}

static FT_Error Find_Glyph(TTF_Font* font, unsigned short ch, int want)
{
	int retval = 0;

	if(ch < 256) {
		font->current = &font->cache[ch];
	} else {
		if (font->scratch.cached != ch) {
			Flush_Glyph(&font->scratch);
		}
		font->current = &font->scratch;
	}
	if ((font->current->stored & want) != want) {
		retval = Load_Glyph(font, ch, font->current, want);
	}
	return retval;
}

static unsigned short *UTF8_to_UNICODE(unsigned short *unicode, const char *utf8, int len)
{
	int i, j;
	unsigned short ch;

	for (i=0, j=0; i < len; ++i, ++j) {
		ch = ((const unsigned char *)utf8)[i];
		if (ch >= 0xF0) {
			ch  =  (unsigned short)(utf8[i]&0x07) << 18;
			ch |=  (unsigned short)(utf8[++i]&0x3F) << 12;
			ch |=  (unsigned short)(utf8[++i]&0x3F) << 6;
			ch |=  (unsigned short)(utf8[++i]&0x3F);
		} else
		if (ch >= 0xE0) {
			ch  =  (unsigned short)(utf8[i]&0x3F) << 12;
			ch |=  (unsigned short)(utf8[++i]&0x3F) << 6;
			ch |=  (unsigned short)(utf8[++i]&0x3F);
		} else
		if (ch >= 0xC0) {
			ch  =  (unsigned short)(utf8[i]&0x3F) << 6;
			ch |=  (unsigned short)(utf8[++i]&0x3F);
		}
		unicode[j] = ch;
	}
	unicode[j] = 0;

	return unicode;
}

/* TTF stuff */

static FT_Library library;
static int TTF_initialized = 0;

int TTF_Init(void)
{
	int status;
	FT_Error error;

	status = 0;
	error = FT_Init_FreeType(&library);
	if (error) {
		iprint(MSG_ERROR, "Couldn't init FreeType engine %d\n", error);
		status = -1;
	} else {
		TTF_initialized = 1;
	}
	return status;
}

void TTF_Quit(void)
{
	if (TTF_initialized) {
		FT_Done_FreeType(library);
	}
	TTF_initialized = 0;
}

static int TTF_SizeUNICODE(TTF_Font *font, const unsigned short *text, int *w, int *h)
{
	int status;
	const unsigned short *ch;
	int x, z;
	int minx, maxx;
	int miny, maxy;
	c_glyph *glyph;
	FT_Error error;

	/* Initialize everything to 0 */
	if (! TTF_initialized) {
		return -1;
	}
	status = 0;
	minx = maxx = 0;
	miny = maxy = 0;

	/* Load each character and sum it's bounding box */
	x= 0;
	for (ch=text; *ch; ++ch) {
		error = Find_Glyph(font, *ch, CACHED_METRICS);
		if (error) {
			return -1;
		}
		glyph = font->current;

		z = x + glyph->minx;
		if (minx > z) {
			minx = z;
		}
		if (font->style & TTF_STYLE_BOLD) {
			x += font->glyph_overhang;
		}
		if (glyph->advance > glyph->maxx) {
			z = x + glyph->advance;
		} else {
			z = x + glyph->maxx;
		}
		if (maxx < z) {
			maxx = z;
		}
		x += glyph->advance;

		if (glyph->miny < miny) {
			miny = glyph->miny;
		}
		if (glyph->maxy > maxy) {
			maxy = glyph->maxy;
		}
	}

	/* Fill the bounds rectangle */
	if (w) {
		*w = (maxx - minx);
	}
	if (h)
		*h = font->height;

	return status;
}

static void TTF_RenderUNICODE_Shaded(stheme_t *theme, u8 *target, const unsigned short *text,
			      TTF_Font* font, int x, int y, color fcol, rect *re)
{
	int xstart, width, height, i, j, row_underline;
	unsigned int val;
	const unsigned short* ch;
	unsigned char* src;
	unsigned char* dst;
	int row, col;
	c_glyph *glyph;
	FT_Error error;

	/* Get the dimensions of the text surface */
	if ((TTF_SizeUNICODE(font, text, &width, NULL) < 0) || !width) {
		iprint(MSG_ERROR, "Text has zero width.\n");
		return;
	}
	height = font->height;

	/*
	 * The underline stuff below is a little hackish. The characters
	 * that are being rendered do not form a continuous rectangle, and
	 * we want for the underline to be continuous and span below the
	 * whole text. To achieve that, while rendering each character,
	 * we have to not only paint the part of the underline that is
	 * directly below it, but also the part that 'links' it to the next
	 * character. Thus all the (font->style & TTF_STYLE_UNDERLINE) ? .. : ..
	 * code.
	 */
	row_underline = font->ascent - font->underline_offset - 1;
	if (row_underline >= height) {
		row_underline = (height-1) - font->underline_height;
	}

	/* Load and render each character */
	xstart = 0;
	for(ch = text; *ch; ++ch) {
		FT_Bitmap* current;
		rect tre;

		error = Find_Glyph(font, *ch, CACHED_METRICS|CACHED_PIXMAP);
		if (error)
			return;
		glyph = font->current;

		tre.x1 = xstart + x;
		tre.x2 = tre.x1 + glyph->advance;
		tre.y1 = y;
		tre.y2 = y + height;

		if (tre.x1 < 0)
			goto next_glyph;

		/* TODO: optimize rect-based rendering (?) */
		if (!rect_intersect(re, &tre))
			goto next_glyph;

		current = &glyph->pixmap;
		for(row = 0; row < ((font->style & TTF_STYLE_UNDERLINE) ? height-glyph->yoffset : current->rows); ++row) {
			int add;
			u8 *memlimit = target + theme->xres * theme->yres * fbd.bytespp;

			/* Sanity checks.. */
			i = y + row + glyph->yoffset;
			j = tre.x1;

			if (i < re->y1 || i > re->y2)
				continue;

			if (j >= theme->xres)
				continue;

			if (font->style & TTF_STYLE_UNDERLINE && glyph->minx > 0) {
				j -= glyph->minx;
			}

			dst = (unsigned char *)target + (i * theme->xres + j) * fbd.bytespp;
			src = current->buffer + row*current->pitch;

			add = x & 1;
			add ^= (add ^ (row+y)) & 1 ? 1 : 3;

			for (col = ((font->style & TTF_STYLE_UNDERLINE && glyph->minx > 0) ? -glyph->minx : 0);
			     col < ((font->style & TTF_STYLE_UNDERLINE && *(ch+1)) ?
			           current->width + glyph->advance : current->width); col++, j++) {

				if (j < re->x1 || j > re->x2)
					continue;

				if (col + j >= theme->xres-1)
					continue;

				if (dst + fbd.bytespp-1 > memlimit)
					break;

				if (row < current->rows && col < current->width && col >= 0)
					val = *src++;
				else
					val = 0;

				/* Handle underline */
				if (font->style & TTF_STYLE_UNDERLINE && row+glyph->yoffset >= row_underline &&
				    row+glyph->yoffset < row_underline + font->underline_height) {
					val = NUM_GRAYS-1;
				}

				put_pixel(fcol.a*val/255, fcol.r, fcol.g, fcol.b, dst, dst, add);
				dst += fbd.bytespp;
				add ^= 3;
			}
		}

next_glyph:

		xstart += glyph->advance;
		if (font->style & TTF_STYLE_BOLD) {
			xstart += font->glyph_overhang;
		}
	}
	return;
}

static void TTF_CloseFont(TTF_Font* font)
{
	Flush_Cache(font);
	FT_Done_Face(font->face);
	free(font);
}

static void TTF_SetFontStyle(TTF_Font* font, int style)
{
	font->style = style;
	Flush_Cache(font);
}

static TTF_Font* TTF_OpenFontIndex(const char *file, int ptsize, long index)
{
	TTF_Font* font;
	FT_Error error;
	FT_Face face;
	FT_Fixed scale;

	font = (TTF_Font*) malloc(sizeof *font);
	if (font == NULL) {
		iprint(MSG_ERROR, "Out of memory\n");
		return NULL;
	}
	memset(font, 0, sizeof(*font));

	/* Open the font and create ancillary data */
	error = FT_New_Face(library, file, 0, &font->face);

	if (error)
		error = FT_New_Face(library, TTF_DEFAULT, 0, &font->face);

//	if (error && !strict_font)
//		error=FT_New_Memory_Face(library, (const FT_Byte*)luxisri_ttf, LUXISRI_SIZE, 0, &font->face);

	if (error) {
		iprint(MSG_ERROR, "Couldn't load font file\n");
		free(font);
		return NULL;
	}

	if (index != 0) {
		if (font->face->num_faces > index) {
			FT_Done_Face(font->face);
			error = FT_New_Face(library, file, index, &font->face);
			if (error) {
				iprint(MSG_ERROR, "Couldn't get font face\n");
				free(font);
				return NULL;
			}
		} else {
			iprint(MSG_ERROR, "No such font face\n");
			free(font);
			return NULL;
		}
	}
	face = font->face;

	/* Make sure that our font face is scalable (global metrics) */
	if (! FT_IS_SCALABLE(face)) {
		iprint(MSG_ERROR, "Font face is not scalable\n");
		TTF_CloseFont(font);
		return NULL;
	}

	/* Set the character size and use default DPI (72) */
	error = FT_Set_Char_Size(font->face, 0, ptsize * 64, 0, 0);
	if (error) {
		iprint(MSG_ERROR, "Couldn't set font size\n");
		TTF_CloseFont(font);
		return NULL;
	}

	/* Get the scalable font metrics for this font */
	scale = face->size->metrics.y_scale;
	font->ascent  = FT_CEIL(FT_MulFix(face->bbox.yMax, scale));
	font->descent = FT_CEIL(FT_MulFix(face->bbox.yMin, scale));
	font->height  = font->ascent - font->descent + /* baseline */ 1;
	font->lineskip = FT_CEIL(FT_MulFix(face->height, scale));
	font->underline_offset = FT_FLOOR(FT_MulFix(face->underline_position, scale));
	font->underline_height = FT_FLOOR(FT_MulFix(face->underline_thickness, scale));
	if (font->underline_height < 1) {
		font->underline_height = 1;
	}

	/* Set the default font style */
	font->style = TTF_STYLE_NORMAL;
	font->glyph_overhang = face->size->metrics.y_ppem / 10;
	/* x offset = cos(((90.0-12)/360)*2*M_PI), or 12 degree angle */
	font->glyph_italics = 0.207f;
	font->glyph_italics *= font->height;

	return font;
}

static TTF_Font* TTF_OpenFont(const char *file, int ptsize)
{
	TTF_Font *a;

	a = TTF_OpenFontIndex(file, ptsize, 0);

	if (a == NULL) {
		iprint(MSG_ERROR, "Couldn't load %d pt font from %s\n", ptsize, file);
	}

	return a;
}

int load_fonts(stheme_t *theme)
{
	item *i;

	for (i = theme->fonts.head; i != NULL; i = i->next) {
		font_e *fe = (font_e*) i->p;
		if (!fe->font) {
			fe->font = TTF_OpenFont(fe->file, fe->size);
		}
	}

	return 0;
}

int free_fonts(stheme_t *theme)
{
	item *i, *j;

	for (i = theme->fonts.head; i != NULL;) {
		font_e *fe = (font_e*) i->p;
		j = i->next;

		if (fe->font)
			TTF_CloseFont(fe->font);

		if (fe->file)
			free(fe->file);

		free(fe);
		free(i);
		i = j;
	}

	return 0;
}

static char *text_get_output(char *prg)
{
	char *buf = malloc(1024);
	fd_set rfds;
	struct timeval tv;
	int pfds[2];
	pid_t pid;
	int i;

	if (!buf)
		return NULL;

	pipe(pfds);
	pid = fork();
	buf[0] = 0;

	if (pid == 0) {
#ifndef TARGET_KERNEL
		/* Only play with stdout if we are NOT the kernel helper.
		 * Otherwise, things will break horribly and we'll end up
		 * with a deadlock. */
		close(1);
#endif
		dup(pfds[1]);
		close(pfds[0]);
		execlp("sh", "sh", "-c", prg, NULL);
	} else {
		FD_ZERO(&rfds);
		FD_SET(pfds[0], &rfds);
		tv.tv_sec = 0;
		tv.tv_usec = 250000;
		i = select(pfds[0]+1, &rfds, NULL, NULL, &tv);
		if (i != -1 && i != 0) {
			i = read(pfds[0], buf, 1024);
			if (i > 0)
				buf[i] = 0;
		}

		close(pfds[0]);
		close(pfds[1]);
	}

	return buf;
}

static char *text_eval(char *txt)
{
	char *p, *t, *ret, *d;
	int len, i;

	i = len = strlen(txt);
	p = txt;

	while ((t = strstr(p, "$progress")) != NULL) {
		len += 3;
		p = t+1;
	}

	ret = malloc(len+1);

	p = txt;
	d = ret;

	while (*p != 0) {
		if (*p == '\\') {
			/* to allow literal "$progress" i.e. \$progress */
			p++;

			/* might have reached end of string */
			if (*p == 0)
				break;

			if (*p == 'n')
				*d = '\n';
			else
				*d = *p;
			p++;
			d++;
			continue;
		}

		*d = *p;

		if (!strncmp(p, "$progress", 9)) {
			d += sprintf(d, "%d", config.progress * 100 / FBSPL_PROGRESS_MAX);
			p += 9;
		} else {
			p++;
			d++;
		}
	}

	*d = 0; /* NULL-terminate */

	return ret;
}

void text_render(stheme_t *theme, text *ct, rect *re, u8 *target)
{
	u16 *t, *p;
	obj *o = container_of(ct);
	int x, y;

	if (!target || !ct || !ct->font || !ct->font->font)
		return;

	x = o->bnd.x1;
	y = o->bnd.y1;

	TTF_SetFontStyle(ct->font->font, ct->style);

	/* Render the unicode text line by line. */
	for (t = p = ct->cache; *p != 0; p++) {
		if (*p == '\n') {
			*p = 0;
			if (p > t)
				TTF_RenderUNICODE_Shaded(theme, target, t, ct->font->font, x, y, ct->col, re);
			y += ct->font->font->height;
			*p = '\n';
			t = p+1;
		}
	}

	if (*t != 0) {
		TTF_RenderUNICODE_Shaded(theme, target, t, ct->font->font, x, y, ct->col, re);
	}
}

static void text_get_xy(text *ct, int *x, int *y)
{
	int t;
	*x = ct->x;
	*y = ct->y;
	t = ct->hotspot & F_HS_HORIZ_MASK;
	if (t == F_HS_HMIDDLE)
		*x -= ct->last_width/2;
	else if (t == F_HS_RIGHT)
		*x -= ct->last_width;

	t = ct->hotspot & F_HS_VERT_MASK;
	if (t == F_HS_VMIDDLE)
		*y -= ct->font->font->height/2;
	else if (t == F_HS_BOTTOM)
		*y -= ct->font->font->height;
	return;
}

void text_bnd(stheme_t *theme, text *ct, rect *bnd)
{
	obj *o;
	char *txt = NULL, *txt2;
	u16 *p;
	int unicode_len;
	int lines = 1;

	o = container_of(ct);

	if (!ct->font || !ct->font->font)
		return;

	if (ct->cache)
		free(ct->cache);

	if (ct->flags & F_TXT_EXEC) {
		txt = text_get_output(ct->val);
	}

	if (ct->flags & F_TXT_EVAL) {
		if (txt) {
			txt2 = text_eval(txt);
			free(txt);
			txt = txt2;
		} else {
			txt = text_eval(ct->val);
		}
	}

	if (!txt)
		txt = ct->val;

	if (ct->curr_progress >= 0)
		ct->curr_progress = config.progress;

	/* Copy the Latin-1 text to a UNICODE text buffer */
	unicode_len = strlen(txt);
	ct->cache = (u16 *)malloc((unicode_len+1) * sizeof(*ct->cache));

	if (ct->cache == NULL) {
		iprint(MSG_ERROR, "Out of memory.\n");
		return;
	}

	UTF8_to_UNICODE(ct->cache, txt, unicode_len);
	if (txt != ct->val)
		free(txt);
	TTF_SetFontStyle(ct->font->font, ct->style);

	text_get_xy(ct, &bnd->x1, &bnd->y1);

	/* Get the dimensions of the text surface */
	if ((TTF_SizeUNICODE(ct->font->font, ct->cache, &bnd->x2, NULL) < 0) || !bnd->x2) {
		iprint(MSG_ERROR, "Text has zero width.\n");
		return;
	}
	bnd->x2 += bnd->x1 - 1;

	for (p = ct->cache; *p; p++) {
		if (*p == '\n')
			lines++;
	}

	bnd->y2 = bnd->y1 + ct->font->font->height * lines - 1;
}

void text_prerender(stheme_t *theme, text *ct, bool force)
{
	rect bnd;
	obj *o = container_of(ct);

	if (ct->curr_progress == config.progress && !force)
		return;

	text_bnd(theme, ct, &bnd);

	blit_add(theme, &bnd);
	render_add(theme, o, &bnd);

	blit_add(theme, &o->bnd);
	render_add(theme, o, &o->bnd);

	/* TODO: compare w/ the old bounding rectangle here, reallocate
	 * the background buffer if necessary */

	memcpy(&o->bnd, &bnd, sizeof(rect));
}
