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

#define	MAX_STRIDE			4
#define	IMAGE_SOURCE_CHANNEL_MAX	4
//#define	ISOURCE_PIPEFORMAT		"image-source-%d"

struct vsource_frame {
	long long imgpts;		// presentation timestamp
	//enum vsource_format pixelformat; // rgba or yuv420p
	PixelFormat pixelformat;
	int linesize[MAX_STRIDE];	// strides for YUV
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
};

struct vsource_config {
	int rtp_id;	// RTP channel id
	int maxwidth;
	int maxheight;
	int maxstride;
	// do not touch - filled by video_source_setup functions
	int id;		// image source id
};

EXPORT struct vsource_frame * vsource_frame_init(struct vsource_frame *frame, int maxwidth, int maxheight, int maxstride);
EXPORT void vsource_frame_release(struct vsource_frame *frame);
EXPORT void vsource_dup_frame(struct vsource_frame *src, struct vsource_frame *dst);

EXPORT int video_source_channels();
EXPORT int video_source_maxwidth(int channel);
EXPORT int video_source_maxheight(int channel);
EXPORT int video_source_maxstride(int channel);
EXPORT const char *video_source_pipename(int channel);

EXPORT int video_source_setup_ex(const char *pipeformat, struct vsource_config *config, int nConfig);
EXPORT int video_source_setup(const char *pipeformat, int channel_id, int maxwidth, int maxheight, int maxstride);

#endif
