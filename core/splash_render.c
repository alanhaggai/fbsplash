/*
 * splash_render.c - Functions for boxes rendering
 *
 * Copyright (C) 2004, Michal Januszewski <spock@gentoo.org>
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * $Header: /srv/cvs/splash/utils/splash_render.c,v 1.12 2005/01/29 23:27:49 spock Exp $
 * 
 */

#include <linux/fb.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
//#include <linux/types.h>
#include "splash.h"

void draw_box(u8* data, struct splash_box box, int fd)
{
	int rlen, glen, blen;
	int x, y, a, r, g, b, i;
	int add;
	
	u8 *pic, *tmp;
	struct colorf h_ap1, h_ap2, h_bp1, h_bp2;
	
	int bytespp = fb_var.bits_per_pixel >> 3;
	int b_width = box.x2 - box.x1;
	int b_height = box.y2 - box.y1;
	
	if (box.x2 > fb_var.xres || box.y2 > fb_var.yres || b_width < 0 || b_height < 0) {
		fprintf(stderr, "Ignoring invalid box (%d, %d, %d, %d).\n", box.x1, box.y1, box.x2, box.y2);
		return;
	}	

	if (b_width <= 0)
		b_width = 1;
		
	if (b_height <= 0)
		b_height = 1; 
	
	h_ap1.r = (double)box.c_ul.r / b_height;
	h_ap1.g = (double)box.c_ul.g / b_height;
	h_ap1.b = (double)box.c_ul.b / b_height;
	h_ap1.a = (double)box.c_ul.a / b_height;

	h_ap2.r = (double)(box.c_ur.r - box.c_ul.r) / (b_width * b_height);
	h_ap2.g = (double)(box.c_ur.g - box.c_ul.g) / (b_width * b_height);
	h_ap2.b = (double)(box.c_ur.b - box.c_ul.b) / (b_width * b_height);
	h_ap2.a = (double)(box.c_ur.a - box.c_ul.a) / (b_width * b_height);

	h_bp1.r = (double)box.c_ll.r / b_height;
	h_bp1.g = (double)box.c_ll.g / b_height;
	h_bp1.b = (double)box.c_ll.b / b_height;
	h_bp1.a = (double)box.c_ll.a / b_height;

	h_bp2.r = (double)(box.c_lr.r - box.c_ll.r) / (b_width * b_height);
	h_bp2.g = (double)(box.c_lr.g - box.c_ll.g) / (b_width * b_height);
	h_bp2.b = (double)(box.c_lr.b - box.c_ll.b) / (b_width * b_height);
	h_bp2.a = (double)(box.c_lr.a - box.c_ll.a) / (b_width * b_height);
	
	if (fb_fix.visual == FB_VISUAL_DIRECTCOLOR) {
		blen = glen = rlen = min(min(fb_var.red.length,fb_var.green.length),fb_var.blue.length);
	} else {
		rlen = fb_var.red.length;
		glen = fb_var.green.length;
		blen = fb_var.blue.length;
	}

	for (y = box.y1; y <= box.y2; y++) {

		tmp = pic = data + (box.x1 + y * fb_var.xres) * bytespp;

		if (fd)
			lseek(fd, (fb_var.yoffset + y) * fb_fix.line_length + (box.x1 * bytespp), SEEK_SET);

		/* do a nice 2x2 ordered dithering, like it was done in bootsplash;
		 * this makes the pics in 15/16bpp modes look much nicer;
		 * the produced pattern is:
		 * 303030303..
		 * 121212121..
		 */
		add = (box.x1 & 1);
		add ^= (add ^ y) & 1 ? 1 : 3;

		for (x = box.x1; x <= box.x2; x++) {

			int t1 = b_height - (y-box.y1);
			int t2 = y - box.y1;	
			int t3 = x - box.x1;
						
			a = t1 * (h_ap1.a + t3*h_ap2.a) + t2 * (h_bp1.a + t3*h_bp2.a);
			r = t1 * (h_ap1.r + t3*h_ap2.r) + t2 * (h_bp1.r + t3*h_bp2.r);
			g = t1 * (h_ap1.g + t3*h_ap2.g) + t2 * (h_bp1.g + t3*h_bp2.g);
			b = t1 * (h_ap1.b + t3*h_ap2.b) + t2 * (h_bp1.b + t3*h_bp2.b);
	
			if (a != 255) {
			
				if (fb_var.bits_per_pixel == 16) { 
					i = *(u16*)pic;
				} else if (fb_var.bits_per_pixel == 24) {
					i = *(u32*)pic & 0xffffff;
				} else if (fb_var.bits_per_pixel == 32) {
					i = *(u32*)pic;
				} else {
					i = *(u32*)pic & ((2 << fb_var.bits_per_pixel)-1);
				}

				r = (( (i >> fb_var.red.offset & ((1 << rlen)-1)) 
				      << (8 - rlen)) * (255 - a) + r * a) / 255;
				g = (( (i >> fb_var.green.offset & ((1 << glen)-1)) 
				      << (8 - glen)) * (255 - a) + g * a) / 255;
				b = (( (i >> fb_var.blue.offset & ((1 << blen)-1)) 
				      << (8 - blen)) * (255 - a) + b * a) / 255;
			}
		
			/* we only need to do dithering is depth is <24bpp */
			if (fb_var.bits_per_pixel < 24) {
				r = CLAMP(r + add*2 + 1);
				g = CLAMP(g + add);
				b = CLAMP(b + add*2 + 1);
			}
	
			r >>= (8 - rlen);
			g >>= (8 - glen);
			b >>= (8 - blen);

			i = (r << fb_var.red.offset) |
		 	    (g << fb_var.green.offset) |
			    (b << fb_var.blue.offset);

			if (fb_var.bits_per_pixel == 16) {
				*(u16*)pic = i;
				pic += 2;
			} else if (fb_var.bits_per_pixel == 24) {
				
				if (endianess == little) { 
					*(u16*)pic = i & 0xffff;
					pic[2] = (i >> 16) & 0xff;
				} else {
					*(u16*)pic = (i >> 8) & 0xffff;
					pic[2] = i & 0xff;
				}
				pic += 3;
			} else if (fb_var.bits_per_pixel == 32) {
				*(u32*)pic = i;
				pic += 4;
			}

			add ^= 3;
		}
	
		if (fd)
			write(fd, tmp, (box.x2 - box.x1 + 1) * bytespp);
	}
}

