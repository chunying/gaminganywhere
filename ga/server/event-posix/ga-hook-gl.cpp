/*
 * Copyright (c) 2013 Chun-Ying Huang
 *
 * This file is part of GamingAnywhere (GA).
 *
 * GA is free software; you can redistribute it and/or modify it
 * under the terms of the 3-clause BSD License as published by the
 * Free Software Foundation: http://directory.fsf.org/wiki/License:BSD_3Clause
 *
 * GA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the 3-clause BSD License along with GA;
 * if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#ifndef WIN32
#include <dlfcn.h>
#endif

#include "ga-common.h"
#include "ga-conf.h"
#include "vsource.h"
#include "dpipe.h"

#include "ga-hook-common.h"
#include "ga-hook-gl.h"
#ifndef WIN32
#include "ga-hook-lib.h"
#endif

#include <map>
using namespace std;

#ifndef WIN32
#ifdef __cplusplus
extern "C" {
#endif
void glFlush();
#ifdef __cplusplus
}
#endif
#endif

// for hooking
t_glFlush		old_glFlush = NULL;

static void
gl_global_init() {
#ifndef WIN32
	static int initialized = 0;
	pthread_t t;
	if(initialized != 0)
		return;
	//
	if(pthread_create(&t, NULL, ga_server, NULL) != 0) {
		ga_error("ga_hook: create thread failed.\n");
		exit(-1);
	}

	initialized = 1;
#endif
	return;
}

static void
gl_hook_symbols() {
#ifndef WIN32
	void *handle = NULL;
	char *ptr, soname[2048];
	if((ptr = getenv("LIBVIDEO")) == NULL) {
		strncpy(soname, "libGL.so.1", sizeof(soname));
	} else {
		strncpy(soname, ptr, sizeof(soname));
	}
	if((handle = dlopen(soname, RTLD_LAZY)) == NULL) {
		ga_error("dlopen '%s' failed: %s\n", soname, strerror(errno));
		exit(-1);
	}
	// for hooking
	old_glFlush = (t_glFlush)
				ga_hook_lookup_or_quit(handle, "glFlush");
	ga_error("hook-gl: hooked.\n");
	// indirect hook
	if((ptr = getenv("HOOKVIDEO")) == NULL)
		goto quit;
	strncpy(soname, ptr, sizeof(soname));
	if((handle = dlopen(soname, RTLD_LAZY)) != NULL) {
		hook_lib_generic(soname, handle, "glFlush", (void*) hook_glFlush);
	}
	ga_error("hook-gl: hooked into %s.\n", soname);
quit:
#endif
	return;
}

void
#ifdef WIN32
WINAPI
#endif
hook_glFlush() {
	static int frame_interval;
	static struct timeval initialTv, captureTv;
	static int frameLinesize;
	static unsigned char *frameBuf;
	static int sb_initialized = 0;
	static int global_initialized = 0;
	//
	GLint vp[4];
	int vp_x, vp_y, vp_width, vp_height;
	int i;
	//
	dpipe_buffer_t *data;
	vsource_frame_t *frame;
	//
	if(global_initialized == 0) {
		gl_global_init();
		global_initialized = 1;
	}
	//
	if(old_glFlush == NULL) {
		gl_hook_symbols();
	}
	old_glFlush();
	// capture the screen
	glGetIntegerv(GL_VIEWPORT, vp);
	vp_x = vp[0];
	vp_y = vp[1];
	vp_width = vp[2];
	vp_height = vp[3];		
	//
	if(vp_width < 16 || vp_height < 16) {
		return;
	}
	//
	ga_error("XXX hook_gl: viewport (%d,%d)-(%d,%d)\n",
		vp_x, vp_y, vp_width, vp_height);
	//
	if(ga_hook_capture_prepared(vp_width, vp_height, 1) < 0)
		return;
	//
	if(sb_initialized == 0) {
		frame_interval = 1000000/video_fps; // in the unif of us
		frame_interval++;
		gettimeofday(&initialTv, NULL);
		frameBuf = (unsigned char*) malloc(encoder_width * encoder_height * 4);
		if(frameBuf == NULL) {
			ga_error("allocate frame failed.\n");
			return;
		}
		frameLinesize = game_width * 4;
		sb_initialized = 1;
	} else {
		gettimeofday(&captureTv, NULL);
	}
	//
	if (enable_server_rate_control && ga_hook_video_rate_control() < 0) {
		return;
	}
	//
	do {
		unsigned char *src, *dst;
		//
		frameLinesize = game_width<<2;
		//
		data = dpipe_get(g_pipe[0]);
		frame = (vsource_frame_t*) data->pointer;
		frame->pixelformat = AV_PIX_FMT_RGBA;
		frame->realwidth = game_width;
		frame->realheight = game_height;
		frame->realstride = frameLinesize;
		frame->realsize = game_height * frameLinesize;
		frame->linesize[0] = frameLinesize;/*frame->stride*/;
		// read a block of pixels from the framebuffer (backbuffer)
		glReadBuffer(GL_BACK);
		glReadPixels(0, 0, game_width, game_height, GL_RGBA, GL_UNSIGNED_BYTE, frameBuf);
		// image is upside down!
		src = frameBuf + frameLinesize * (game_height - 1);
		dst = frame->imgbuf;
		for(i = 0; i < frame->realheight; i++) {
			bcopy(src, dst, frameLinesize);
			dst += frameLinesize/*frame->stride*/;
			src -= frameLinesize;
		}
		frame->imgpts = tvdiff_us(&captureTv, &initialTv)/frame_interval;
		frame->timestamp = captureTv;
	} while(0);
	// duplicate from channel 0 to other channels
	ga_hook_capture_dupframe(frame);
	dpipe_store(g_pipe[0], data);
	//
	return;
}

#ifndef WIN32	/* POSIX interfaces */
void
glFlush() {
	hook_glFlush();
}

__attribute__((constructor))
static void
gl_hook_loaded(void) {
	ga_error("ga-hook-gl loaded!\n");
	if(ga_hook_init() < 0) {
		ga_error("ga_hook: init failed.\n");
		exit(-1);
	}
	return;
}
#endif /* ! WIN32 */

