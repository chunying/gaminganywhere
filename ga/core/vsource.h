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

/**
 * @file
 * Define video sources: header files
 */

#ifndef __ISOURCE_H__
#define __ISOURCE_H__

#include <pthread.h>

#include "ga-common.h"
#include "ga-avcodec.h"
#include "dpipe.h"

/** Define the default width of the max resolution
 * (can be tuned by configuration).
 * Note: Apple MBP 13" retina is 2560x1600 */
#define	VIDEO_SOURCE_DEF_MAXWIDTH	2560
/** Define the default height of the max resolution
 * (can be tuned by configuration).
 * Note: Apple MBP 13" retina is 2560x1600 */
#define	VIDEO_SOURCE_DEF_MAXHEIGHT	1600
/** Define the maximum number of video planes */
#define	VIDEO_SOURCE_MAX_STRIDE		4
/** Define the maximum number of video sources. This value must be at least 1 */
#define	VIDEO_SOURCE_CHANNEL_MAX	2
/** Define the default video source pipe name format */
#define	VIDEO_SOURCE_PIPEFORMAT		"video-%d"
/** Define the default video source pipe pool size (frames in the pipe) */
#define	VIDEO_SOURCE_POOLSIZE		8

/**
 * Data structure to store a video frame in RGBA or YUV420 format.
 */
typedef struct vsource_frame_s {
	int channel;		/**< The channel id for the video frame */
	long long imgpts;	/**< Captured time stamp
				 * (or presentatin timestamp).
				 * This is actually a sequence number of
				 * captured video frame.  */
	AVPixelFormat pixelformat;/**< pixel format, currently support
				 * RGBA, BGRA, or YUV420P
				 * Note: current use values defined in ffmpeg */
	int linesize[VIDEO_SOURCE_MAX_STRIDE];	/**< strides
				 * for each video plane (YUV420P only). */
	int realwidth;		/**< Actual width of the video frame */
	int realheight;		/**< Actual height of the video frame */
	int realstride;		/**< stride for RGBA and BGRA video frame */
	int realsize;		/**< Total size of the video frame data */
	struct timeval timestamp;	/**< Captured timestamp */
	// internal data - should not change after initialized
	int maxstride;		/**< */
	int imgbufsize;		/**< Allocated video frame buffer size */
	unsigned char *imgbuf;	/**< Pointer to the video frame buffer */
	unsigned char *imgbuf_internal;	/**< Internal pointer
				 * for buffer allocation.
				 * This is used to ensure that \a imgbuf
				 * is started at an aligned address
				 * XXX: NOT USED NOW. */
	int alignment;		/**< \a imgbuf alignment value.
				 * \a imgbuf = \a imgbuf_internal + \a alignment.
				 * XXX: NOT USED NOW. */
}	vsource_frame_t;

/**
 * Data structure to setup a video configuration.
 */
typedef struct vsource_config_s {
	int curr_width;		/**< Current video width */
	int curr_height;	/**< Current video height */
	int curr_stride;	/**< Current video stride:
				 * should be the value of height * 4,
				 * because the captured video should be
				 * in RGBA or BGRA format */
}	vsource_config_t;

/**
 * Data structure to store a video source pipe name.
 */
typedef struct pipename_s {
	struct pipename_s *next;/**< Pointer to next pipename */
	char name[1];		/**< Name of the pape.
				 * Length is variable, so must be the last field. */
}	pipename_t;

/**
 * Data structure to define a video source.
 */
typedef struct vsource_s {
	int channel;		/**< Channel Id of the video source */
	pipename_t *pipename;	/**< Pipeline name of the video source, e.g, video-%d */
	// max
	int max_width;		/**< Maximum video frame width */
	int max_height;		/**< Maximum video frame height */
	int max_stride;		/**< Maximum video frame stride: should be at least max_height * 4 */
	// curr
	int curr_width;		/**< Current video frame width */
	int curr_height;	/**< Current video frame height */
	int curr_stride;	/**< Current video frame stride: should be at least curr_stride * 4 */
	// output
	int out_width;		/**< Video output width */
	int out_height;		/**< Video output height */
	int out_stride;		/**< Video output stride: should be at least out_height * 4 */
	//
}	vsource_t;

EXPORT vsource_frame_t * vsource_frame_init(int channel, vsource_frame_t *frame);
EXPORT void vsource_frame_release(vsource_frame_t *frame);
EXPORT void vsource_dup_frame(vsource_frame_t *src, vsource_frame_t *dst);
EXPORT int vsource_embed_colorcode_init(int RGBmode);
EXPORT void vsource_embed_colorcode_reset();
EXPORT void vsource_embed_colorcode_inc(vsource_frame_t *frame);
EXPORT void vsource_embed_colorcode(vsource_frame_t *frame, unsigned int value);

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
EXPORT int video_source_mem_size(int channel);

EXPORT int video_source_setup_ex(vsource_config_t *config, int nConfig);
EXPORT int video_source_setup(int curr_width, int curr_height, int curr_stride);

#endif
