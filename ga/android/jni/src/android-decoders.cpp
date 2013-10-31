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

#include "ga-common.h"
#include "rtspclient.h"
#include "libgaclient.h"
#include "minih264.h"
#include "minivp8.h"
#include "android-decoders.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#ifdef __cplusplus
}
#endif

int
android_prepare_audio(RTSPThreadParam *rtspParam, const char *mime, bool builtinDecoder) {
	if(initAudio(rtspParam->jnienv, mime, 0, 0, builtinDecoder) == NULL) {
		rtsperror("rtspclient: initAudio failed.\n");
		rtspParam->quitLive555 = 1;
		return -1;
	}
	if(builtinDecoder) {
		rtsperror("init built-in audio decoder.\n");
		if(startAudioDecoder(rtspParam->jnienv) == NULL) {
			rtsperror("rtspclient: start audio decoder failed.\n");
			rtspParam->quitLive555 = 1;
			return -1;
		}
	}
	return 0;
}

int
android_decode_audio(RTSPThreadParam *rtspParam, unsigned char *buffer, int bufsize, struct timeval pts) {
	if(bufsize > 0) {
		decodeAudio(rtspParam->jnienv, buffer, bufsize, pts, 0);
	}
	return 0;
}

#define	RTSP_VIDEOSTATE_SPS_RCVD	1
#define	RTSP_VIDEOSTATE_PPS_RCVD	2

int
android_decode_h264(RTSPThreadParam *rtspParam, unsigned char *buffer, int bufsize, struct timeval pts, bool marker) {
	struct mini_h264_context ctx;
	// look for sps/pps
	if(mini_h264_parse(&ctx, buffer, bufsize) != 0) {
		rtsperror("%lu.%06lu bad h.264 unit.\n", pts.tv_sec, pts.tv_usec);
		return -1;
	}
	if(ctx.type == 7) {
		// sps
		if(rtspParam->videostate == RTSP_VIDEOSTATE_NULL) {
			rtsperror("rtspclient: initial SPS received.\n");
			if(initVideo(rtspParam->jnienv, "video/avc", ctx.width, ctx.height) == NULL) {
				rtsperror("rtspclient: initVideo failed.\n");
				rtspParam->quitLive555 = 1;
				return -1;
			} else {
				rtsperror("rtspclient: initVideo success [video/avc@%ux%d]\n",
					ctx.width, ctx.height);
			}
			if(ctx.rawsps != NULL && ctx.spslen > 0) {
				videoSetByteBuffer(rtspParam->jnienv, "csd-0", ctx.rawsps, ctx.spslen);
				free(ctx.rawsps);
			}
			rtspParam->videostate = RTSP_VIDEOSTATE_SPS_RCVD;
			return -1;
		}
	} else if(ctx.type == 8) {
		// pps 
		if(rtspParam->videostate == RTSP_VIDEOSTATE_SPS_RCVD) {
			rtsperror("rtspclient: initial PPS received.\n");
			if(ctx.rawpps != NULL && ctx.ppslen > 0) {
				videoSetByteBuffer(rtspParam->jnienv, "csd-1", ctx.rawpps, ctx.ppslen);
				free(ctx.rawpps);
			}
			if(startVideoDecoder(rtspParam->jnienv) == NULL) {
				rtsperror("rtspclient: cannot start video decoder.\n");
				rtspParam->quitLive555 = 1;
				return -1;
			} else {
				rtsperror("rtspclient: video decoder started.\n");
			}
			rtspParam->videostate = RTSP_VIDEOSTATE_PPS_RCVD;
			return -1;
		}
	}
	//
	if(rtspParam->videostate != RTSP_VIDEOSTATE_PPS_RCVD) {
		// drop the frame
		rtsperror("rtspclient: drop video frame, state=%d type=%d\n", rtspParam->videostate, ctx.type);
		return -1;
	}
	if(ctx.is_config) {
		//rtsperror("rtspclient: got a config packet, type=%d\n", ctx.type);
		decodeVideo(rtspParam->jnienv, buffer, bufsize, pts, marker, BUFFER_FLAG_CODEC_CONFIG);
		return -1;
	}
	//
	if(ctx.type == 1 || ctx.type == 5 || ctx.type == 19) {
		if(ctx.frametype == TYPE_I_FRAME || ctx.frametype == TYPE_SI_FRAME) {
			// XXX: enable intra-refresh at the server will disable IDR/I-frames
			// need to do something?
			//rtsperror("got an I/SI frame, type = %d/%d(%d)\n", ctx.type, ctx.frametype, ctx.slicetype);
		}
	}
	decodeVideo(rtspParam->jnienv, buffer, bufsize, pts, marker, 0/*marker ? BUFFER_FLAG_SYNC_FRAME : 0*/);
	return 0;
}

// Ref:
//	RTP: http://tools.ietf.org/html/draft-ietf-payload-vp8-09
//	Payload: http://datatracker.ietf.org/doc/rfc6386/

#define	RTSP_VIDEOSTATE_HAS_KEYFRAME	1

int
android_decode_vp8(RTSPThreadParam *rtspParam, unsigned char *buffer, int bufsize, struct timeval pts, bool marker) {
	struct mini_vp8_context ctx;
	if(mini_vp8_parse(&ctx, buffer, bufsize) != 0) {
		rtsperror("%lu.%06lu bad vp8 unit.\n", pts.tv_sec, pts.tv_usec);
		return -1;
	}
	if(ctx.is_keyframe) {
		if(rtspParam->videostate == RTSP_VIDEOSTATE_NULL) {
			rtsperror("rtspclient: initial KEYFRAME received.\n");
			if(initVideo(rtspParam->jnienv, "video/x-vnd.on2.vp8", ctx.width, ctx.height) == NULL) {
				rtsperror("rtspclient: initVideo failed.\n");
				rtspParam->quitLive555 = 1;
				return -1;
			} else {
				rtsperror("rtspclient: initVideo success [video/x-vnd2.on2.vp8@%ux%d]\n",
					ctx.width, ctx.height);
			}
			if(startVideoDecoder(rtspParam->jnienv) == NULL) {
				rtsperror("rtspclient: cannot start video decoder.\n");
				rtspParam->quitLive555 = 1;
				return -1;
			}
			rtspParam->videostate = RTSP_VIDEOSTATE_HAS_KEYFRAME;
		}
	}
	//
	if(rtspParam->videostate != RTSP_VIDEOSTATE_HAS_KEYFRAME) {
		// drop the frame
		rtsperror("rtspclient: drop non-keyframe video frame\n");
		return -1;
	}
	//
	decodeVideo(rtspParam->jnienv, buffer, bufsize, pts, marker, ctx.is_keyframe==0 ? 0 : BUFFER_FLAG_SYNC_FRAME);
	//
	return 0;
}

