/*
 * tty.c - support for True Type fonts
 *
 * Splashutils adaptations:
 *   Copyright (C) 2004-2005 Michael Januszewski <spock@gentoo.org>
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
#include <assert.h>
	
#include <ft2build.h>
#include <freetype/ftoutln.h>
#include <freetype/ttnameid.h>

#include "splash.h"

TTF_Font *global_font;
char *boot_message = NULL;

#define DEFAULT_PTSIZE  18
#define NUM_GRAYS       256

static int alpha = 100;

unsigned char* TTF_RenderUNICODE_Shaded(u8 *target, const unsigned short *text,
					TTF_Font* font, int x, int y, color fcol);


/* Cached glyph information */
typedef struct cached_glyph {
	int stored;
	FT_UInt index;
	FT_Bitmap bitmap;
	FT_Bitmap pixmap;
	int minx;
	int maxx;
	int miny;
	int maxy;
	int yoffset;
	int advance;
	unsigned short cached;
} c_glyph;

struct _TTF_Font {
	/* Freetype2 maintains all sorts of useful info itself */
	FT_Face face;

	/* We'll cache these ourselves */
	int height;
	int ascent;
	int descent;
	int lineskip;

	/* The font style */
	int style;

	/* Extra width in glyph bounds for text styles */
	int glyph_overhang;
	float glyph_italics;

	/* Information in the font for underlining */
	int underline_offset;
	int underline_height;

	/* Cache for style-transformed glyphs */
	c_glyph *current;
	c_glyph cache[256];
	c_glyph scratch;
};


static void Flush_Glyph(c_glyph* glyph);

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

/* character conversion */

/* Macro to convert a character to a Unicode value -- assume already Unicode */
#define UNICODE(c)      c

/*
static unsigned short *ASCII_to_UNICODE(unsigned short *unicode, const char *text, int len)
{
	int i;
		
	for (i=0; i < len; ++i) {
		unicode[i] = ((const unsigned char *)text)[i];
	}
	unicode[i] = 0;
			
	return unicode;
}
*/

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
		fprintf(stderr, "Couldn't init FreeType engine %d\n", error);
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

unsigned char*TTF_RenderText_Shaded(u8 *target, const char *text, TTF_Font *font, int x, int y, color col)
{
	unsigned short *unicode_text;
	int unicode_len;
    
	unsigned short *p, *t;
	
	/* Copy the Latin-1 text to a UNICODE text buffer */
	unicode_len = strlen(text);
	unicode_text = (unsigned short *)malloc((unicode_len+1)*(sizeof*unicode_text));
	
	if (unicode_text == NULL) {
		printf("Out of memory\n");
		return(NULL);
	}	       

	UTF8_to_UNICODE(unicode_text, text, unicode_len);
//	ASCII_to_UNICODE(unicode_text, text, unicode_len);
   
	for (t = p = unicode_text; *p != 0; p++) {
	
		if (*p == '\n') {
			*p = 0;
			if (p > t)
				TTF_RenderUNICODE_Shaded(target, t, font, x, y, col);
			y += font->height;
			t = p+1;
		}
	}

	if (*t != 0) {
		TTF_RenderUNICODE_Shaded(target, t, font, x, y, col);
	}
    
	/* Free the text buffer and return */
	free(unicode_text);
	return NULL;
}

void TTF_CloseFont(TTF_Font* font)
{
	Flush_Cache(font);
	FT_Done_Face(font->face);
	free(font);
}			     

void TTF_SetFontStyle(TTF_Font* font, int style)
{
	font->style = style;
	Flush_Cache(font);
}
    