#if 0

/* draws a partially transparent box over the splash picture */
void draw_transp_box(u8 *data, struct splash_box box, struct splash_box next) 
{
	int rlen, glen, blen;
	int x, y, a, r, g, b, i;

	u8* pic;

	int bytespp = fb_var.bits_per_pixel >> 3;
	
	if (fb_fix.visual == FB_VISUAL_DIRECTCOLOR) {
		blen = glen = rlen = min(min(fb_var.red.length,fb_var.green.length),fb_var.blue.length);
	} else {
		rlen = fb_var.red.length;
		glen = fb_var.green.length;
		blen = fb_var.blue.length;
	}

	for (y = box.y1; y <= box.y2; y++) {

		pic = data + (box.x1 + y * fb_var.xres) * bytespp;

		for (x = box.x1; x <= box.x2; x++) {

			a = box.color & 0xff;
			r = box.color >> 24;
			g = box.color >> 16 & 0xff;
			b = box.color >> 8 & 0xff;
	
			if (a != 255) {
			
				if (fb_var.bits_per_pixel == 16) { 
					i = *(u16*)pic;
				} else if (fb_var.bits_per_pixel == 24) {
					i = *(u32*)pic & 0xffffff;
				} else if (fb_var.bits_per_pixel == 32) {
					i = *(u32*)pic;
				} else {
					i = *(u32*)pic & ((2 << fb_var.bits_per_pixel)-1);
				}

				r = (( (i >> fb_var.red.offset & ((1 << rlen)-1)) 
				      << (8 - rlen)) * (255 - a) + r * a) / 255;
				g = (( (i >> fb_var.green.offset & ((1 << glen)-1)) 
				      << (8 - glen)) * (255 - a) + g * a) / 255;
				b = (( (i >> fb_var.blue.offset & ((1 << blen)-1)) 
				      << (8 - blen)) * (255 - a) + b * a) / 255;
			}

			r >>= (8 - rlen);
			g >>= (8 - glen);
			b >>= (8 - blen);

			i = (r << fb_var.red.offset) |
		 	    (g << fb_var.green.offset) |
			    (b << fb_var.blue.offset);

			if (fb_var.bits_per_pixel == 16) {
				*(u16*)pic = i;
				pic += 2;
			} else if (fb_var.bits_per_pixel == 24) {
				
				if (endianess == little) { 
					*(u16*)pic = i & 0xffff;
					pic[2] = (i >> 16) & 0xff;
				} else {
					*(u16*)pic = (i >> 8) & 0xffff;
					pic[2] = i & 0xff;
				}
				pic += 3;
			} else if (fb_var.bits_per_pixel == 32) {
				*(u32*)pic = i;
				pic += 4;
			}	
		}
	}
}

