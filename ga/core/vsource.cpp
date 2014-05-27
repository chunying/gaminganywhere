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
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <map>

#include "vsource.h"
#include "ga-common.h"
#include "ga-conf.h"

// golbal image structure
static int gChannels;
static vsource_t gVsource[VIDEO_SOURCE_CHANNEL_MAX];
static pipeline *gPipe[VIDEO_SOURCE_CHANNEL_MAX];

vsource_frame_t *
vsource_frame_init(int channel, vsource_frame_t *frame) {
	int i;
	vsource_t *vs;
	//
	if(channel < 0 || channel >= VIDEO_SOURCE_CHANNEL_MAX)
		return NULL;
	vs = &gVsource[channel];
	// has not been initialized?
	if(vs->max_width == 0)
		return NULL;
	//
	bzero(frame, sizeof(vsource_frame_t));
	//
	for(i = 0; i < VIDEO_SOURCE_MAX_STRIDE; i++) {
		frame->linesize[i] = vs->max_stride;
	}
	frame->maxstride = vs->max_stride;
	frame->imgbufsize = vs->max_height * vs->max_stride;
	if(ga_malloc(frame->imgbufsize, (void**) &frame->imgbuf_internal, &frame->alignment) < 0) {
		return NULL;
	}
	frame->imgbuf = frame->imgbuf_internal + frame->alignment;
	bzero(frame->imgbuf, frame->imgbufsize);
	return frame;
}

void
vsource_frame_release(vsource_frame_t *frame) {
	if(frame == NULL)
		return;
	if(frame->imgbuf != NULL)
		free(frame->imgbuf);
	return;
}

void
vsource_dup_frame(vsource_frame_t *src, vsource_frame_t *dst) {
	int j;
	dst->imgpts = src->imgpts;
	dst->pixelformat = src->pixelformat;
	for(j = 0; j < VIDEO_SOURCE_MAX_STRIDE; j++) {
		dst->linesize[j] = src->linesize[j];
	}
	dst->realwidth = src->realwidth;
	dst->realheight = src->realheight;
	dst->realstride = src->realstride;
	dst->realsize = src->realsize;
	bcopy(src->imgbuf, dst->imgbuf, src->realstride * src->realheight/*dst->imgbufsize*/);
	return;
}

int
video_source_channels() {
	return gChannels;
}

vsource_t *
video_source(int channel) {
	if(channel < 0 || channel > gChannels) {
		return NULL;
	}
	return &gVsource[channel];
}

static const char *
video_source_add_pipename_internal(vsource_t *vs, const char *pipename) {
	pipename_t *p;
	if(vs == NULL || pipename == NULL)
		return NULL;
	if((p = (pipename_t *) malloc(sizeof(pipename_t) + strlen(pipename) + 1)) == NULL)
		return NULL;
	p->next = vs->pipename;
	bcopy(pipename, p->name, strlen(pipename)+1);
	vs->pipename = p;
	return p->name;
}

const char *
video_source_add_pipename(int channel, const char *pipename) {
	vsource_t *vs = video_source(channel);
	if(vs == NULL)
		return NULL;
	return video_source_add_pipename_internal(vs, pipename);
}

const char *
video_source_get_pipename(int channel) {
	vsource_t *vs = video_source(channel);
	if(vs == NULL)
		return NULL;
	if(vs->pipename == NULL)
		return NULL;
	return vs->pipename->name;
}

int
video_source_max_width(int channel) {
	vsource_t *vs = video_source(channel);
	return vs == NULL ? -1 : vs->max_width;
}

int
video_source_max_height(int channel) {
	vsource_t *vs = video_source(channel);
	return vs == NULL ? -1 : vs->max_height;
}

int
video_source_max_stride(int channel) {
	vsource_t *vs = video_source(channel);
	return vs == NULL ? -1 : vs->max_stride;
}

int
video_source_curr_width(int channel) {
	vsource_t *vs = video_source(channel);
	return vs == NULL ? -1 : vs->curr_width;
}

int
video_source_curr_height(int channel) {
	vsource_t *vs = video_source(channel);
	return vs == NULL ? -1 : vs->curr_height;
}

int
video_source_curr_stride(int channel) {
	vsource_t *vs = video_source(channel);
	return vs == NULL ? -1 : vs->curr_stride;
}

int
video_source_out_width(int channel) {
	vsource_t *vs = video_source(channel);
	return vs == NULL ? -1 : vs->out_width;
}

int
video_source_out_height(int channel) {
	vsource_t *vs = video_source(channel);
	return vs == NULL ? -1 : vs->out_height;
}

