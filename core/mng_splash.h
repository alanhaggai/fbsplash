#ifndef _FBANIM_MNG_H_
#define _FBANIM_MNG_H_

#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>
#include <libmng.h>

typedef struct {
	void *data;
	int len, ptr, open;

	char *canvas;
	int canvas_h, canvas_w, canvas_bytes_pp;

	int wait_msecs;
	struct timeval start_time;
	int displayed_first;
	int num_frames;
} mng_anim;

struct anim;

/* mng_render.c */
extern mng_handle mng_load(char *filename, int *w, int *h);
extern void mng_done(mng_handle mngh);
extern mng_retcode mng_render_next(mng_handle mngh);
extern void anim_prerender(struct stheme *theme, struct anim *a, bool force);
extern void anim_render(struct stheme *theme, struct anim *a, rect *re, u8* tg);
extern mng_retcode mng_render_proportional(mng_handle mngh, int progress);

/* mng_callbacks.c */
extern mng_ptr fb_mng_memalloc(mng_size_t len);
extern void fb_mng_memfree(mng_ptr p, mng_size_t len);
extern mng_retcode mng_init_callbacks(mng_handle handle);
extern mng_retcode mng_display_restart(mng_handle mngh);

/* MNG-error printing functions */
static inline void __print_mng_error(mng_handle mngh, char* s, ...)
{
	va_list ap;

	mng_int8 severity;
	mng_chunkid chunkname;
	mng_uint32 chunkseq;
	mng_int32 extra1, extra2;
	mng_pchar err_text;
	mng_getlasterror(mngh, &severity, &chunkname, &chunkseq,
			&extra1, &extra2, &err_text);
	va_start(ap, s);
	vfprintf(stderr, s, ap);
	va_end(ap);
	fprintf(stderr, ": %s\n", err_text);
}
#define print_mng_error(mngh, s, x...) do { \
		__print_mng_error(mngh, "%s: "s, __FUNCTION__, ## x); \
	} while (0)

#endif /* _FBANIM_MNG_H_ */
