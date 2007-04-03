/*
 * mng_callbacks.c - Callback functions for libmng
 *
 * Copyright (C) 2005 Bernard Blackham <bernard@blackham.com.au>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include <libmng.h>
#include <sys/time.h>
#include <time.h>
#include "util.h"

mng_ptr fb_mng_memalloc(mng_size_t len)
{
	return calloc(1, len);
}

void fb_mng_memfree(mng_ptr p, mng_size_t len)
{
	free(p);
}

static mng_bool fb_mng_openstream(mng_handle handle)
{
	mng_anim *mng = mng_get_userdata(handle);

	if (mng->data == NULL || mng->len == 0)
		return MNG_FALSE;

	mng->ptr = 0;
	mng->open = 1;

	return MNG_TRUE;
}

static mng_bool fb_mng_closestream(mng_handle handle)
{
	mng_anim *mng = mng_get_userdata(handle);

	mng->open = 0;
	return MNG_TRUE;
}

static mng_bool fb_mng_readdata(mng_handle handle, mng_ptr buf, 
		mng_uint32 len, mng_uint32p pread)
{
	mng_anim *mng = mng_get_userdata(handle);
	char *src_buf;
	
	if (mng->data == NULL || !mng->open)
		return MNG_FALSE;

	src_buf = ((char*)mng->data) + mng->ptr;

	if (mng->ptr + len > mng->len)
		len = mng->len - mng->ptr;

	memcpy(buf, src_buf, len);
	if (pread)
		*pread = len;

	mng->ptr += len;
	
	return MNG_TRUE;
}

static mng_ptr fb_mng_getcanvasline(mng_handle handle, mng_uint32 line_num)
{
	mng_anim *mng = mng_get_userdata(handle);

	if (mng->canvas == NULL || line_num >= mng->canvas_h) {
		fprintf(stderr, "%s(mngh, %d): Requested invalid line or canvas was NULL.\n",
				__FUNCTION__, line_num);
		return MNG_NULL;
	}

	return mng->canvas + (line_num * mng->canvas_w * mng->canvas_bytes_pp);
}

static mng_bool fb_mng_refresh(mng_handle handle, mng_uint32 x, mng_uint32 y,
		mng_uint32 width, mng_uint32 height)
{

	/* FIXME */
	return MNG_TRUE;
}

static mng_uint32 fb_mng_gettickcount(mng_handle handle)
{
	struct timeval tv;
	mng_anim *mng = mng_get_userdata(handle);

	if (gettimeofday(&tv, NULL) < 0) {
		perror("fb_mng_gettickcount: gettimeofday");
		abort();
	}

	if (mng->start_time.tv_sec == 0) {
		mng->start_time.tv_sec = tv.tv_sec;
		mng->start_time.tv_usec = tv.tv_usec;
	}

	return ((tv.tv_sec - mng->start_time.tv_sec)*1000) +
		((tv.tv_usec - mng->start_time.tv_usec)/1000);
}

static mng_bool fb_mng_settimer(mng_handle handle, mng_uint32 msecs)
{
	mng_anim *mng = mng_get_userdata(handle);

	mng->wait_msecs = msecs;
	return MNG_TRUE;
}

static mng_bool fb_mng_processheader(mng_handle handle, mng_uint32 width,
		mng_uint32 height)
{
	mng_anim *mng = mng_get_userdata(handle);

	free(mng->canvas);

	mng->canvas_bytes_pp = 4;

	if ((mng->canvas = malloc(width*height*mng->canvas_bytes_pp)) == NULL) {
		fprintf(stderr, "%s: Unable to allocate memory for MNG canvas\n",
				__FUNCTION__);
		return MNG_FALSE;
	}
	mng->canvas_w = width;
	mng->canvas_h = height;

	mng_set_canvasstyle(handle, MNG_CANVAS_RGBA8);
#if 0
	mng_set_bgcolor(handle, 0, 0, 0); /* FIXME - make configurable? */
#endif
	return MNG_TRUE;
}

static mng_bool fb_mng_errorproc(mng_handle handler, mng_int32 code,
		mng_int8 severity, mng_chunkid chunkname, mng_uint32 chunkseq,
		mng_int32 extra1, mng_int32 extra2, mng_pchar errtext)
{
	fprintf(stderr, "libmng error: Code: %d, Severity: %d - %s\n",
			code, severity, errtext);
	abort();
	return MNG_TRUE;
}

#ifdef MNG_SUPPORT_TRACE
static mng_bool fb_mng_traceproc(mng_handle handle, mng_int32 funcnr,
		mng_int32 seq, mng_pchar funcname)
{
	fprintf(stderr, "libmng trace: %s (seq %d\n)", funcname, seq);
	return MNG_TRUE;

}
#endif

mng_retcode mng_init_callbacks(mng_handle handle)
{
	mng_retcode ret;

#define set_cb(x) \
		if ((ret = mng_setcb_##x(handle, fb_mng_##x)) != MNG_NOERROR) \
			return ret;

	set_cb(errorproc);
	set_cb(openstream);
	set_cb(closestream);
	set_cb(readdata);
	set_cb(getcanvasline);
	set_cb(refresh);
	set_cb(gettickcount);
	set_cb(settimer);
	set_cb(processheader);
#ifdef MNG_SUPPORT_TRACE
	set_cb(traceproc);
#endif

#undef set_cb
	return MNG_NOERROR;
}
