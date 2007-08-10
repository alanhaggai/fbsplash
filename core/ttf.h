#ifndef _TTF_H
#define _TTF_H
#include <ft2build.h>
#include <freetype/ftoutln.h>
#include <freetype/ttnameid.h>

#define CACHED_METRICS  0x10
#define CACHED_BITMAP   0x01
#define CACHED_PIXMAP   0x02

#define TTF_STYLE_NORMAL        0x00
#define TTF_STYLE_BOLD          0x01
#define TTF_STYLE_ITALIC        0x02
#define TTF_STYLE_UNDERLINE     0x04

/* Handy routines for converting from fixed point */
#define FT_FLOOR(X)     ((X & -64) / 64)
#define FT_CEIL(X)      (((X + 63) & -64) / 64)

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

typedef struct _TTF_Font TTF_Font;

int TTF_Init(void);
void TTF_Quit(void);

struct fbspl_theme;
struct text;

void text_render(struct fbspl_theme *theme, struct text *ct, rect *re, u8 *target);
void text_prerender(struct fbspl_theme *theme, struct text *ct, bool force);
void text_bnd(struct fbspl_theme *theme, struct text *ct, rect *bnd);

int load_fonts(struct fbspl_theme *theme);
int free_fonts(struct fbspl_theme *theme);

#endif
