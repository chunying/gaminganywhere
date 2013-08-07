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

#ifndef __RTSP_SERVER_H__
#define __RTSP_SERVER_H__

#include <pthread.h>

#include "vsource.h"
#include "ga-common.h"
#include "ga-avcodec.h"

// acquired from ffmpeg source code
#ifdef __cplusplus
extern "C" {
#endif
#include "ffmpeg/rtsp.h"
#include "ffmpeg/rtspcodes.h"
int ffio_open_dyn_packet_buf(AVIOContext **, int);
#ifdef __cplusplus
}
#endif

#define	RTSP_CHANNEL_MAX	8

enum RTSPServerState {
	SERVER_STATE_IDLE = 0,
	SERVER_STATE_READY,
	SERVER_STATE_PLAYING,
	SERVER_STATE_PAUSE,
	SERVER_STATE_TEARDOWN
};

struct RTSPContext {
#ifdef WIN32
	SOCKET fd;
#else
	int fd;
#endif
	//
	int state;
	int hasVideo;
	// video
	// audio
	// threads
	pthread_t ithread, vthread, athread;
	long vthreadId;
	//// RTSP
	// internal read buffer
	char *rbuffer;
	int rbufhead;
	int rbuftail;
	int rbufsize;
	// for creating SDP
	AVFormatContext *sdp_fmtctx;
	AVStream *sdp_vstream[IMAGE_SOURCE_CHANNEL_MAX];
	AVStream *sdp_astream;
	AVCodecContext *sdp_vencoder[IMAGE_SOURCE_CHANNEL_MAX];
	AVCodecContext *sdp_aencoder;
	// for real audio/video encoding
	int seq;
	char *session_id;
	enum RTSPLowerTransport lower_transport[RTSP_CHANNEL_MAX];
	AVFormatContext *fmtctx[RTSP_CHANNEL_MAX];
	AVStream *stream[RTSP_CHANNEL_MAX];
	AVCodecContext *encoder[RTSP_CHANNEL_MAX];
	// streaming
	URLContext *rtp[RTSP_CHANNEL_MAX];	// RTP over UDP
	pthread_mutex_t rtsp_writer_mutex;	// RTP over RTSP/TCP
};

EXPORT void rtsp_cleanup(RTSPContext *rtsp, int retcode);
EXPORT int rtsp_write_bindata(RTSPContext *ctx, int streamid, uint8_t *buf, int buflen);
EXPORT void* rtspserver(void *arg);

#endif
