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
#include <map>

#include "vsource.h"
#include "vconverter.h"
#include "server.h"
#include "rtspserver.h"
#include "encoder-common.h"

#include "ga-common.h"
#include "ga-conf.h"
#include "ga-avcodec.h"

#include "pipeline.h"
#include "filter-rgb2yuv.h"

#define	POOLSIZE	8

using namespace std;
static pthread_mutex_t initMutex = PTHREAD_MUTEX_INITIALIZER;
static map<void*,bool> initialized;
static int outputW;
static int outputH;

int
filter_RGB2YUV_init(void *arg) {
	// arg is image source id
	int iid;
	int iwidth;
	int iheight;
	int istride;
	int resolution[2];
	//char pipename[64];
	const char **filterpipe = (const char **) arg;
	//pipeline *srcpipe = (pipeline*) arg;
	pipeline *srcpipe = pipeline::lookup(filterpipe[0]);
	pipeline *pipe = NULL;
	struct pooldata *data = NULL;
	struct SwsContext *swsctx = NULL;
	//
	//
	map<void*,bool>::iterator mi;
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
	if(srcpipe == NULL) {
		ga_error("RGB2YUV filter: init - NULL pipeline specified (%s).\n", filterpipe[0]);
		goto init_failed;
	}
	iid = ((struct vsource_config*) srcpipe->get_privdata())->id;
	iwidth = video_source_maxwidth(iid);
	iheight = video_source_maxheight(iid);
	istride = video_source_maxstride(iid);
	//
	outputW = iwidth;	// by default, the same as max resolution
	outputH = iheight;
	if(ga_conf_readints("output-resolution", resolution, 2) == 2) {
		outputW = resolution[0];
		outputH = resolution[1];
		ga_error("RGB2YUV filter: output-resolution specified: %dx%d\n",
			outputW, outputH);
	}
	// create default converters
	do {
		char pixelfmt[64];
		if(ga_conf_readv("filter-source-pixelformat", pixelfmt, sizeof(pixelfmt)) != NULL) {
			if(strcasecmp("rgba", pixelfmt) == 0) {
				swsctx = create_frame_converter(
						iwidth, iheight, PIX_FMT_RGBA,
						outputW, outputH, PIX_FMT_YUV420P);
				//swsctx = ga_swscale_init(PIX_FMT_RGBA, iwidth, iheight, iwidth, iheight);
				ga_error("RGB2YUV filter: RGBA source specified.\n");
			} else if(strcasecmp("bgra", pixelfmt) == 0) {
				swsctx = create_frame_converter(
						iwidth, iheight, PIX_FMT_BGRA,
						outputW, outputH, PIX_FMT_YUV420P);
				//swsctx = ga_swscale_init(PIX_FMT_BGRA, iwidth, iheight, iwidth, iheight);
				ga_error("RGB2YUV filter: BGRA source specified.\n");
			}
		}
		if(swsctx == NULL) {
#ifdef __APPLE__
			//swsctx = ga_swscale_init(PIX_FMT_RGBA, iwidth, iheight, iwidth, iheight);
			swsctx = create_frame_converter(
					iwidth, iheight, PIX_FMT_RGBA,
					outputW, outputH, PIX_FMT_YUV420P);
#else
			//swsctx = ga_swscale_init(PIX_FMT_BGRA, iwidth, iheight, iwidth, iheight);
			swsctx = create_frame_converter(
					iwidth, iheight, PIX_FMT_BGRA,
					outputW, outputH, PIX_FMT_YUV420P);
#endif
		}
	} while(0);
	//
	if(swsctx == NULL) {
		ga_error("RGB2YUV filter: cannot initialize swsscale.\n");
		goto init_failed;
	}
	//
	if((pipe = new pipeline()) == NULL) {
		ga_error("RGB2YUV filter: init pipeline failed.\n");
		goto init_failed;
	}
	// has privdata from the source?
	if(srcpipe->get_privdata_size() > 0) {
		if(pipe->alloc_privdata(srcpipe->get_privdata_size()) == NULL) {
			ga_error("RGB2YUV filter: cannot allocate privdata.\n");
			goto init_failed;
		}
		pipe->set_privdata(srcpipe->get_privdata(), srcpipe->get_privdata_size());
	}
	//
	if((data = pipe->datapool_init(POOLSIZE, sizeof(struct vsource_frame))) == NULL) {
		ga_error("RGB2YUV filter: cannot allocate data pool.\n");
		goto init_failed;
	}
	// per frame init
	for(; data != NULL; data = data->next) {
		if(vsource_frame_init((struct vsource_frame*) data->ptr, iwidth, iheight, istride) == NULL) {
			ga_error("RGB2YUV filter: init frame failed.\n");
			goto init_failed;
		}
	}
	//
	pipeline::do_register(filterpipe[1], pipe);
	//
	pthread_mutex_lock(&initMutex);
	initialized[arg] = true;
	pthread_mutex_unlock(&initMutex);
	//
	return 0;
init_failed:
	if(pipe) {
		delete pipe;
	}
	return -1;
}

