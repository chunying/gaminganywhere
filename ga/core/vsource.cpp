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
#ifndef WIN32
#include <arpa/inet.h>
#endif

#include "vsource.h"
#include "ga-common.h"
#include "ga-conf.h"
#include "ga-avcodec.h"
#include "ga-crc.h"

#define	COLORCODE_MAX_DIGIT	10
#define	COLORCODE_MAX_WIDTH	128
#define	COLORCODE_DEF_DIGIT	5
#define	COLORCODE_DEF_WIDTH	80
#define	COLORCODE_DEF_HEIGHT	80
#define	COLORCODE_CRC		2	/* current 2 octals for CRC */
#define	COLORCODE_ID		2	/* current 2 octals for ID */
#define	COLORCODE_SUFFIX	(COLORCODE_CRC + COLORCODE_ID)

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

/* {  B,   G,   R}  // XXX: here is BGR
   {  0,   0,   0}, //0 - black
   {224,   0,   0}, //1 - blue
   {  0, 224,   0}, //2 - green
   {  0,   0, 224}, //3 - red
   {  0, 224, 224}, //4 - yellow
   {224,   0, 224}, //5 - magenta
   {224, 224,   0}, //6 - cyan
   {255, 255, 255}, //7 - white */

static unsigned char rgbacolor[8][4] =  /* XXX: here is RGBA */
	{ {   0,   0,   0, 255 },
	  {   0,   0, 234, 255 },
	  {   0, 234,   0, 255 },
	  { 234,   0,   0, 255 },
	  { 234, 234,   0, 255 },
	  { 234,   0, 234, 255 },
	  {   0, 234, 234, 255 },
	  { 255, 255, 255, 255 } };

static unsigned char bgracolor[8][4] =  /* XXX: here is BGRA */
	{ {   0,   0,   0, 255 },
	  { 234,   0,   0, 255 },
	  {   0, 234,   0, 255 },
	  {   0,   0, 234, 255 },
	  {   0, 234, 234, 255 },
	  { 234,   0, 234, 255 },
	  { 234, 234,   0, 255 },
	  { 255, 255, 255, 255 } };

static unsigned char yuv_colorY[8] =	/* XXX: YUV: plane Y' */
	//{ 0x10, 0x26, 0x81, 0x4a, 0xBA, 0x5F, 0x97, 0xEB }; // - 224
	{ 0x10, 0x27, 0x86, 0x4C, 0xC2, 0x63, 0x9D, 0xEB }; // - 234
static unsigned char yuv_colorU[8] =	/* XXX: YUV: plane U */
	//{ 0x7C, 0xD9, 0x41, 0x70, 0x2A, 0xB5, 0x97, 0x85 }; // - 224
	{ 0x7C, 0xDD, 0x3E, 0x6F, 0x26, 0xB7, 0x98, 0x85 }; // - 234
static unsigned char yuv_colorV[8] =	/* XXX: YUV: plane V */
	//{ 0x7A, 0x77, 0x3B, 0xD8, 0x80, 0xCD, 0x26, 0x86 }; // - 224
	{ 0x79, 0x77, 0x38, 0xDC, 0x80, 0xD1, 0x22, 0x87 }; // - 234

static int vsource_colorcode_initialized = 0;
static unsigned int vsource_colorcode_counter = 0;
static unsigned int vsource_colorcode_counter_mask = 0;
static int vsource_colorcode_digits = COLORCODE_DEF_DIGIT;
static int vsource_colorcode_width = COLORCODE_DEF_WIDTH;
static int vsource_colorcode_height = COLORCODE_DEF_HEIGHT;
static int vsource_colorcode_total_width = 0;
static unsigned int vsource_colorcode_initmask = 0;
static unsigned int vsource_colorcode_initshift = 0;
static unsigned char * vsource_colorcode_planes[4];
static unsigned char vsource_colorcode_buffer[4 * COLORCODE_MAX_WIDTH * (COLORCODE_MAX_DIGIT + COLORCODE_SUFFIX)];