TTF_Font* TTF_OpenFontIndex(const char *file, int ptsize, long index)
{
	TTF_Font* font;
	FT_Error error;
	FT_Face face;
	FT_Fixed scale;

	font = (TTF_Font*) malloc(sizeof *font);
	if (font == NULL) {
		fprintf(stderr, "Out of memory\n");
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
		printf("Couldn't load font file\n");
		free(font);
		return NULL;
	}

	if (index != 0) {
		if (font->face->num_faces > index) {
			FT_Done_Face(font->face);
			error = FT_New_Face(library, file, index, &font->face);
			if(error) {
				printf("Couldn't get font face\n");
				free(font);
				return NULL;
			}
		} else {
			fprintf(stderr, "No such font face\n");
			free(font);
			return NULL;
		}
	}
	face = font->face;

	/* Make sure that our font face is scalable (global metrics) */
	if (! FT_IS_SCALABLE(face)) {
		fprintf(stderr,"Font face is not scalable\n");
		TTF_CloseFont(font);
		return NULL;
	}
       /* Set the character size and use default DPI (72) */
	error = FT_Set_Char_Size(font->face, 0, ptsize * 64, 0, 0);
	if (error) {
		fprintf(stderr, "Couldn't set font size\n");
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

TTF_Font* TTF_OpenFont(const char *file, int ptsize)
{
	TTF_Font *a;
	
	a = TTF_OpenFontIndex(file, ptsize, 0);

	if (a == NULL) {
		fprintf(stderr, "Couldn't load %d pt font from %s\n", ptsize, file);
	}
	
	return a;
}

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

int TTF_SizeUNICODE(TTF_Font *font, const unsigned short *text, int *w, int *h)
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


unsigned char* TTF_RenderUNICODE_Shaded(u8 *target, const unsigned short *text,
					TTF_Font* font, int x, int y, color fcol)
{
	int rlen, glen, blen;
	int xstart;
	int width;
	int height;
	unsigned char* textbuf;
	int rd, gd, bd;
	unsigned int val;
	const unsigned short* ch;
	unsigned char* src;
	unsigned char* dst;
	int row, col;
	c_glyph *glyph;
	FT_Error error;

	/* Get the dimensions of the text surface */
	if((TTF_SizeUNICODE(font, text, &width, NULL) < 0) || !width) {
		fprintf(stderr,"Text has zero width\n");
		return NULL;
	}
	height = font->height;

	/* Create the target surface */
	textbuf=malloc(width*height*bytespp);
	
	if(textbuf == NULL) {
		return NULL;
	}

	if (fb_fix.visual == FB_VISUAL_DIRECTCOLOR) {
		blen = glen = rlen = min(min(fb_var.red.length,fb_var.green.length),fb_var.blue.length);
	} else {
		rlen = fb_var.red.length;
		glen = fb_var.green.length;
		blen = fb_var.blue.length;
	}
	
	/* Load and render each character */
	xstart = 0;
	for(ch = text; *ch; ++ch) {
		FT_Bitmap* current;

		error = Find_Glyph(font, *ch, CACHED_METRICS|CACHED_PIXMAP);
		if(error) {
			free(textbuf);
			return NULL;
		}
		glyph = font->current;

		current = &glyph->pixmap;
		for(row = 0; row < current->rows; ++row) {
			int add;
			u8 *memlimit = target + fb_var.xres * fb_var.yres * bytespp;
			dst = (unsigned char *) target +
				(y + row + glyph->yoffset) * fb_fix.line_length +
				(xstart + glyph->minx + x)*bytespp;
			src = current->buffer + row * current->pitch;

			add = x & 1;
			add ^= (add ^ (row+y)) & 1 ? 1 : 3;
			
			for (col=current->width; col>0; --col) {

				int i;		
				int rb, gb, bb;
			
				if (bytespp == 2 && dst+1 > memlimit)
					break;
				else if (bytespp == 3 && dst+2 > memlimit)
					break;
				else if (bytespp == 4 && dst+3 > memlimit)
					break;
							
				val=*src++;
				val=alpha*val/100;

				rd = fcol.r * val/255;
				gd = fcol.g * val/255;
				bd = fcol.b * val/255;
			
				if (bytespp == 2) { 
					i = *(u16*)dst;
				} else if (bytespp == 3) {
					i = *(u32*)dst & 0xffffff;
				} else if (bytespp == 4) {
					i = *(u32*)dst;
				}				

				rb = ((i >> fb_var.red.offset & ((1 << rlen)-1)) << (8 - rlen));
				gb = ((i >> fb_var.green.offset & ((1 << glen)-1)) << (8 - glen));
				bb = ((i >> fb_var.blue.offset & ((1 << blen)-1)) << (8 - blen));
							
				rd += rb * (255-val)/255;
				gd += gb * (255-val)/255;
				bd += bb * (255-val)/255;

				if (fcol.a != 255) {
					rd = (rd * fcol.a + rb * (255-fcol.a))/255;
					gd = (gd * fcol.a + gb * (255-fcol.a))/255;
					bd = (bd * fcol.a + bb * (255-fcol.a))/255;
				}
		
				/* we only need to do dithering is depth is <24bpp */
				if (fb_var.bits_per_pixel < 24) {
					rd = CLAMP(rd + add*2 + 1);
					gd = CLAMP(gd + add);
					bd = CLAMP(bd + add*2 + 1);
				}
				
			
				rd >>= (8 - rlen);
				gd >>= (8 - glen);
				bd >>= (8 - blen);

				i = (rd << fb_var.red.offset) |
		 	    	    (gd << fb_var.green.offset) |
			    	    (bd << fb_var.blue.offset);

				if (bytespp == 2) {
					*(u16*)dst = i;
					dst += 2;
				} else if (bytespp == 3) {
				
					if (endianess == little) { 
						*(u16*)dst = i & 0xffff;
						dst[2] = (i >> 16) & 0xff;
					} else {
						*(u16*)dst = (i >> 8) & 0xffff;
						dst[2] = i & 0xff;
					}
					dst += 3;
				} else if (bytespp == 4) {
					*(u32*)dst = i;
					dst += 4;
				}
	
				add ^= 3;
			}
		}
		
		xstart += glyph->advance;
		if(font->style & TTF_STYLE_BOLD) {
			xstart += font->glyph_overhang;
		}
	}
	
	/* Handle the underline style */
	if(font->style & TTF_STYLE_UNDERLINE) {
		row = font->ascent - font->underline_offset - 1;
		if (row >= y) {
			row = (height-1) - font->underline_height;
		}
		dst = (unsigned char *)textbuf + row * width*bytespp;
		for (row=font->underline_height; row>0; --row) {
			memset(dst, NUM_GRAYS - 1, width);
			dst += width*bytespp;
		}
	}
	return textbuf;
}


int TTF_Render(u8 *target, char *text, TTF_Font *font, int style, int x, int y, color col)
{
	if (!target || !text || !font)
		return -1;
	
	TTF_SetFontStyle(font, style);
	TTF_RenderText_Shaded(target, text, font, x, y, col);

	return 0;
}

int load_fonts(void)
{
	item *i;

	if (!global_font)
		global_font = TTF_OpenFont(cf.text_font, cf.text_size);
	
	for (i = fonts.head; i != NULL; i = i->next) {
		font_e *fe = (font_e*) i->p;
		if (!fe->font) {
			fe->font = TTF_OpenFont(fe->file, fe->size);
		}
	}

	return 0;
}

int free_fonts(void)
{
	item *i, *j;

	if (global_font) {
		TTF_CloseFont(global_font);
		global_font = NULL;
	}
	
	for (i = fonts.head; i != NULL;) {
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