/* draws a solid box on the background image */
void draw_std_box(u8 *data, struct splash_box box) 
{
	int x, y, bytespp = fb_var.bits_per_pixel >> 3;
	u8 *t;

	t = data + (box.y1 * fb_var.xres + box.x1) * bytespp;
	
	for (y = box.y1; y <= box.y2; y++) {
		
		for (x = box.x1; x < box.x2; x++) {

			switch (fb_var.bits_per_pixel) {

			case 32:
				*(u32*)(t) = box.color;
				t += 4;
				break;
			case 24:
				if (endianess == little) {
					*(u16*)(t) = box.color & 0xffff; t += 2;
					*(u8*)(t) = box.color >> 16; t++;
				} else {
					*(u16*)(t) = box.color >> 8; t += 2;
					*(u8*)(t) = box.color & 0xff; t++;
				}
				break;
			case 16:
			case 15:
				*(u16*)(t) = box.color;
				t += 2;
				break;
			}
		}

		t += (fb_var.xres - box.x2 + box.x1) * bytespp;
	}
}

#endif 

/* Interpolates two boxes, based on the value of the arg_progress variable.
 * This is a strange implementation of a arg_progress bar, introduced by the
 * authors of Bootsplash. */
void interpolate_box(struct splash_box *a, struct splash_box b)
{
	int h = PROGRESS_MAX - arg_progress;

#define inter_color(cl1, cl2) 					\
{								\
	cl1.r = (cl1.r * h + cl2.r * arg_progress) / PROGRESS_MAX; 	\
	cl1.g = (cl1.g * h + cl2.g * arg_progress) / PROGRESS_MAX;	\
	cl1.b = (cl1.b * h + cl2.b * arg_progress) / PROGRESS_MAX;	\
	cl1.a = (cl1.a * h + cl2.a * arg_progress) / PROGRESS_MAX; 	\
}
	
	a->x1 = (a->x1 * h + b.x1 * arg_progress) / PROGRESS_MAX;
	a->x2 = (a->x2 * h + b.x2 * arg_progress) / PROGRESS_MAX;
	a->y1 = (a->y1 * h + b.y1 * arg_progress) / PROGRESS_MAX;
	a->y2 = (a->y2 * h + b.y2 * arg_progress) / PROGRESS_MAX;

	inter_color(a->c_ul, b.c_ul);
	inter_color(a->c_ur, b.c_ur);
	inter_color(a->c_ll, b.c_ll);
	inter_color(a->c_lr, b.c_lr);
}

void draw_icons(u8 *data)
{
	int i;
	for (i = 0; i < cf_icons_cnt; i++) {
		draw_icon(cf_icons[i], data);
	}
	return;
}


/* a wrapper function to handle different types of boxes to be drawn */
void draw_boxes(u8 *data, char mode, unsigned char origin)
{
	char dev[16];
	int i, fd = 0;
	struct splash_box tmp;

	if (arg_progress > 0 && arg_task == paint && origin == FB_SPLASH_IO_ORIG_USER && cf_rects_cnt == 0) {
		sprintf(dev, "/dev/fb%d", arg_fb);
		if ((fd = open(dev, O_WRONLY)) == -1) {
			sprintf(dev, "/dev/fb/%d", arg_fb);
			if ((fd = open(dev, O_WRONLY)) == -1)
				printerr("Failed to open /dev/fb%d or /dev/fb/%d.\n", arg_fb, arg_fb);
		}
	}
	
	for (i = 0; i < cf_boxes_cnt; i++) {

		if (cf_boxes[i].attr & BOX_SILENT && mode != 's')
			continue;

		if (!(cf_boxes[i].attr & BOX_SILENT) && mode != 'v')
			continue;

		if (cf_boxes[i].attr & BOX_NOOVER && arg_progress != 0 && arg_task != repaint)
			continue;

		if (cf_boxes[i].attr & BOX_INTER && i < cf_boxes_cnt - 1) {
			if (arg_progress == 0)
				goto next;				
			tmp = cf_boxes[i];
			interpolate_box(&tmp, cf_boxes[i+1]);
			draw_box(data, tmp, fd);
next:			i++;
			continue;
		}			

		draw_box(data, cf_boxes[i], fd);
	}

	if (fd)
		close(fd);
	
	return;
}

