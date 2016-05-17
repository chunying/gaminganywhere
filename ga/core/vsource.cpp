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

/**
 * @file
 * Define video sources: the implementation
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

/**< Video buffer allocation alignment: should be 2^n */
#define	VSOURCE_ALIGNMENT	16
/**< Video buffer allocation alignment mask: should be \em VSOURCE_ALIGNMENT-1 */
#define	VSOURCE_ALIGNMENT_MASK	0x0f

// embed colorcode feature
#define	COLORCODE_MAX_DIGIT	10	/**< Maximum number of embedded color code digits */
#define	COLORCODE_MAX_WIDTH	128	/**< Maximum Width of each embedded color code digit */
#define	COLORCODE_DEF_DIGIT	5	/**< Number of embedded color code sequence digts */
#define	COLORCODE_DEF_WIDTH	80	/**< Width of each embedded color code digit */
#define	COLORCODE_DEF_HEIGHT	80	/**< Height of embedded color code */
#define	COLORCODE_CRC		2	/**< Colorcode: current 2 octals for CRC */
#define	COLORCODE_ID		2	/**< Colorcode: current 2 octals for ID */
#define	COLORCODE_SUFFIX	(COLORCODE_CRC + COLORCODE_ID)	/**< Digits
					  * appended to the embedded color code sequence */

// golbal image structure
static int gChannels;		/**< Total number of video channels */
static vsource_t gVsource[VIDEO_SOURCE_CHANNEL_MAX];	/**< Video source */
static dpipe_t *gPipe[VIDEO_SOURCE_CHANNEL_MAX];	/**< Video pipeline */

/**
 * Initialize a video frame
 *
 * @param channel [in] The channel Id of the video frame.
 * @param frame [in] Pointer to an allocated video frame structure.
 * @return Return the same pointer as \a frame, unless an incorrect configuration is given.
 *
 * Note that video frame data is stored right after a video frame structure.
 * So the size of allocated video frame structure must be at least:
 * \em sizeof(vsource_frame_t)
 * + \em video-source-max-stride * \em video-source-max-height
 * + \em VSOURCE_ALIGNMENT.
 *
 * \a imgbufsize will be set to \em video-source-max-stride * \em video-source-max-height,
 * and \a imgbuf is pointed to an aligned memory address.
 */
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
	frame->imgbuf = ((unsigned char *) frame) + sizeof(vsource_frame_t);
	frame->imgbuf += ga_alignment(frame->imgbuf, VSOURCE_ALIGNMENT);
	//ga_error("XXX: frame=%p, imgbuf=%p, sizeof(vframe)=%d, bzero(%d)\n",
	//	frame, frame->imgbuf, sizeof(vsource_frame_t), frame->imgbufsize);
	bzero(frame->imgbuf, frame->imgbufsize);
	return frame;
}

/**
 * Release a video frame data structure.
 *
 * Currently it does nothing because memory allocation is done in pipeline.
 */
void
vsource_frame_release(vsource_frame_t *frame) {
	return;
}

/**
 * Duplicate a video frame.
 *
 * @param src [in] Pointer to a source video frame.
 * @param dst [in] Pointer to an initialized destination video frame.
 */
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

/**
 * Color code colors based on RGBA color.
 * The order is: blak blue green, red, yellow, magenta, cyan, and white */
static unsigned char rgbacolor[8][4] =
	{ {   0,   0,   0, 255 },
	  {   0,   0, 234, 255 },
	  {   0, 234,   0, 255 },
	  { 234,   0,   0, 255 },
	  { 234, 234,   0, 255 },
	  { 234,   0, 234, 255 },
	  {   0, 234, 234, 255 },
	  { 255, 255, 255, 255 } };

/**
 * Color code colors based on BGRA color.
 * The order is: blak blue green, red, yellow, magenta, cyan, and white */
static unsigned char bgracolor[8][4] =
	{ {   0,   0,   0, 255 },
	  { 234,   0,   0, 255 },
	  {   0, 234,   0, 255 },
	  {   0,   0, 234, 255 },
	  {   0, 234, 234, 255 },
	  { 234,   0, 234, 255 },
	  { 234, 234,   0, 255 },
	  { 255, 255, 255, 255 } };

/**
 * Color code colors based on YUV420P color.
 * The order is: blak blue green, red, yellow, magenta, cyan, and white */
