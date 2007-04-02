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

extern TTF_Font *global_font;
extern int boot_msg_width;

int TTF_Init(void);
void TTF_CloseFont(TTF_Font* font);
TTF_Font* TTF_OpenFont(const char *file, int ptsize);
int TTF_Render(u8 *target, char *text, TTF_Font *font, int style, int x, 
		int y, color col, u8 hotspot, int *width);
int load_fonts(void);
int free_fonts(void);

#endif
