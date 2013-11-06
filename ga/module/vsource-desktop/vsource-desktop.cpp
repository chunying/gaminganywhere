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
#include <pthread.h>
#ifndef WIN32
#include <sys/time.h>
#endif

#include <map>

#include "server.h"
#include "vsource.h"
#include "pipeline.h"
#include "encoder-common.h"

#include "ga-common.h"

#ifdef WIN32
#include "ga-win32-common.h"
#ifdef D3D_CAPTURE
#include "ga-win32-d3d.h"
#elif defined DFM_CAPTURE
#include "ga-win32-dfm.h"
#else
#include "ga-win32-gdi.h"
#endif
#elif defined __APPLE__
#include "ga-osx.h"
#else
#include "ga-xwin.h"
#endif

#include "ga-avcodec.h"

#include "vsource-desktop.h"

using namespace std;
static pthread_mutex_t initMutex = PTHREAD_MUTEX_INITIALIZER;
static map<void*,bool> initialized;

static struct gaRect croprect;
static struct gaRect *prect = NULL;
static int screenwidth, screenheight;

#define	SOURCES	1

static struct gaImage realimage, *image = &realimage;

int
vsource_init(void *arg) {
	struct RTSPConf *rtspconf = rtspconf_global();
	//const char *pipeformat = (const char *) arg;
	void **ptr = (void**) arg;
	const char *pipeformat = (const char *) ptr[0];
	struct gaRect *rect = (struct gaRect*) ptr[1];
	//
	map<void*,bool>::iterator mi;
	//
	pthread_mutex_lock(&initMutex);
	if((mi = initialized.find(arg)) != initialized.end()) {
		if(mi->second != false) {
			// has been initialized
			pthread_mutex_unlock(&initMutex);
			return 0;
		}
	}
	pthread_mutex_unlock(&initMutex);
	//
	if(rect != NULL) {
		if(ga_fillrect(&croprect, rect->left, rect->top, rect->right, rect->bottom) == NULL) {
			ga_error("image source: invalid rect (%d,%d)-(%d,%d)\n",
				rect->left, rect->top,
				rect->right, rect->bottom);
			return -1;
		}
		prect = &croprect;
	} else {
		prect = NULL;
	}
	//
#ifdef WIN32
	// prevent GetSystemMetrics() get worng numbers
	// XXX: currently disabled
	//SetProcessDPIAware();
	#ifdef D3D_CAPTURE
	if(ga_win32_D3D_init(image) < 0) {
		ga_error("WIN32-D3D capture init failed.\n");
		return -1;
	}
	#elif defined DFM_CAPTURE
	if(ga_win32_DFM_init(image) < 0) {
		ga_error("WIN32-DFM capture init failed.\n");
		return -1;
	}
	#else
	if(ga_win32_GDI_init(image) < 0) {
		ga_error("WIN32-GDI capture init failed.\n");
		return -1;
	}
	#endif	/* D3D_CAPTURE */
#elif defined __APPLE__
	if(ga_osx_init(image) < 0) {
		ga_error("Mac OS X capture init failed.\n");
		return -1;
	}
#else
	//if(ga_xwin_init(rtspconf->display, &display, &rootWindow, &image) < 0) {
	if(ga_xwin_init(rtspconf->display, image) < 0) {
		ga_error("XWindow capture init failed.\n");
		return -1;
	}
#endif

	screenwidth = image->width;
	screenheight = image->height;

#ifdef SOURCES
	do {
		int i;
		vsource_config config[SOURCES];
		bzero(config, sizeof(config));
		for(i = 0; i < SOURCES; i++) {
			config[i].rtp_id = i;
			config[i].maxwidth = prect ? prect->width : image->width;
			config[i].maxheight = prect ? prect->height : image->height;
			config[i].maxstride = prect ? prect->linesize : image->bytes_per_line;
		}
		if(video_source_setup_ex(pipeformat, config, SOURCES) < 0) {
			return -1;
		}
	} while(0);
#else
	if(video_source_setup(pipeformat, 0,
			prect ? prect->width : image->width,
			prect ? prect->height : image->height,
			prect ? prect->linesize : image->bytes_per_line) < 0) {
		return -1;
	}
#endif
	//
	pthread_mutex_lock(&initMutex);
	initialized[arg] = true;
	pthread_mutex_unlock(&initMutex);
	//
	return 0;
}

