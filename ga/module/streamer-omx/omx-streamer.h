/*
 * Copyright (c) 2013-2015 Chun-Ying Huang
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

#ifndef __OMX_STREAMER_H__
#define __OMX_STREAMER_H__

#ifdef __cplusplus
extern "C" {
#endif
#include <bcm_host.h>
#include <interface/vcos/vcos_semaphore.h>
#include <interface/vmcs_host/vchost.h>
#include <IL/OMX_Core.h>
#include <IL/OMX_Component.h>
#include <IL/OMX_Video.h>
#include <IL/OMX_Broadcom.h>
#ifdef __cplusplus
}
#endif
#define OSCAM_DEF_SHARPNESS                   0		// -100 .. 100
#define OSCAM_DEF_CONTRAST                    0		// -100 .. 100
#define OSCAM_DEF_BRIGHTNESS                  50	//    0 .. 100
#define OSCAM_DEF_SATURATION                  0		// -100 .. 100
#define OSCAM_DEF_EXPOSURE_VALUE_COMPENSATION 0		//  -10 .. 10
#define OSCAM_DEF_EXPOSURE_ISO_SENSITIVITY    100	//  100 .. 800
#define OSCAM_DEF_EXPOSURE_AUTO_SENSITIVITY   OMX_FALSE
#define OSCAM_DEF_FRAME_STABILISATION         OMX_TRUE
#define OSCAM_DEF_WHITE_BALANCE_CONTROL       OMX_WhiteBalControlAuto	// OMX_WHITEBALCONTROLTYPE
#define OSCAM_DEF_IMAGE_FILTER                OMX_ImageFilterNoise	// OMX_IMAGEFILTERTYPE
#define OSCAM_DEF_FLIP_HORIZONTAL             OMX_FALSE
#define OSCAM_DEF_FLIP_VERTICAL               OMX_FALSE

typedef struct omx_streamer_config_s {
	int		camera_sharpness;	// -100 .. 100
	int		camera_contrast;	// -100 .. 100
	int		camera_brightness;	//    0 .. 100
	int		camera_saturation;	// -100 .. 100
	int		camera_ev;		//  -10 .. 10
	int		camera_iso;		//  100 .. 800
	OMX_BOOL	camera_iso_auto;
	OMX_BOOL	camera_frame_stabilisation;
	OMX_BOOL	camera_flip_horizon;
	OMX_BOOL	camera_flip_vertical;
	enum OMX_WHITEBALCONTROLTYPE	camera_whitebalance;
	enum OMX_IMAGEFILTERTYPE	camera_filter;
}	omx_streamer_config_t;

typedef struct omx_streamer_s {
	int		initialized;
	OMX_HANDLETYPE	camera;
	OMX_HANDLETYPE	encoder;
	OMX_HANDLETYPE	null_sink;
	OMX_BUFFERHEADERTYPE *inbuf;	// for camera
	OMX_BUFFERHEADERTYPE *outbuf;	// for encoder
	int		camera_ready;
	int		encoder_output_buffer_available;
	int		flushed;
	VCOS_SEMAPHORE_T handler_lock;
	// camera settings: XXX - not yet implemented
	omx_streamer_config_t config;
	////////
	int		width;
	int		height;
	int		fps_n;
	int		fps_d;
	int		bitrate;
	int		gopsize;
	// buffer for encoded data
	int		buffer_filled;
	int		bufsize;
	unsigned char 	*buffer;
	// internal statistics
	int		frame_out;	// number of output frames
	//
	unsigned char	sps[1024];
	unsigned int	spslen;
	unsigned char	pps[1024];
	unsigned int	ppslen;
	//
}	omx_streamer_t;

int omx_streamer_init(omx_streamer_t *ctx, omx_streamer_config_t *config, int width, int height, int fps_n, int fps_d, int bitrate, int gopsize);
int omx_streamer_deinit(omx_streamer_t *ctx);
int omx_streamer_suspend(omx_streamer_t *ctx);
int omx_streamer_resume(omx_streamer_t *ctx);
int omx_streamer_reconfigure(omx_streamer_t *ctx, int bitrateKbps, unsigned int framerate, unsigned int width, unsigned int height);
/* start streaming */
int omx_streamer_start(omx_streamer_t *ctx);
/* preparing stop: called before stop */
int omx_streamer_prepare_stop(omx_streamer_t *ctx);
/* stop streaming */
int omx_streamer_stop(omx_streamer_t *ctx);
/* get h.264 sps/pps */
const unsigned char * omx_streamer_get_h264_sps(omx_streamer_t *ctx, int *size);
const unsigned char * omx_streamer_get_h264_pps(omx_streamer_t *ctx, int *size);
/* get the encoded stream */
unsigned char * omx_streamer_get(omx_streamer_t *ctx, int *encsize);

#endif /* __OMX_STREAMER_H__ */