static unsigned char yuv_colorY[8] =	/**< For YUV plane Y' */
	//{ 0x10, 0x26, 0x81, 0x4a, 0xBA, 0x5F, 0x97, 0xEB }; // - 224
	{ 0x10, 0x27, 0x86, 0x4C, 0xC2, 0x63, 0x9D, 0xEB }; // - 234
static unsigned char yuv_colorU[8] =	/**< For YUV plane U */
	//{ 0x7C, 0xD9, 0x41, 0x70, 0x2A, 0xB5, 0x97, 0x85 }; // - 224
	{ 0x7C, 0xDD, 0x3E, 0x6F, 0x26, 0xB7, 0x98, 0x85 }; // - 234
static unsigned char yuv_colorV[8] =	/**< For YUV plane V */
	//{ 0x7A, 0x77, 0x3B, 0xD8, 0x80, 0xCD, 0x26, 0x86 }; // - 224
	{ 0x79, 0x77, 0x38, 0xDC, 0x80, 0xD1, 0x22, 0x87 }; // - 234

// global configuratoin for embedding color code feature (CC)
static int vsource_colorcode_initialized = 0;			/**< CC has been initialized */
static unsigned int vsource_colorcode_counter = 0;		/**< CC's current sequence number */
static unsigned int vsource_colorcode_counter_mask = 0;		/**< CC's mask used to rotate sequence number */
static int vsource_colorcode_digits = COLORCODE_DEF_DIGIT;	/**< CC's number of digits */
static int vsource_colorcode_width = COLORCODE_DEF_WIDTH;	/**< CC's per-digit width */
static int vsource_colorcode_height = COLORCODE_DEF_HEIGHT;	/**< CC's digit height */
static int vsource_colorcode_total_width = 0;			/**< CC's total digit width */
static unsigned int vsource_colorcode_initmask = 0;		/**< CC's mask to obtain 3-bit digits (starting from the left-most digit) */
static unsigned int vsource_colorcode_initshift = 0;		/**< CC's shift value to compute init-mask */
static unsigned char * vsource_colorcode_planes[4];		/**< CC's color code planes */
static unsigned char vsource_colorcode_buffer[4 * COLORCODE_MAX_WIDTH * (COLORCODE_MAX_DIGIT + COLORCODE_SUFFIX)];	/**<
								 * rendered color code buffer */
//
static FILE *savefp_ccodets = NULL;				/**< FILE pointer used to store color code sequence and timestamp */

/**
 * Initialize the color code feature.
 *
 * @param RGBmode [in] Specify to generate RGB or YUV color codes.
 *	Values can be zero (YUV) or non-zero (RGB).
 * @return 0 on success, or -1 on error.
 */