void *
filter_RGB2YUV_threadproc(void *arg) {
	// arg is pointer to source pipe
	//char pipename[64];
	const char **filterpipe = (const char **) arg;
	//pipeline *srcpipe = (pipeline*) arg;
	pipeline *srcpipe = pipeline::lookup(filterpipe[0]);
	pipeline *dstpipe = NULL;
	struct pooldata *srcdata = NULL;
	struct pooldata *dstdata = NULL;
	struct vsource_frame *srcframe = NULL;
	struct vsource_frame *dstframe = NULL;
	// image info
	//int istride = video_source_maxstride();
	//
	unsigned char *src[] = { NULL, NULL, NULL, NULL };
	unsigned char *dst[] = { NULL, NULL, NULL, NULL };
	int srcstride[] = { 0, 0, 0, 0 };
	int dststride[] = { 0, 0, 0, 0 };
	//
	struct SwsContext *swsctx = NULL;
	//
	pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	//
	if(srcpipe == NULL) {
		ga_error("RGB2YUV filter: NULL pipeline specified.\n");
		goto filter_quit;
	}
	// init variables
	if((dstpipe = pipeline::lookup(filterpipe[1])) == NULL) {
		ga_error("RGB2YUV filter: cannot find pipeline '%s'\n", filterpipe[1]/*pipename*/);
		goto filter_quit;
	}
	//
	ga_error("RGB2YUV filter: pipe from '%s' to '%s' (output-resolution=%dx%d)\n",
		srcpipe->name(), dstpipe->name(),
		outputW/*iwidth*/, outputH/*iheight*/);
	//
	srcpipe->client_register(ga_gettid(), &cond);
	// start filtering
	ga_error("RGB2YUV filter started: tid=%ld.\n", ga_gettid());
	//
	while(true) {
		// wait for notification
		srcdata = srcpipe->load_data();
		if(srcdata == NULL) {
			srcpipe->wait(&cond, &condMutex);
			srcdata = srcpipe->load_data();
			if(srcdata == NULL) {
				ga_error("RGB2YUV filter: unexpected NULL frame received (from '%s', data=%d, buf=%d).\n",
					srcpipe->name(), srcpipe->data_count(), srcpipe->buf_count());
				exit(-1);
				// should never be here
				goto filter_quit;
			}
		}
		srcframe = (struct vsource_frame*) srcdata->ptr;
		//
		dstdata = dstpipe->allocate_data();
		dstframe = (struct vsource_frame*) dstdata->ptr;
		// basic info
		dstframe->imgpts = srcframe->imgpts;
		dstframe->pixelformat = PIX_FMT_YUV420P;	//yuv420p;
		// scale image: XXX: RGBA or BGRA
		if(srcframe->pixelformat == PIX_FMT_RGBA
		|| srcframe->pixelformat == PIX_FMT_BGRA/*rgba*/) {
			swsctx = lookup_frame_converter(
					srcframe->realwidth,
					srcframe->realheight,
					srcframe->pixelformat);
			if(swsctx == NULL) {
				swsctx = create_frame_converter(
					srcframe->realwidth,
					srcframe->realheight,
					srcframe->pixelformat,
					outputW,
					outputH,
					PIX_FMT_YUV420P);
			}
			if(swsctx == NULL) {
				ga_error("RGB2YUV filter: fatal - cannot create frame converter (%d,%d)->(%x,%d)\n",
					srcframe->realwidth, srcframe->realheight,
					outputW, outputH);
			}
			src[0] = srcframe->imgbuf;
			src[1] = NULL;
			srcstride[0] = srcframe->realstride; //srcframe->stride;
			srcstride[1] = 0;
			dst[0] = dstframe->imgbuf;
			dst[1] = dstframe->imgbuf + outputH*outputW;
			dst[2] = dstframe->imgbuf + outputH*outputW + (outputH*outputW>>2);
			dst[3] = NULL;
			dstframe->linesize[0] = dststride[0] = outputW;
			dstframe->linesize[1] = dststride[1] = outputW>>1;
			dstframe->linesize[2] = dststride[2] = outputW>>1;
			dstframe->linesize[3] = dststride[3] = 0;
			sws_scale(swsctx,
				src, srcstride, 0, srcframe->realheight,
				dst, dstframe->linesize);
		}
		srcpipe->release_data(srcdata);
		dstpipe->store_data(dstdata);
		dstpipe->notify_all();
		//
	}
	//
filter_quit:
	if(srcpipe) {
		srcpipe->client_unregister(ga_gettid());
		srcpipe = NULL;
	}
	if(dstpipe) {
		delete dstpipe;
		dstpipe = NULL;
	}
	//
	if(swsctx)	sws_freeContext(swsctx);
	//
	ga_error("RGB2YUV filter: thread terminated.\n");
	//
	return NULL;
}

