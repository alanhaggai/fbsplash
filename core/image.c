/*
 * image.c - Functions to load & unpack PNGs and JPEGs
 *
 * Copyright (C) 2004-2005, Michael Januszewski <spock@gentoo.org>
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include <stdio.h>
#include <stdlib.h>
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

struct fb_image verbose_img;
struct fb_image silent_img;

typedef struct {
	u8 r, g, b;
} __attribute__ ((packed)) rgbcolor;

/* This function converts a truecolor image to whatever format the 
 * framebuffer uses */
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
int load_png(char *filename, u8 **data, struct fb_cmap *cmap, int *width, int *height, u8 want_alpha)
{
	png_structp 	png_ptr;
	png_infop 	info_ptr;
	png_bytep 	row_pointer;
	png_colorp 	palette;
	int 		rowbytes, num_palette;
	int 		i, j, bytespp = (fb_var.bits_per_pixel + 7) >> 3;
	u8 *buf = NULL;
	u8 *t;

	if (want_alpha)
		bytespp = 4;
	
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

	if (cmap && info_ptr->color_type != PNG_COLOR_TYPE_PALETTE)
		return -2;

	if (info_ptr->color_type == PNG_COLOR_TYPE_GRAY ||
	    info_ptr->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

	if (info_ptr->bit_depth == 16)
		png_set_strip_16(png_ptr);

	if (!want_alpha && info_ptr->color_type & PNG_COLOR_MASK_ALPHA)
		png_set_strip_alpha(png_ptr);

#ifndef TARGET_KERNEL	
	if (!(info_ptr->color_type & PNG_COLOR_MASK_ALPHA) & want_alpha) {
		png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
	}
#endif
	png_read_update_info(png_ptr, info_ptr);

	if (!cmap && info_ptr->color_type != PNG_COLOR_TYPE_RGB && info_ptr->color_type != PNG_COLOR_TYPE_RGBA)
		return -3;

	if (cmap) {
		png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette);
	
		if (num_palette > cmap->len)
			return -3;
	}

	rowbytes = png_get_rowbytes(png_ptr, info_ptr);	

	if ((width && *width && info_ptr->width != *width) || (height && *height && info_ptr->height != *height)) {
		printerr("Image size mismatch: %s.\n", filename);
		return -2;
	} else {
		*width = info_ptr->width;
		*height = info_ptr->height;
	}
	
	*data = malloc(fb_var.xres * fb_var.yres * bytespp);
	if (!*data) {
		printerr("Failed to allocate memory for image: %s.\n", filename);
		return -4;
	}
	
	buf = malloc(rowbytes);	
	if (!buf) {
		printerr("Failed to allocate memory for image line buffer.\n");
		free(*data);
		return -4;
	}
	
	for (i = 0; i < info_ptr->height; i++) {
		if (cmap) {
			row_pointer = *data + info_ptr->width * i;
		} else if (want_alpha) {
			row_pointer = *data + info_ptr->width * i * 4;
		} else {
			row_pointer = buf;
		}
		
	        png_read_row(png_ptr, row_pointer, NULL);
		
		if (cmap) {
			int h = 256 - cmap->len;
			t = *data + info_ptr->width * i;

			if (h) {
				/* Move the colors up by 'h' offset. This is used because fbcon
				 * takes the first 16 colors. */
				for (j = 0; j < rowbytes; j++) {
					t[j] += h;
				}
			}
		
		/* We only need to convert the image if we the alpha channel is not required */	
		} else if (!want_alpha) {
			truecolor2fb((truecolor*)buf, *data + info_ptr->width * bytespp * i, info_ptr->width, i, 0);
		}
	}

	if (cmap) {
		for (i = 0; i < cmap->len; i++) {
			cmap->red[i] = palette[i].red * 257;
			cmap->green[i] = palette[i].green * 257;
			cmap->blue[i] = palette[i].blue * 257;
		}	
	}

	free(buf);
	fclose(fp);
	
	return 0;
}

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
#endif /* PNG */

