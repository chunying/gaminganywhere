/*
 * Copyright (c) 2013-2014 Chun-Ying Huang
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
#include "rtspconf.h"
#include "encoder-common.h"

#include "ga-common.h"
#include "ga-conf.h"
#include "ga-avcodec.h"

#include "dpipe.h"
#include "dpipe.h"
#include "filter-rgb2yuv.h"

#define	POOLSIZE		8
#define	ENABLE_EMBED_COLORCODE	1

using namespace std;

static int filter_initialized = 0;
static int filter_started = 0;
static pthread_t filter_tid[VIDEO_SOURCE_CHANNEL_MAX];
static FILE *savefp = NULL;

/* filter_RGB2YUV_init: arg is two pointers to pipeline format string */
/*	1st ptr: source pipeline */
/*	2nd ptr: destination pipeline */
/* the number of pipeline(s) is equivalut to the number of video source(s) */

static int
filter_RGB2YUV_init(void *arg) {
	// arg is image source id
	int iid;
	const char **filterpipe = (const char **) arg;
	dpipe_t *srcpipe[VIDEO_SOURCE_CHANNEL_MAX];
	dpipe_t *dstpipe[VIDEO_SOURCE_CHANNEL_MAX];
	char savefile[128];
	//
	if(filter_initialized != 0)
		return 0;
	//
	if(ga_conf_readv("save-yuv-image", savefile, sizeof(savefile)) != NULL) {
		savefp = ga_save_init(savefile);
	}
#ifdef ENABLE_EMBED_COLORCODE
	vsource_embed_colorcode_init(0/*RGBmode*/);
#endif
	//
	bzero(dstpipe, sizeof(dstpipe));
	//
	for(iid = 0; iid < video_source_channels(); iid++) {
		char pixelfmt[64];
		char srcpipename[64], dstpipename[64];
		int inputW, inputH, outputW, outputH;
		struct SwsContext *swsctx = NULL;
		dpipe_buffer_t *data = NULL;
		//
		snprintf(srcpipename, sizeof(srcpipename), filterpipe[0], iid);
		snprintf(dstpipename, sizeof(dstpipename), filterpipe[1], iid);
		srcpipe[iid] = dpipe_lookup(srcpipename);
		if(srcpipe[iid] == NULL) {
			ga_error("RGB2YUV filter: cannot find pipe %s\n", srcpipename);
			goto init_failed;
		}
		inputW = video_source_curr_width(iid);
		inputH = video_source_curr_height(iid);
		outputW = video_source_out_width(iid);
		outputH = video_source_out_height(iid);
		// create default converters
		if(ga_conf_readv("filter-source-pixelformat", pixelfmt, sizeof(pixelfmt)) != NULL) {
			if(strcasecmp("rgba", pixelfmt) == 0) {
				swsctx = create_frame_converter(
						inputW, inputH, PIX_FMT_RGBA,
						outputW, outputH, PIX_FMT_YUV420P);
				ga_error("RGB2YUV filter: RGBA source specified.\n");
			} else if(strcasecmp("bgra", pixelfmt) == 0) {
				swsctx = create_frame_converter(
						inputW, inputH, PIX_FMT_BGRA,
						outputW, outputH, PIX_FMT_YUV420P);
				ga_error("RGB2YUV filter: BGRA source specified.\n");
			} else if(strcasecmp("yuv420p", pixelfmt) == 0) {
				swsctx = create_frame_converter(
						inputW, inputH, PIX_FMT_YUV420P,
						outputW, outputH, PIX_FMT_YUV420P);
				ga_error("RGB2YUV filter: YUV source specified.\n");
			}
		}
		if(swsctx == NULL) {
#ifdef __APPLE__
			swsctx = create_frame_converter(
					inputW, inputH, PIX_FMT_RGBA,
					outputW, outputH, PIX_FMT_YUV420P);
#else
			swsctx = create_frame_converter(
					inputW, inputH, PIX_FMT_BGRA,
					outputW, outputH, PIX_FMT_YUV420P);
#endif
		}
		if(swsctx == NULL) {
			ga_error("RGB2YUV filter: cannot initialize converters.\n");
			goto init_failed;
		}
		//
		dstpipe[iid] = dpipe_create(iid, dstpipename, POOLSIZE,
				sizeof(vsource_frame_t) + video_source_mem_size(iid));
		if(dstpipe[iid] == NULL) {
			ga_error("RGB2YUV filter: create dst-pipeline failed (%s).\n", dstpipename);
			goto init_failed;
		}
		for(data = dstpipe[iid]->in; data != NULL; data = data->next) {
			if(vsource_frame_init(iid, (vsource_frame_t*) data->pointer) == NULL) {
				ga_error("RGB2YUV filter: init frame failed for %s.\n", dstpipename);
				goto init_failed;
			}
		}
		video_source_add_pipename(iid, dstpipename);
	}
	//
	filter_initialized = 1;
	//
	return 0;
init_failed:
	for(iid = 0; iid < video_source_channels(); iid++) {
		if(dstpipe[iid] != NULL)
			dpipe_destroy(dstpipe[iid]);
		dstpipe[iid] = NULL;
	}
#if 0
	if(pipe) {
		delete pipe;
	}
#endif
	return -1;
}

