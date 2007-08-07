#ifndef _LINUX_FBCONDECOR_H
#define _LINUX_FBCONDECOR_H

#include <asm/types.h>

#ifndef FBCON_DECOR_THEME_LEN

struct fbcon_decor_iowrapper
{
	unsigned short vc;		/* Virtual console */
	unsigned char origin;		/* Point of origin of the request */
	void *data;
};

#define FBIOCONDECOR_SETCFG	_IOWR('F', 0x19, struct fbcon_decor_iowrapper)
#define FBIOCONDECOR_GETCFG	_IOR('F', 0x1A, struct fbcon_decor_iowrapper)
#define FBIOCONDECOR_SETSTATE	_IOWR('F', 0x1B, struct fbcon_decor_iowrapper)
#define FBIOCONDECOR_GETSTATE	_IOR('F', 0x1C, struct fbcon_decor_iowrapper)
#define FBIOCONDECOR_SETPIC 	_IOWR('F', 0x1D, struct fbcon_decor_iowrapper)

#define FBCON_DECOR_THEME_LEN		128	/* Maximum lenght of a theme name */
#define FBCON_DECOR_IO_ORIG_KERNEL	0	/* Kernel ioctl origin */
#define FBCON_DECOR_IO_ORIG_USER	1	/* User ioctl origin */

#endif /* FBCON_DECOR_THEME_LEN */
#endif /* _LINUX_FBCONDECOR_H */