int load_jpeg(char *filename, u8 **data, int *width, int *height)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE* injpeg;

	u8 *buf = NULL;
	int i, bytespp = (fb_var.bits_per_pixel+7) >> 3;
	
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	
	if ((injpeg = fopen(filename,"r")) == NULL) {
		printerr("Can't open file %s!\n", filename);
		return -1;	
	}

	jpeg_stdio_src(&cinfo, injpeg);
	jpeg_read_header(&cinfo, TRUE);
	jpeg_start_decompress(&cinfo);

	if ((width && cinfo.output_width != *width) || (height && cinfo.output_height != *height)) {
		printerr("Image size mismatch: %s.\n", filename);
		return -2;
	} else {
		*width = cinfo.output_width;
		*height = cinfo.output_height;
	}
	
	buf = malloc(cinfo.output_width * cinfo.output_components * sizeof(char));
	if (!buf) {
		printerr("Failed to allocate JPEG decompression buffer.\n");
		return -1;
	}

	*data = malloc(cinfo.output_width * cinfo.output_height * bytespp);
	if (!*data) {
		printerr("Failed to allocate memory for image: %s.\n", filename);
		return -4;
	}
	
	for (i = 0; i < cinfo.output_height; i++) {
		jpeg_read_scanlines(&cinfo, (JSAMPARRAY) &buf, 1);
		truecolor2fb((truecolor*)buf, *data + cinfo.output_width * bytespp * i, cinfo.output_width, i, 0);
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	fclose(injpeg);

	free(buf);
	return 0;
}

int load_bg_images(char mode)
{
	struct fb_image *img = (mode == 'v') ? &verbose_img : &silent_img;
	char *pic;
	int i;
	
	img->width = fb_var.xres;
	img->height = fb_var.yres;
	img->depth = fb_var.bits_per_pixel;

	/* Deal with 8bpp modes. Only PNGs can be loaded, and pic256
	 * option has to be used to specify the filename of the image */
	if (fb_var.bits_per_pixel == 8) {
		pic = (mode == 'v') ? cf_pic256 : cf_silentpic256;
		
		if (!pic) {
			printerr("No 8bpp %s picture specified in the theme config.\n", (mode == 'v') ? "verbose" : "silent" );
			return -1;
		}

#ifdef CONFIG_PNG
		if (!is_png(pic)) {
			printerr("Unrecognized format of the verbose 8bpp background image.\n");
			return -1;	
		}

		/* We have a palette of 256 colors, but fbcon takes 16 of these for 
		 * font colors in verbose mode, so we have 240 left for the picture */
		if (mode != 's') {
			i = PALETTE_COLORS;
			img->cmap.start = 16;
		} else {
			i = 256;
			img->cmap.start = 0;
		}
		
		img->cmap.transp = NULL;
		img->cmap.red = malloc(i * 3 * 2);
	
		if (!img->cmap.red) {
			printerr("Failed to allocate memory for the image palette.\n");
			return -4;
		}
					
		img->cmap.green = img->cmap.red + i;
		img->cmap.blue = img->cmap.green + i;
		img->cmap.len = i;
		
		if (load_png(pic, (u8**)&img->data, &img->cmap, &img->width, &img->height, 0)) {
			printerr("Failed to load PNG file %s.\n", pic);
			return -1;
		}	
#else
		printerr("This version of splashutils has been compiled without support for 8bpp modes.\n");
		return -1;
#endif
	/* Deal with 15, 16, 24 and 32bpp modes */
	} else {
		pic = (mode == 'v') ? cf_pic : cf_silentpic;
		
#ifdef CONFIG_PNG
		if (is_png(cf_pic)) {
			i = load_png(pic, (u8**)&img->data, NULL, &img->width, &img->height, 0);
		} else
#endif
		{
			i = load_jpeg(pic, (u8**)&img->data, &img->width, &img->height);
		}
		
		if (i) {
			printerr("Failed to load image %s.\n", pic);
			return -1;
		}
	}

	return 0;
}


int load_images(char mode)
{
	item *i;
	
	if (!config_file) {
		printerr("No config file specified.\n");
		return -1;
	}
	
	if (mode == 'v' || mode == 'a')
		load_bg_images('v');

	if (mode == 's' || mode == 'a') {
		load_bg_images('s');
	
		for (i = icons.head; i != NULL; i = i->next) {
			icon_img *ii = (icon_img*) i->p;
			ii->w = ii->h = 0;
			
			if (!is_png(ii->filename)) {
				printerr("Icon %s is not a PNG file.\n", ii->filename);
				continue;
			}
			
			if (load_png(ii->filename, &ii->picbuf, NULL, &ii->w, &ii->h, 1)) {
				printerr("Failed to load icon %s.\n", ii->filename);
				ii->picbuf = NULL;
				ii->w = ii->h = 0;
				continue;
			}
		}
	}

	return 0;
}