static int
filter_RGB2YUV_deinit(void *arg) {
	if(savefp != NULL) {
		ga_save_close(savefp);
		savefp = NULL;
	}
	return 0;
}

/* filter_RGB2YUV_threadproc: arg is two pointers to pipeline name */
/*	1st ptr: source pipeline */
/*	2nd ptr: destination pipeline */

static void *
filter_RGB2YUV_threadproc(void *arg) {
	// arg is pointer to source pipe
	//char pipename[64];
	const char **filterpipe = (const char **) arg;
	dpipe_t *srcpipe = dpipe_lookup(filterpipe[0]);
	dpipe_t *dstpipe = dpipe_lookup(filterpipe[1]);
	dpipe_buffer_t *srcdata = NULL;
	dpipe_buffer_t *dstdata = NULL;
	vsource_frame_t *srcframe = NULL;
	vsource_frame_t *dstframe = NULL;
	// image info
	//int istride = video_source_maxstride();
	//
	unsigned char *src[] = { NULL, NULL, NULL, NULL };
	unsigned char *dst[] = { NULL, NULL, NULL, NULL };
	int srcstride[] = { 0, 0, 0, 0 };
	int dststride[] = { 0, 0, 0, 0 };
	int iid;
	int outputW, outputH;
	//
	struct SwsContext *swsctx = NULL;
	//
	pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	//
	if(srcpipe == NULL || dstpipe == NULL) {
		ga_error("RGB2YUV filter: bad pipeline (src=%p; dst=%p).\n", srcpipe, dstpipe);
		goto filter_quit;
	}
#ifdef ENABLE_EMBED_COLORCODE
	vsource_embed_colorcode_reset();
#endif
	//
	iid = dstpipe->channel_id;
	outputW = video_source_out_width(iid);
	outputH = video_source_out_height(iid);
	//
	ga_error("RGB2YUV filter[%ld]: pipe#%d from '%s' to '%s' (output-resolution=%dx%d)\n",
		ga_gettid(), iid,
		srcpipe->name, dstpipe->name,
		outputW/*iwidth*/, outputH/*iheight*/);
	// start filtering
	while(filter_started != 0) {
		// wait for notification
		srcdata = dpipe_load(srcpipe, NULL);
		if(srcdata == NULL) {
			ga_error("RGB2YUV filter: unexpected NULL frame received (from '%s', data=%d, buf=%d).\n",
				srcpipe->name, srcpipe->out_count, srcpipe->in_count);
			exit(-1);
			// should never be here
			goto filter_quit;
		}
		srcframe = (vsource_frame_t*) srcdata->pointer;
		//
		dstdata = dpipe_get(dstpipe);
		dstframe = (vsource_frame_t*) dstdata->pointer;
		// basic info
		dstframe->imgpts = srcframe->imgpts;
		dstframe->timestamp = srcframe->timestamp;
		dstframe->pixelformat = PIX_FMT_YUV420P;	//yuv420p;
		dstframe->realwidth = outputW;
		dstframe->realheight = outputH;
		dstframe->realstride = outputW;
		dstframe->realsize = outputW * outputH * 3 / 2;
		// scale image: RGBA, BGRA, or YUV
		swsctx = lookup_frame_converter(
				srcframe->realwidth,
				srcframe->realheight,
				srcframe->pixelformat,
				dstframe->realwidth,
				dstframe->realheight,
				dstframe->pixelformat);
		if(swsctx == NULL) {
			swsctx = create_frame_converter(
				srcframe->realwidth,
				srcframe->realheight,
				srcframe->pixelformat,
				dstframe->realwidth,
				dstframe->realheight,
				dstframe->pixelformat);
		}
		if(swsctx == NULL) {
			ga_error("RGB2YUV filter: fatal - cannot create frame converter (%d,%d,%d)->(%x,%d,%d)\n",
				srcframe->realwidth, srcframe->realheight, srcframe->pixelformat,
				dstframe->realwidth, dstframe->realheight, dstframe->pixelformat);
		}
		//
		if(srcframe->pixelformat == PIX_FMT_RGBA
		|| srcframe->pixelformat == PIX_FMT_BGRA/*rgba*/) {
			src[0] = srcframe->imgbuf;
			src[1] = NULL;
			srcstride[0] = srcframe->realstride; //srcframe->stride;
			srcstride[1] = 0;
		} else if(srcframe->pixelformat == PIX_FMT_YUV420P) {
			src[0] = srcframe->imgbuf;
			src[1] = src[0] + ((srcframe->realwidth * srcframe->realheight));
			src[2] = src[1] + ((srcframe->realwidth * srcframe->realheight)>>2);
			src[3] = NULL;
			srcstride[0] = srcframe->linesize[0];
			srcstride[1] = srcframe->linesize[1];
			srcstride[2] = srcframe->linesize[2];
			srcstride[3] = NULL;
		} else {
			ga_error("filter-RGB2YUV: unsupported pixel format (%d)\n", srcframe->pixelformat);
			exit(-1);
		}
		//
		dst[0] = dstframe->imgbuf;
		dst[1] = dstframe->imgbuf + outputH*outputW;
		dst[2] = dstframe->imgbuf + outputH*outputW + (outputH*outputW>>2);
		dst[3] = NULL;
		dstframe->linesize[0] = dststride[0] = outputW;
		dstframe->linesize[1] = dststride[1] = outputW>>1;
		dstframe->linesize[2] = dststride[2] = outputW>>1;
		dstframe->linesize[3] = dststride[3] = 0;
		//
		sws_scale(swsctx,
			src, srcstride, 0, srcframe->realheight,
			dst, dstframe->linesize);
		// embed first, and then save
#ifdef ENABLE_EMBED_COLORCODE
		vsource_embed_colorcode_inc(dstframe);
#endif
		// only save the first channel
		if(iid == 0 && savefp != NULL) {
			ga_save_yuv420p(savefp, outputW, outputH, dst, dstframe->linesize);
		}
		//
		dpipe_put(srcpipe, srcdata);
		dpipe_store(dstpipe, dstdata);
		//
	}
	//
filter_quit:
	if(srcpipe) {
		srcpipe = NULL;
	}
	if(dstpipe) {
		dpipe_destroy(dstpipe);
		dstpipe = NULL;
	}
	//
	if(swsctx)	sws_freeContext(swsctx);
	//
	ga_error("RGB2YUV filter: thread terminated.\n");
	//
	return NULL;
}

