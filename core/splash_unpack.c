/*
 * splash_unpack.c - Functions to load & unpack PNGs and JPEGs
 *
 * Copyright (C) 2004, Michal Januszewski <spock@gentoo.org>
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include <stdio.h>
#include <stdlib.h>
//#include <linux/types.h>
#include <linux/fb.h>

#include "config.h"

#ifdef CONFIG_PNG
#ifdef TARGET_KERNEL
  #include "libs/libpng-1.2.8/png.h"
#else
  #include <png.h>
#endif
#endif

#ifdef TARGET_KERNEL
  #include "libs/jpeg-6b/jpeglib.h"
#else
  #include <jpeglib.h>
#endif

#include "splash.h"

typedef struct truecolor {
	u8 r, g, b, a;
} __attribute__ ((packed)) truecolor;

typedef struct {
	u8 r, g, b;
} __attribute__ ((packed)) rgbcolor;

void truecolor2fb (truecolor* data, u8* out, int len, int y, u8 alpha)
{
	int i, add = 0, r, g, b, a;
	int rlen, blen, glen;
	rgbcolor* rgb = (rgbcolor*)data;
	u32 t;
	
	if (fb_fix.visual == FB_VISUAL_DIRECTCOLOR) {
		blen = glen = rlen = min(min(fb_var.red.length,fb_var.green.length),fb_var.blue.length);
	} else {
		rlen = fb_var.red.length;
		glen = fb_var.green.length;
		blen = fb_var.blue.length;
	}
		
	add ^= (0 ^ y) & 1 ? 1 : 3;

	for (i = 0; i < len; i++) {

		if (alpha) {
			r = data[i].r;
			g = data[i].g;
			b = data[i].b;
			a = data[i].a;
		} else {
			r = rgb[i].r;
			g = rgb[i].g;
			b = rgb[i].b;
		}
			
		if (alpha) {
	
			switch (fb_var.bits_per_pixel) {
			
			case 32:
				t = *(u32*)(&out[(i * 4)]);
				break;
			case 24:
				t = *(u32*)(&out[(i * 3)]) & 0xffffff;
				break;
			case 16:
			case 15:
				t = *(u16*)(&out[(i * 2)]);
				break;
			}
	
			r = (( (t >> fb_var.red.offset & ((1 << rlen)-1)) 
				<< (8 - rlen)) * (255 - a) + r * a) / 255;
			g = (( (t >> fb_var.green.offset & ((1 << glen)-1)) 
			    	<< (8 - glen)) * (255 - a) + g * a) / 255;
			b = (( (t >> fb_var.blue.offset & ((1 << blen)-1)) 
				<< (8 - blen)) * (255 - a) + b * a) / 255;

		} 

		if (fb_var.bits_per_pixel < 24) {
			r = CLAMP(r + add*2 + 1);
			g = CLAMP(g + add);
			b = CLAMP(b + add*2 + 1);
		}
		
		r >>= (8 - rlen);
		g >>= (8 - glen);
		b >>= (8 - blen);
	
		t = (r << fb_var.red.offset) | 
		    (g << fb_var.green.offset) |
		    (b << fb_var.blue.offset);

		switch (fb_var.bits_per_pixel) {

		case 32:
			*(u32*)(&out[(i * 4)]) = t;
			break;
		case 24:
			if (endianess == little) {
				*(u16*)(&out[(i*3)]) = t & 0xffff;
				*(u8*)(&out[(i*3+2)]) = t >> 16;
			} else {
				*(u16*)(&out[(i*3)]) = t >> 8;
				*(u8*)(&out[(i*3+2)]) = t & 0xff;
			}
			break;
		case 16:
		case 15:
			*(u16*)(&out[(i * 2)]) = t;
			break;
		}

		add ^= 3;
	}
}

#ifdef CONFIG_PNG
#define PALETTE_COLORS 240

int draw_icon(struct splash_icon ic, u8 *data)
{
	png_structp 	png_ptr;
	png_infop 	info_ptr;
	png_bytep 	row_pointer;
	int 		rowbytes;
	int 		i, bytespp = fb_var.bits_per_pixel >> 3;
	u8 *buf = NULL;
	FILE *fp;
	
	fp = fopen(ic.filename,"r");
	if (!fp)
		return -1;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	info_ptr = png_create_info_struct(png_ptr);

	if (setjmp(png_jmpbuf(png_ptr))) {
		return -1;
	}

	png_init_io(png_ptr, fp);
	png_read_info(png_ptr, info_ptr);

	if (fb_var.bits_per_pixel == 8)
		return -2;

	if (info_ptr->color_type == PNG_COLOR_TYPE_GRAY ||
	    info_ptr->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

	if (info_ptr->bit_depth == 16)
		png_set_strip_16(png_ptr);

#ifndef TARGET_KERNEL	
	if (!(info_ptr->color_type & PNG_COLOR_MASK_ALPHA)) {
		png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
	}
#endif
	png_read_update_info(png_ptr, info_ptr);

	if (info_ptr->color_type != PNG_COLOR_TYPE_RGB && info_ptr->color_type != PNG_COLOR_TYPE_RGBA)
		return -3;

	rowbytes = png_get_rowbytes(png_ptr, info_ptr);	
	
	if (info_ptr->width > fb_var.xres - ic.x || info_ptr->height > fb_var.yres - ic.y) {
		fprintf(stderr, "Warning: icon %s does not fit on the screen!\n", ic.filename);
		return -2;
	}

	buf = malloc(rowbytes);	
	if (!buf) {
		return -4;
	}

	for (i = 0; i < info_ptr->height && (i + ic.y) < fb_var.yres; i++) {

		row_pointer = buf;
	        png_read_row(png_ptr, row_pointer, NULL);

		truecolor2fb((truecolor*)buf, (u8*)data + fb_var.xres * bytespp * (i + ic.y) + (ic.x * bytespp), 
			     info_ptr->width, i + ic.y, 1);
	}

	free(buf);
	fclose(fp);
	
	return 0;

}

int load_png(char *filename, struct fb_image *img, char mode)
{
	png_structp 	png_ptr;
	png_infop 	info_ptr;
	png_bytep 	row_pointer;
	png_colorp 	palette;
	int 		num_palette;
	int 		rowbytes;
	int 		i, j, bytespp = fb_var.bits_per_pixel >> 3;
	u8 *buf = NULL;
	u8 *t;
	int 		pal_len;

	if (mode != 's')
		pal_len = PALETTE_COLORS;
	else
		pal_len = 256;
		
	FILE *fp = fopen(filename,"r");
	if (!fp)
		return -1;
	
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	info_ptr = png_create_info_struct(png_ptr);

	if (setjmp(png_jmpbuf(png_ptr))) {
		return -1;
	}

	png_init_io(png_ptr, fp);
	png_read_info(png_ptr, info_ptr);

	if (fb_var.bits_per_pixel == 8 && info_ptr->color_type != PNG_COLOR_TYPE_PALETTE)
		return -2;

	if (info_ptr->bit_depth == 16)
		png_set_strip_16(png_ptr);

	if (info_ptr->color_type & PNG_COLOR_MASK_ALPHA)
		png_set_strip_alpha(png_ptr);

	if (info_ptr->color_type == PNG_COLOR_TYPE_GRAY ||
	    info_ptr->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

	png_read_update_info(png_ptr, info_ptr);
	
	if (fb_var.bits_per_pixel == 8) {
		png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette);
	
		/* we have a palette of 256 colors, but fbcon takes 16 of these for 
		 * font colors, so we have 240 left for the picture */
		if (num_palette > pal_len)
			return -3;
	}

	rowbytes = png_get_rowbytes(png_ptr, info_ptr);	
	
	if (info_ptr->width > fb_var.xres || info_ptr->height > fb_var.yres)
		return -2;

	img->data = malloc(fb_var.xres * fb_var.yres * bytespp);
	if (!img->data)
		return -4;
	
	img->cmap.transp = NULL;
	if (fb_var.bits_per_pixel == 8) {
		img->cmap.red = malloc(pal_len * 3 * 2);
	
		if (!img->cmap.red) {
			free((void*)img->data);
			return -4;
		}
		
		img->cmap.green = img->cmap.red + pal_len;
		img->cmap.blue = img->cmap.green + pal_len;
		img->cmap.len = pal_len;
		
		if (mode == 'v')
			img->cmap.start = 16;
		else
			img->cmap.start = 0;
	} else {
		img->cmap.len = 0;
		img->cmap.red = NULL;
	}
	
	buf = malloc(rowbytes);	
	if (!buf) {
		free((void*)img->data);
		if (img->cmap.red)
			free((void*)img->cmap.red);
		return -4;
	}
	
	for (i = 0; i < info_ptr->height; i++) {
		if (fb_var.bits_per_pixel > 8) {
			row_pointer = buf;
		} else {
			row_pointer = (u8*)img->data + fb_var.xres * i;
		}
		
	        png_read_row(png_ptr, row_pointer, NULL);
		
		if (fb_var.bits_per_pixel > 8) {
			truecolor2fb((truecolor*)buf, (u8*)img->data + fb_var.xres * bytespp * i, info_ptr->width, i, 0);
		} else {
			t = (u8*)img->data + fb_var.xres * i;

			/* first 16 colors are taken by fbcon */
			if (mode == 'v') {
				for (j = 0; j < rowbytes; j++) {
					t[j] += 16;
				}
			}
		}
	}

	if (fb_var.bits_per_pixel == 8) {
		for (i = 0; i < num_palette; i++) {
			img->cmap.red[i] = palette[i].red * 257;
			img->cmap.green[i] = palette[i].green * 257;
			img->cmap.blue[i] = palette[i].blue * 257;
		}	
	}

	free(buf);
	fclose(fp);
	
	return 0;
}
#endif /* PNG */