int
video_source_out_stride(int channel) {
	vsource_t *vs = video_source(channel);
	return vs == NULL ? -1 : vs->out_stride;
}

#define	max(x, y)	((x) > (y) ? (x) : (y))

int
video_source_setup_ex(vsource_config_t *config, int nConfig) {
	int idx;
	int maxres[2] = { 0, 0 };
	int outres[2] = { 0, 0 };
	//
	if(config==NULL || nConfig <=0 || nConfig > VIDEO_SOURCE_CHANNEL_MAX) {
		ga_error("video source: invalid video source configuration request=%d; MAX=%d; config=%p\n",
			nConfig, VIDEO_SOURCE_CHANNEL_MAX, config);
		return -1;
	}
	//
	if(ga_conf_readints("max-resolution", maxres, 2) != 2) {
		maxres[0] = maxres[1] = 0;
	}
	if(ga_conf_readints("output-resolution", outres, 2) != 2) {
		outres[0] = outres[1] = 0;
	}
	//
	for(idx = 0; idx < nConfig; idx++) {
		vsource_t *vs = &gVsource[idx];
		pooldata_t *data = NULL;
		char pipename[64];
		//
		bzero(vs, sizeof(vsource_t));
		snprintf(pipename, sizeof(pipename), VIDEO_SOURCE_PIPEFORMAT, idx);
		vs->channel     = idx;
		if(video_source_add_pipename_internal(vs, pipename) == NULL) {
			ga_error("video source: setup pipename failed (%s).\n", pipename);
			return -1;
		}
		vs->max_width   = max(VIDEO_SOURCE_DEF_MAXWIDTH, maxres[0]);
		vs->max_height  = max(VIDEO_SOURCE_DEF_MAXHEIGHT, maxres[1]);
		vs->max_stride  = max(VIDEO_SOURCE_DEF_MAXWIDTH, maxres[0]) * 4;
		vs->curr_width  = config[idx].curr_width;
		vs->curr_height = config[idx].curr_height;
		vs->curr_stride = config[idx].curr_stride;
		if(outres[0] != 0) {
			vs->out_width   = outres[0];
			vs->out_height  = outres[1];
			vs->out_stride  = outres[0] * 4;
		} else {
			vs->out_width   = vs->curr_width;
			vs->out_height  = vs->curr_height;
			vs->out_stride  = vs->curr_stride;
		}
		// create pipe
		if((gPipe[idx] = new pipeline()) == NULL) {
			ga_error("video source: init pipeline failed.\n");
			return -1;
		}
#if 1		// no need for privdata
		if(gPipe[idx]->alloc_privdata(sizeof(vsource_t)) == NULL) {
			ga_error("video source: cannot allocate private data.\n");
			delete gPipe[idx];
			gPipe[idx] = NULL;
			return -1;
		}
#if 0
		config[idx].id = idx;
		gPipe[idx]->set_privdata(&config[idx], sizeof(struct vsource_config));
#else
		gPipe[idx]->set_privdata(vs, sizeof(vsource_t));
#endif
#endif
		// create data pool for the pipe
		if((data = gPipe[idx]->datapool_init(VIDEO_SOURCE_POOLSIZE, sizeof(vsource_frame_t))) == NULL) {
			ga_error("video source: cannot allocate data pool.\n");
			delete gPipe[idx];
			gPipe[idx] = NULL;
			return -1;
		}
		// per frame init
		for(; data != NULL; data = data->next) {
			if(vsource_frame_init(idx, (vsource_frame_t*) data->ptr) == NULL) {
				ga_error("video source: init frame failed.\n");
				return -1;
			}
		}
		//
		if(pipeline::do_register(pipename, gPipe[idx]) < 0) {
			ga_error("video source: register pipeline failed (%s)\n",
					pipename);
			return -1;
		}
		//
		ga_error("video-source: %s initialized max-curr-out = (%dx%d)-(%dx%d)-(%dx%d)\n",
			pipename, vs->max_width, vs->max_height,
			vs->curr_width, vs->curr_height, vs->out_width, vs->out_height);
	}
	//
	gChannels = idx;
	//
	return 0;
}

int
video_source_setup(int curr_width, int curr_height, int curr_stride) {
	vsource_config_t c;
	bzero(&c, sizeof(c));
	//config.rtp_id = channel_id;
	c.curr_width = curr_width;
	c.curr_height = curr_height;
	c.curr_stride = curr_stride;
	//
	return video_source_setup_ex(&c, 1);
}