/* filter_RGB2YUV_start: arg is two pointers to pipeline format string */
/*	1st ptr: source pipeline */
/*	2nd ptr: destination pipeline */
/* the number of pipeline(s) is equivalut to the number of video source(s) */

static int
filter_RGB2YUV_start(void *arg) {
	int iid;
	const char **filterpipe = (const char **) arg;
	static char *filter_param[VIDEO_SOURCE_CHANNEL_MAX][2];
#define	MAXPARAMLEN	64
	static char params[VIDEO_SOURCE_CHANNEL_MAX][2][MAXPARAMLEN];
	//
	if(filter_started != 0)
		return 0;
	filter_started = 1;
	for(iid = 0; iid < video_source_channels(); iid++) {
		snprintf(params[iid][0], MAXPARAMLEN, filterpipe[0], iid);
		snprintf(params[iid][1], MAXPARAMLEN, filterpipe[1], iid);
		filter_param[iid][0] = params[iid][0];
		filter_param[iid][1] = params[iid][1];
		pthread_cancel_init();
		if(pthread_create(&filter_tid[iid], NULL, filter_RGB2YUV_threadproc, filter_param[iid]) != 0) {
			filter_started = 0;
			ga_error("filter RGB2YUV: create thread failed.\n");
			return -1;
		}
		pthread_detach(filter_tid[iid]);
	}
	return 0;
}

/* filter_RGB2YUV_stop: no arguments are required */

static int
filter_RGB2YUV_stop(void *arg) {
	int iid;
	if(filter_started == 0)
		return 0;
	filter_started = 0;
	for(iid = 0; iid < video_source_channels(); iid++) {
		pthread_cancel(filter_tid[iid]);
	}
	return 0;
}

ga_module_t *
module_load() {
	static ga_module_t m;
	bzero(&m, sizeof(m));
	m.type = GA_MODULE_TYPE_FILTER;
	m.name = strdup("filter-RGB2YUV");
	m.init = filter_RGB2YUV_init;
	m.start = filter_RGB2YUV_start;
	m.stop = filter_RGB2YUV_stop;
	m.deinit = filter_RGB2YUV_deinit;
	//m.threadproc = filter_RGB2YUV_threadproc;
	return &m;
}