int
vsource_embed_colorcode_init(int RGBmode) {
	int i, param[3];
	// read param
	if(ga_conf_readints("embed-colorcode", param, 3) != 3)
		return -1;
	vsource_colorcode_digits = param[0];
	vsource_colorcode_width = param[1];
	vsource_colorcode_height = param[2];
	// sanity checks
	if(vsource_colorcode_digits <= 0)
		return -1;
	if(vsource_colorcode_digits > COLORCODE_MAX_DIGIT)
		vsource_colorcode_digits = COLORCODE_MAX_DIGIT;
	if(vsource_colorcode_width > COLORCODE_MAX_WIDTH)
		vsource_colorcode_width = COLORCODE_MAX_WIDTH;
	//
	vsource_colorcode_initshift = (vsource_colorcode_digits - 1) * 3;
	vsource_colorcode_initmask = (0x07 << vsource_colorcode_initshift);
	vsource_colorcode_total_width =
		(vsource_colorcode_digits + COLORCODE_SUFFIX) * vsource_colorcode_width;
	vsource_colorcode_counter_mask = 0;
	for(i = 0; i < vsource_colorcode_digits; i++) {
		vsource_colorcode_counter_mask <<= 3;
		vsource_colorcode_counter_mask |= 0x07;
	}
	//
	ga_error("video source: color code initialized - %dx%d, %d digits, shift=%d, initmask=%08x, totalwidth=%d, counter-mask=%08x\n",
		vsource_colorcode_width, vsource_colorcode_height,
		vsource_colorcode_digits,
		vsource_colorcode_initshift, vsource_colorcode_initmask,
		vsource_colorcode_total_width,
		vsource_colorcode_counter_mask);
	// assign buffers
	vsource_colorcode_planes[0] = vsource_colorcode_buffer;
	if(RGBmode != 0) {
		vsource_colorcode_planes[1] = NULL;
	} else {
		vsource_colorcode_planes[1] = vsource_colorcode_planes[0] + vsource_colorcode_total_width;
		vsource_colorcode_planes[2] = vsource_colorcode_planes[1] + (vsource_colorcode_total_width>>2);
		vsource_colorcode_planes[3] = NULL;
	}
	//
	vsource_colorcode_initialized = 1;
	return 0;
}

void
vsource_embed_colorcode_reset() {
	vsource_colorcode_counter = 0;
	return;
}

static crc5_t
vsource_crc5_usb(unsigned char *data, int length) {
	crc5_t crc;
	crc = crc5_init();
	crc = crc5_update_usb(crc, data, length);
	crc = crc5_finalize(crc);
	return crc;
}

static crc5_t
vsource_crc5_ccitt(unsigned char *data, int length) {
	crc5_t crc;
	crc = crc5_init();
	crc = crc5_update_ccitt(crc, data, length);
	crc = crc5_finalize(crc);
	return crc;
}

static void
vsource_embed_yuv_code(vsource_frame_t *frame, unsigned int value) {
	int i, j, width, height;
	/* CRC * 2 + ID * 2 */
	unsigned char suffix[COLORCODE_SUFFIX] = {0, 0, 3, 7};
	unsigned int shift = vsource_colorcode_initshift;
	unsigned int mask = vsource_colorcode_initmask;
	unsigned int digit;
	unsigned char *srcY = vsource_colorcode_buffer;
	unsigned char *srcU = srcY + vsource_colorcode_total_width;
	unsigned char *srcV = srcU + (vsource_colorcode_total_width>>1);
	unsigned char *dstY, *dstU, *dstV;
	//// make the color code line
	// compute crc
#if 0
	unsigned int crcin = htonl(value);
	suffix[0] = 0x07 & vsource_crc5_ccitt((unsigned char*) &crcin, sizeof(crcin));
	suffix[1] = 0x07 & vsource_crc5_usb((unsigned char*) &crcin, sizeof(crcin));
#else
	if(value != 0) {
		suffix[0] = (43 * (((value * 32)/43) + 1) - 32 * value) & 0x07;
		suffix[1] = (37 * (((value * 32)/37) + 1) - 32 * value) & 0x07;
	}
#endif
	// value part
	while(mask != 0) {
		digit = ((value & mask) >> shift);
		for(i = 0; i < vsource_colorcode_width; i++) {
			*srcY++ = yuv_colorY[digit];
		}
		width = (vsource_colorcode_width>>1);
		for(i = 0; i < width; i++) {
			*srcU++ = yuv_colorU[digit];
			*srcV++ = yuv_colorV[digit];
		}
		mask >>= 3;
		shift -= 3;
	}
	// suffix part: crc + id
	for(j = 0; j < sizeof(suffix); j++) {
		for(i = 0; i < vsource_colorcode_width; i++) {
			*srcY++ = yuv_colorY[suffix[j]];
		}
		width = (vsource_colorcode_width>>1);
		for(i = 0; i < width; i++) {
			*srcU++ = yuv_colorU[suffix[j]];
			*srcV++ = yuv_colorV[suffix[j]];
		}
	}
	// reset srcY, srcU, and srcV
	srcY = vsource_colorcode_buffer;
	srcU = srcY + vsource_colorcode_total_width;
	srcV = srcU + (vsource_colorcode_total_width>>1);
	//// fill color code line
	height = frame->realheight < vsource_colorcode_height ?
			frame->realheight : vsource_colorcode_height;
	dstY = frame->imgbuf;
	dstU = dstY + frame->linesize[0] * (frame->realheight);
	dstV = dstU + frame->linesize[1] * (frame->realheight >> 1);
	//
	for(i = 0; i < height; i++) {
		bcopy(srcY, dstY, vsource_colorcode_total_width);
		dstY += frame->linesize[0];
	}
	height >>= 1;
	for(i = 0; i < height; i++) {
		bcopy(srcU, dstU, vsource_colorcode_total_width>>1);
		bcopy(srcV, dstV, vsource_colorcode_total_width>>1);
		dstU += frame->linesize[1];
		dstV += frame->linesize[2];
	}
	//
	return;
}

