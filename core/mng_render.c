/*
 * mng_render.c - Functions for rendering MNG animations.
 *
 * Copyright (C) 2005 Bernard Blackham <bernard@blackham.com.au>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libmng.h>
#include <unistd.h>
#include "util.h"

static int mng_readfile(mng_handle mngh, char *filename)
{
	int fd, len;
	char *file_data;
	struct stat sb;
	mng_anim *mng = mng_get_userdata(mngh);

	if ((fd = open(filename, O_RDONLY)) < 0) {
		perror("mng_readfile: open");
		return 0;
	}
	if (fstat(fd, &sb) == -1) {
		perror("mng_readfile: stat");
		goto close_fail;
	}
	mng->len = sb.st_size;

	if ((mng->data = malloc(mng->len)) == NULL) {
		iprint(MSG_ERROR, "mng_readfile: Unable to allocate memory for MNG file\n");
		goto close_fail;
	}

	len = 0;
	file_data = mng->data;
	while (len < mng->len) {
		int ret;
		int amt = 0x10000;
		if (mng->len - len < amt)
			amt = mng->len - len;
		ret = read(fd, file_data, amt); /* read up to 64KB at a time */
		switch (ret) {
			case -1:
				perror("mng_readfile: read");
				goto close_fail;
			case 0:
				iprint(MSG_ERROR, "mng_readfile: Shorter file than expected!\n");
				goto close_fail;
		}
		file_data += ret;
		len += ret;
	}

	close(fd);

	return 1;

close_fail:
	close(fd);
	return 0;
}

mng_handle mng_load(char* filename, int *width, int *height)
{
	mng_handle mngh;
	mng_anim *mng;

	mng = (mng_anim*)malloc(sizeof(mng_anim));
	if (!mng) {
		iprint(MSG_ERROR, "%s: Unable to allocate memory for MNG data\n",
				__FUNCTION__);
		return MNG_NULL;
	}

	memset(mng, 0, sizeof(mng_anim));
	mngh = mng_initialize(mng, fb_mng_memalloc, fb_mng_memfree,
			MNG_NULL);
	if (mngh == MNG_NULL) {
		iprint(MSG_ERROR, "%s: mng_initialize failed\n", __FUNCTION__);
		goto freemem_fail;
	}

	if (mng_init_callbacks(mngh)) {
		print_mng_error(mngh, "mng_init_callbacks failed");
		goto cleanup_fail;
	}

	/* Load the file into memory */
	if (!mng_readfile(mngh, filename))
		goto cleanup_fail;

	/* Read and parse the file */
	if (mng_read(mngh) != MNG_NOERROR) {
		/* Do something about it. */
		print_mng_error(mngh, "mng_read failed");
		goto cleanup_fail;
	}

	mng->num_frames = mng_get_totalframes(mngh);
	*width = mng->canvas_w;
	*height = mng->canvas_h;

	return mngh;

cleanup_fail:
	mng_cleanup(&mngh);
freemem_fail:
	free(mng);
	return MNG_NULL;
}

void mng_done(mng_handle mngh)
{
	mng_cleanup(&mngh);
}

mng_retcode mng_render_next(mng_handle mngh)
{
	mng_anim *mng = mng_get_userdata(mngh);
	mng_retcode ret;
	int last_frame = 0;

	/* last_frame = mng_get_currentframe(mngh) == mng->num_frames; FIXME */
	if (!mng->displayed_first) {
		ret = mng_display(mngh);
		if (ret == MNG_NOERROR || ret == MNG_NEEDTIMERWAIT)
			mng->displayed_first = 1;
	} else
		ret = mng_display_resume(mngh);

	if (last_frame)
		return MNG_NOERROR;

	if (ret == MNG_NEEDTIMERWAIT || ret == MNG_NOERROR)
		return ret;

	print_mng_error(mngh, "mng_display failed");

	return ret;
}

mng_retcode mng_render_proportional(mng_handle mngh, int progress)
{
	mng_anim *mng = mng_get_userdata(mngh);
	mng_retcode ret = MNG_NOERROR;
	int frame_num, current_frame;

	frame_num = ((progress * mng->num_frames) / FBSPL_PROGRESS_MAX) + 1;
	if (!mng->displayed_first) {
		ret = mng_display(mngh);
		mng->displayed_first = 1;
	}

	if (mng->num_frames == 0 && mng->displayed_first)
		return MNG_NOERROR;

	if (frame_num > mng->num_frames)
		frame_num = mng->num_frames;

	current_frame = mng_get_currentframe(mngh);
	if (current_frame == frame_num)
		return ret;

	/* Don't bother freezing if the next frame is just n+1. mng_display_resume
	 * will do this case by default, and it saves us from the horrid hack
	 * below.
	 */
	if (frame_num != current_frame + 1) {
		mng_display_freeze(mngh);

		/* XXX: hack! workaround what seems to be a bug in libmng - it won't
		 * actually repaint the canvas if you wind an animation "forwards"
		 * using goframe, only backwards, so go to the end of the animation first.
		 */

		mng_display_goframe(mngh, mng->num_frames);
		if (frame_num != mng->num_frames)
			mng_display_goframe(mngh, frame_num);
	}

	ret = mng_display_resume(mngh);

	if (ret == MNG_NEEDTIMERWAIT || ret == MNG_NOERROR)
		return ret;

	print_mng_error(mngh, "mng_display failed");

	return ret;
}

void anim_prerender(stheme_t *theme, anim *a, bool force)
{
	obj *o = container_of(a);

	if (!o->visible)
		return;

	if ((a->flags & F_ANIM_METHOD_MASK) == F_ANIM_PROPORTIONAL) {
		if (a->curr_progress == config.progress && !force)
			return;

		a->curr_progress = config.progress;

		int ret = mng_render_proportional(a->mng, config.progress);

		if (ret == MNG_NEEDTIMERWAIT || ret == MNG_NOERROR) {
			blit_add(theme, &o->bnd);
			render_add(theme, o, &o->bnd);
		}
	} else {
		blit_add(theme, &o->bnd);
		render_add(theme, o, &o->bnd);
	}
}

void anim_render(stheme_t *theme, anim *a, rect *re, u8* tg)
{
	rgbacolor *src;
	mng_anim *mng = mng_get_userdata(a->mng);
	obj *o = container_of(a);
	int line;

	if (!o->visible)
		return;

	src = (rgbacolor*)mng->canvas;

	src += mng->canvas_w * (re->y1 - a->y) + (re->x1 - a->x);
	tg += ((theme->xres * re->y1) + re->x1) * fbd.bytespp;

	for (line = re->y1; line <= re->y2; line++) {
		rgba2fb(src, tg, tg,  re->x2 - re->x1 + 1, line, 1);
		tg   += theme->xres * fbd.bytespp;
		src  += mng->canvas_w;
	}

	return;
}

mng_retcode mng_display_restart(mng_handle mngh)
{
	mng_anim *mng = mng_get_userdata(mngh);

	mng->displayed_first = 0;
	return mng_display_reset(mngh);
}