int decompress_jpeg(char *filename, struct fb_image *img)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE* injpeg;

	u8 *buf = NULL;
	int i, bytespp = fb_var.bits_per_pixel >> 3;
	
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	
	if ((injpeg = fopen(filename,"r")) == NULL) {
		fprintf(stderr, "Can't open file %s!\n", filename);
		return -1;	
	}

	jpeg_stdio_src(&cinfo, injpeg);
	jpeg_read_header(&cinfo, TRUE);
	jpeg_start_decompress(&cinfo);

	buf = malloc(cinfo.output_width * cinfo.output_components * sizeof(char));
	img->data = malloc(cinfo.output_width * cinfo.output_height * bytespp);
	img->cmap.red = NULL;
	img->cmap.len = 0;
	
	for (i = 0; i < cinfo.output_height; i++) {
		jpeg_read_scanlines(&cinfo, (JSAMPARRAY) &buf, 1);
		truecolor2fb((truecolor*)buf, (u8*)img->data + cinfo.output_width * bytespp * i, cinfo.output_width, i, 0);
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	fclose(injpeg);

	free(buf);
	return 0;
}

#ifdef CONFIG_PNG
int is_png(char *filename)
{
	char header[8];
	FILE *fp = fopen(filename,"r");
	
	if (!fp)
		return -1;

	fread(header, 1, 8, fp);
	fclose(fp);
	
	return !png_sig_cmp(header, 0, 8);
}
#endif