static void
vsource_embed_rgba_code(vsource_frame_t *frame, unsigned int value, unsigned char color[8][4]) {
	int i, j, height;
	/* CRC * 2 + ID * 2 */
	unsigned char suffix[COLORCODE_SUFFIX] = {0, 0, 3, 7};
	unsigned int shift = vsource_colorcode_initshift;
	unsigned int mask = vsource_colorcode_initmask;
	unsigned int digit;
	unsigned char *dst = vsource_colorcode_buffer;
	//// make the color code line
	// compute crc
#if 0
	unsigned int crcin = htonl(value);
	suffix[0] = 0x07 & vsource_crc5_ccitt((unsigned char*) &crcin, sizeof(crcin));
	suffix[1] = 0x07 & vsource_crc5_usb((unsigned char*) &crcin, sizeof(crcin));
#else
	if(value != 0) {
		suffix[0] = (43 * (((value * 32)/43) + 1) - 32 * value) & 0x07;
		suffix[1] = (37 * (((value * 32)/37) + 1) - 32 * value) & 0x07;
	}
#endif
	// value part
	while(mask != 0) {
		digit = ((value & mask) >> shift);
		for(i = 0; i < vsource_colorcode_width; i++) {
			*dst++ = color[digit][0];
			*dst++ = color[digit][1];
			*dst++ = color[digit][2];
			*dst++ = color[digit][3];
		}
		mask >>= 3;
		shift -= 3;
	}
	// suffix part: crc + id
	for(j = 0; j < sizeof(suffix); j++) {
		for(i = 0; i < vsource_colorcode_width; i++) {
			*dst++ = color[suffix[j]][0];
			*dst++ = color[suffix[j]][1];
			*dst++ = color[suffix[j]][2];
			*dst++ = color[suffix[j]][3];
		}
	}
	//// fill color code line
	height = frame->realheight < vsource_colorcode_height ?
			frame->realheight : vsource_colorcode_height;
	dst = frame->imgbuf;
	//
	for(i = 0; i < height; i++) {
		bcopy(vsource_colorcode_buffer, dst, vsource_colorcode_total_width * 4);
		dst += frame->linesize[0];
	}
	//
	return;
}

void
vsource_embed_colorcode_inc(vsource_frame_t *frame) {
	vsource_embed_colorcode(frame, vsource_colorcode_counter);
	vsource_colorcode_counter++;
	vsource_colorcode_counter &= vsource_colorcode_counter_mask;
	return;
}

void
vsource_embed_colorcode(vsource_frame_t *frame, unsigned int value) {
	if(vsource_colorcode_initialized == 0)
		return;
	if(frame == NULL)
		return;
	if(frame->realwidth < vsource_colorcode_total_width)
		return;
	if(frame->pixelformat == PIX_FMT_YUV420P) {
		vsource_embed_yuv_code(frame, value);
	} else if(frame->pixelformat == PIX_FMT_RGBA) {
		vsource_embed_rgba_code(frame, value, rgbacolor);
	} else if(frame->pixelformat == PIX_FMT_BGRA) {
		vsource_embed_rgba_code(frame, value, bgracolor);
	}
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

