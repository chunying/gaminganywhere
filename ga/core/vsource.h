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

#ifndef __ISOURCE_H__
#define __ISOURCE_H__

#include <pthread.h>

#include "ga-common.h"
#include "ga-avcodec.h"
#include "pipeline.h"

#define	VIDEO_SOURCE_DEF_MAXWIDTH	1920
#define	VIDEO_SOURCE_DEF_MAXHEIGHT	1200
#define	VIDEO_SOURCE_MAX_STRIDE		4
#define	VIDEO_SOURCE_CHANNEL_MAX	2
#define	VIDEO_SOURCE_PIPEFORMAT		"video-%d"
#define	VIDEO_SOURCE_POOLSIZE		8

typedef struct vsource_frame_s {
	int channel;
	long long imgpts;		// presentation timestamp
	//enum vsource_format pixelformat; // rgba or yuv420p
	PixelFormat pixelformat;
	int linesize[VIDEO_SOURCE_MAX_STRIDE];	// strides for YUV
	int realwidth;
	int realheight;
	int realstride;
	int realsize;
	// internal data - should not change after initialized
	int maxstride;
	int imgbufsize;
	unsigned char *imgbuf;
	unsigned char *imgbuf_internal;
	int alignment;
}	vsource_frame_t;

typedef struct vsource_config_s {
	//int rtp_id;	// RTP channel id
	int curr_width;
	int curr_height;
	int curr_stride;
	// do not touch - filled by video_source_setup functions
	//int id;		// image source id
}	vsource_config_t;

typedef struct pipename_s {
	struct pipename_s *next;
	char name[1];	// variable length, must be last field
}	pipename_t;

typedef struct vsource_s {
	int channel;
	pipename_t *pipename;
	// max
	int max_width;
	int max_height;
	int max_stride;
	// curr
	int curr_width;
	int curr_height;
	int curr_stride;
	// output
	int out_width;
	int out_height;
	int out_stride;
	//
}	vsource_t;

EXPORT vsource_frame_t * vsource_frame_init(int channel, vsource_frame_t *frame);
EXPORT void vsource_frame_release(vsource_frame_t *frame);
EXPORT void vsource_dup_frame(vsource_frame_t *src, vsource_frame_t *dst);

EXPORT int video_source_channels();
EXPORT vsource_t * video_source(int channel);
EXPORT const char *video_source_add_pipename(int channel, const char *pipename);
EXPORT const char *video_source_get_pipename(int channel);
//
EXPORT int video_source_max_width(int channel);
EXPORT int video_source_max_height(int channel);
EXPORT int video_source_max_stride(int channel);
EXPORT int video_source_curr_width(int channel);
EXPORT int video_source_curr_height(int channel);
EXPORT int video_source_curr_stride(int channel);
EXPORT int video_source_out_width(int channel);
EXPORT int video_source_out_height(int channel);
EXPORT int video_source_out_stride(int channel);

EXPORT int video_source_setup_ex(vsource_config_t *config, int nConfig);
EXPORT int video_source_setup(int curr_width, int curr_height, int curr_stride);

#endif