void *
vsource_threadproc(void *arg) {
	int i;
	int frame_interval;
	struct timeval tv;
	struct pooldata *data;
	struct vsource_frame *frame;
	pipeline *pipe[SOURCES];
#ifdef WIN32
	LARGE_INTEGER initialTv, captureTv, freq;
#else
	struct timeval initialTv, captureTv;
#endif
	const char *pipeformat = (const char *) arg;
	//void **ptr = (void**) arg;
	//const char *pipeformat = (const char *) ptr[0];
	//struct gaRect *rect = (struct gaRect*) ptr[1];
	//
	struct RTSPConf *rtspconf = rtspconf_global();
	//
	frame_interval = 1000000/rtspconf->video_fps;	// in the unif of us
	frame_interval++;
	//
	for(i = 0; i < SOURCES; i++) {
		char pipename[64];
		snprintf(pipename, sizeof(pipename), pipeformat, i);
		if((pipe[i] = pipeline::lookup(pipename)) == NULL) {
			ga_error("image source: cannot find pipeline '%s'\n", pipename);
			exit(-1);
		}
	}
	//
	ga_error("Image source thread started: tid=%ld\n", ga_gettid());
#ifdef WIN32
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&initialTv);
#else
	gettimeofday(&initialTv, NULL);
#endif
	while(true) {
		//
		gettimeofday(&tv, NULL);
		//if(pipe->client_count() <= 0) {
		if(encoder_running() == 0) {
#ifdef WIN32
			Sleep(1);
#else
			usleep(1000);
#endif
			continue;
		}
		// copy image 
		data = pipe[0]->allocate_data();
		frame = (struct vsource_frame*) data->ptr;
#ifdef __APPLE__
		frame->pixelformat = PIX_FMT_RGBA;
#else
		frame->pixelformat = PIX_FMT_BGRA;
#endif
		if(prect == NULL) {
		////////////////////////////////////////
		frame->realwidth = screenwidth;
		frame->realheight = screenheight;
		frame->realstride = screenwidth<<2;
		frame->realsize = screenheight * frame->realstride;
		////////////////////////////////////////
		} else {
		////////////////////////////////////////
		frame->realwidth = prect->width;
		frame->realheight = prect->height;
		frame->realstride = prect->width<<2;
		frame->realsize = prect->height * frame->realstride;
		////////////////////////////////////////
		}
		frame->linesize[0] = frame->realstride/*frame->stride*/;
#ifdef WIN32
		QueryPerformanceCounter(&captureTv);
	#ifdef D3D_CAPTURE
		ga_win32_D3D_capture((char*) frame->imgbuf, frame->imgbufsize, prect);
	#elif defined DFM_CAPTURE
		ga_win32_DFM_capture((char*) frame->imgbuf, frame->imgbufsize, prect);
	#else
		ga_win32_GDI_capture((char*) frame->imgbuf, frame->imgbufsize, prect);
	#endif
#elif defined __APPLE__
		gettimeofday(&captureTv, NULL);
		ga_osx_capture((char*) frame->imgbuf, frame->imgbufsize, prect);
#else // X11
		gettimeofday(&captureTv, NULL);
		ga_xwin_capture((char*) frame->imgbuf, frame->imgbufsize, prect);
#endif
		// draw cursor
#ifdef WIN32
		ga_win32_draw_system_cursor(frame);
#endif
		//gImgPts++;
#ifdef WIN32
		frame->imgpts = pcdiff_us(captureTv, initialTv, freq)/frame_interval;
#else
		frame->imgpts = tvdiff_us(&captureTv, &initialTv)/frame_interval;
#endif
		// duplicate from channel 0 to other channels
		for(i = 1; i < SOURCES; i++) {
			struct pooldata *dupdata;
			struct vsource_frame *dupframe;
			dupdata = pipe[i]->allocate_data();
			dupframe = (struct vsource_frame*) dupdata->ptr;
			//
			vsource_dup_frame(frame, dupframe);
			//
			pipe[i]->store_data(dupdata);
			pipe[i]->notify_all();
		}
		pipe[0]->store_data(data);
		pipe[0]->notify_all();
		//
		ga_usleep(frame_interval, &tv);
	}
	//
	ga_error("image capture thread terminated.\n");
	//
	return NULL;
}

void
vsource_deinit(void *arg) {
#ifdef WIN32
	#ifdef D3D_CAPTURE
	ga_win32_D3D_deinit();
	#elif defined DFM_CAPTURE
	ga_win32_DFM_deinit();
	#else
	ga_win32_GDI_deinit();
	#endif /* D3D_CAPTURE */
	CoUninitialize();
#elif defined __APPLE__
	ga_osx_deinit();
#else
	//ga_xwin_deinit(display, image);
	ga_xwin_deinit();
#endif
	return;
}