int
vsource_embed_colorcode_init(int RGBmode) {
	int i, param[3];
	char savefile_ccodets[128];
	// read param
	if(ga_conf_readints("embed-colorcode", param, 3) != 3)
		return -1;
	if(ga_conf_readv("save-colorcode-timestamp", savefile_ccodets, sizeof(savefile_ccodets)) != NULL)
		savefp_ccodets = ga_save_init_txt(savefile_ccodets);
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

/**
 * Reset the color code sequence number (counter).
 */
void
vsource_embed_colorcode_reset() {
	vsource_colorcode_counter = 0;
	return;
}

/**
 * Get CRC5-USB checksum value
 *
 * @param data [in] The data used to compute the checksum.
 * @param length [in] The lenght of the data
 * @return the CRC5-USB value
 */
static crc5_t
vsource_crc5_usb(unsigned char *data, int length) {
	crc5_t crc;
	crc = crc5_init();
	crc = crc5_update_usb(crc, data, length);
	crc = crc5_finalize(crc);
	return crc;
}

/**
 * Get CRC5-CCITT checksum value
 *
 * @param data [in] The data used to compute the checksum.
 * @param length [in] The lenght of the data
 * @return the CRC5-CCITT value
 */
static crc5_t
vsource_crc5_ccitt(unsigned char *data, int length) {
	crc5_t crc;
	crc = crc5_init();
	crc = crc5_update_ccitt(crc, data, length);
	crc = crc5_finalize(crc);
	return crc;
}

/**
 * Embed color codes in a YUV image. This is an internal function.
 *
 * @param frame [in] Pointer to the video frame.
 * @param value [in] The value to be embedded.
 *	The value contains only the sequence number part.
 */
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
	// save code-timestamp mapping
	struct timeval ccodets;
	if(savefp_ccodets != NULL) {
		gettimeofday(&ccodets, NULL);
		ga_save_printf(savefp_ccodets, "COLORCODE-TIMESTAMP: %08u -> %u.%06u\n",
			value, ccodets.tv_sec, ccodets.tv_usec);
	}
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

/**
 * Embed color codes in a RGBA image. This is an internal function.
 *
 * @param frame [in] Pointer to the video frame.
 * @param value [in] The value to be embedded.
 *	The value contains only the sequence number part.
 */
static void
vsource_embed_rgba_code(vsource_frame_t *frame, unsigned int value, unsigned char color[8][4]) {
	int i, j, height;
	/* CRC * 2 + ID * 2 */
	unsigned char suffix[COLORCODE_SUFFIX] = {0, 0, 3, 7};
	unsigned int shift = vsource_colorcode_initshift;
	unsigned int mask = vsource_colorcode_initmask;
	unsigned int digit;
	unsigned char *dst = vsource_colorcode_buffer;
	// save code-timestamp mapping
	struct timeval ccodets;
	if(savefp_ccodets != NULL) {
		gettimeofday(&ccodets, NULL);
		ga_save_printf(savefp_ccodets, "COLORCODE-TIMESTAMP: %u -> %u.%06u\n",
			value, ccodets.tv_sec, ccodets.tv_usec);
	}
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

/**
 * Incrementally embed color a code into an image.
 *
 * @param frame [in] Pointer to the video frame.
 *
 * You should call this function to embed a color code.
 * It automatically detetcs the image type,
 * embed a sequence number, increase the sequence number by 1, and
 * rotate the sequence number when it reaches the bounday.
 */
void
vsource_embed_colorcode_inc(vsource_frame_t *frame) {
	vsource_embed_colorcode(frame, vsource_colorcode_counter);
	vsource_colorcode_counter++;
	vsource_colorcode_counter &= vsource_colorcode_counter_mask;
	return;
}

/**
 * Embed an arbitrary color code into an image
 *
 * @param frame [in] Pointer to the video frame.
 * @param value [in] The sequence number to be embedded.
 */
void
vsource_embed_colorcode(vsource_frame_t *frame, unsigned int value) {
	if(vsource_colorcode_initialized == 0)
		return;
	if(frame == NULL)
		return;
	if(frame->realwidth < vsource_colorcode_total_width)
		return;
	if(frame->pixelformat == AV_PIX_FMT_YUV420P) {
		vsource_embed_yuv_code(frame, value);
	} else if(frame->pixelformat == AV_PIX_FMT_RGBA) {
		vsource_embed_rgba_code(frame, value, rgbacolor);
	} else if(frame->pixelformat == AV_PIX_FMT_BGRA) {
		vsource_embed_rgba_code(frame, value, bgracolor);
	}
	return;
}

/**
 * Get the number of channels of the video source.
 *
 * @return The total number of channels.
 */
int
video_source_channels() {
	return gChannels;
}

/**
 * Get the video source setup of a given channel.
 *
 * @param channel [in] The channel Id.
 * @return Pointer to the obtained video source info data structure.
 */
vsource_t *
video_source(int channel) {
	if(channel < 0 || channel > gChannels) {
		return NULL;
	}
	return &gVsource[channel];
}

/**
 * Add a pipename to a video source. This is an internal function.
 *
 * @param vs [in] The video source data structure.
 * @param pipename [in] The pipeline name to be added.
 * @return Pointer to the added pipeline name.
 */
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

/**
 * Add a pipeline name to a video source data structure.
 *
 * @param channel [in] The video source channel id.
 * @param pipename [in] The pipeline name to be added.
 * @return Pointer to the added pipeline name.
 */
const char *
video_source_add_pipename(int channel, const char *pipename) {
	vsource_t *vs = video_source(channel);
	if(vs == NULL)
		return NULL;
	return video_source_add_pipename_internal(vs, pipename);
}

/**
 * Get the pipeline name of a video source.
 *
 * @param channel [in] The channel id of the video source.
 * @return Pointer to the pipeline name.
 */
const char *
video_source_get_pipename(int channel) {
	vsource_t *vs = video_source(channel);
	if(vs == NULL)
		return NULL;
	if(vs->pipename == NULL)
		return NULL;
	return vs->pipename->name;
}

/**
 * Get the maximum width of a video source.
 *
 * @param channel [in] The channel id of the video source.
 * @return The maximum width of the video source, or -1 on error.
 */
int
video_source_max_width(int channel) {
	vsource_t *vs = video_source(channel);
	return vs == NULL ? -1 : vs->max_width;
}

/**
 * Get the maximum height of a video source.
 *
 * @param channel [in] The channel id of the video source.
 * @return The maximum height of the video source, or -1 on error.
 */
int
video_source_max_height(int channel) {
	vsource_t *vs = video_source(channel);
	return vs == NULL ? -1 : vs->max_height;
}

/**
 * Get the maximum stride of a video source.
 *
 * @param channel [in] The channel id of the video source.
 * @return The maximum stride of the video source, or -1 on error.
 */
int
video_source_max_stride(int channel) {
	vsource_t *vs = video_source(channel);
	return vs == NULL ? -1 : vs->max_stride;
}

/**
 * Get the current width of a video source.
 *
 * @param channel [in] The channel id of the video source.
 * @return The current width of the video source, or -1 on error.
 */
int
video_source_curr_width(int channel) {
	vsource_t *vs = video_source(channel);
	return vs == NULL ? -1 : vs->curr_width;
}

/**
 * Get the current height of a video source.
 *
 * @param channel [in] The channel id of the video source.
 * @return The current height of the video source, or -1 on error.
 */
int
video_source_curr_height(int channel) {
	vsource_t *vs = video_source(channel);
	return vs == NULL ? -1 : vs->curr_height;
}

/**
 * Get the current stride of a video source.
 *
 * @param channel [in] The channel id of the video source.
 * @return The current stride of the video source, or -1 on error.
 */
int
video_source_curr_stride(int channel) {
	vsource_t *vs = video_source(channel);
	return vs == NULL ? -1 : vs->curr_stride;
}

/**
 * Get the output width of a video source.
 *
 * @param channel [in] The channel id of the video source.
 * @return The output width of the video source, or -1 on error.
 */
int
video_source_out_width(int channel) {
	vsource_t *vs = video_source(channel);
	return vs == NULL ? -1 : vs->out_width;
}

/**
 * Get the output height of a video source.
 *
 * @param channel [in] The channel id of the video source.
 * @return The output height of the video source, or -1 on error.
 */
int
video_source_out_height(int channel) {
	vsource_t *vs = video_source(channel);
	return vs == NULL ? -1 : vs->out_height;
}

/**
 * Get the output stride of a video source.
 *
 * @param channel [in] The channel id of the video source.
 * @return The output stride of the video source, or -1 on error.
 */
int
video_source_out_stride(int channel) {
	vsource_t *vs = video_source(channel);
	return vs == NULL ? -1 : vs->out_stride;
}

/**
  * Return the maximum memory size to store a frame (including size for alignment)
  *
  * @param channel [in] The channel id
  * @return The size in bytes, or 0 if the given \a channel is not initialized
  */
int
video_source_mem_size(int channel) {
	vsource_t *vs = video_source(channel);
	return vs == NULL ? 0 : (vs->max_height * vs->max_stride + VSOURCE_ALIGNMENT);
}

/** Return the larger value of \a x and \a y */
#define	max(x, y)	((x) > (y) ? (x) : (y))

/**
 * The generic function to setup video sources.
 *
 * @param config [in] Pointer to an array of video configurations.
 * @param nConfig [in] Number of configurations in the array.
 * @return 0 on success, or -1 on error.
 *
 * - The configurations have to provide current video width and height
 *   of each video channel.
 * - The maximum resolution is read from \em max-resolution parameter, and
 *   the output resolution is read from \em output-resolution parameter in
 *   the configuratoin file.
 * - The pipeline name is automatically generated based on the index of
 *   each video configuration.
 * - The corresponding video pipeline is created as well.
 */
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
		dpipe_buffer_t *data = NULL;
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
		gPipe[idx] = dpipe_create(idx, pipename, VIDEO_SOURCE_POOLSIZE,
				sizeof(vsource_frame_t) + vs->max_height * vs->max_stride + VSOURCE_ALIGNMENT);
		if(gPipe[idx] == NULL) {
			ga_error("video source: init pipeline failed.\n");
			return -1;
		}
		for(data = gPipe[idx]->in; data != NULL; data = data->next) {
			if(vsource_frame_init(idx, (vsource_frame_t*) data->pointer) == NULL) {
				ga_error("video source: init faile failed.\n");
				return -1;
			}
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

/**
 * Setup up a one-channel only video source
 *
 * @param curr_width [in] Current video source width.
 * @param curr_height [in] Current video source height.
 * @param curr_stride [in] Current video source stride.
 * @return 0 on success, or -1 on error.
 *
 * This function calls the \em video_source_setup_ex function.
 */
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

