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

#ifndef __RTSPCLIENT_H__
#define __RTSPCLIENT_H__

#ifdef ANDROID
#include <jni.h>
#else
#include <SDL2/SDL.h>
#endif
#include <pthread.h>

#include "rtspconf.h"
#ifndef ANDROID
#include "vsource.h"
#endif
#include "dpipe.h"

#define	SDL_USEREVENT_CREATE_OVERLAY	0x0001
#define	SDL_USEREVENT_OPEN_AUDIO	0x0002
#define	SDL_USEREVENT_RENDER_IMAGE	0x0004
#define	SDL_USEREVENT_RENDER_TEXT	0x0008

#define SDL_AUDIO_BUFFER_SIZE		2048

extern int image_rendered;

#define	RTSP_VIDEOSTATE_NULL	0
 
#ifdef ANDROID
#define	VIDEO_SOURCE_CHANNEL_MAX	2
#endif

struct RTSPThreadParam {
	const char *url;
	bool running;
	bool rtpOverTCP;
	char quitLive555;
	// video
	int width[VIDEO_SOURCE_CHANNEL_MAX];
	int height[VIDEO_SOURCE_CHANNEL_MAX];
	AVPixelFormat format[VIDEO_SOURCE_CHANNEL_MAX];
	pthread_mutex_t surfaceMutex[VIDEO_SOURCE_CHANNEL_MAX];
	struct SwsContext *swsctx[VIDEO_SOURCE_CHANNEL_MAX];
	dpipe_t *pipe[VIDEO_SOURCE_CHANNEL_MAX];
#ifdef ANDROID
	JNIEnv *jnienv;
#else
#if 1	// only support SDL2
	unsigned int windowId[VIDEO_SOURCE_CHANNEL_MAX];
	SDL_Window *surface[VIDEO_SOURCE_CHANNEL_MAX];
	SDL_Renderer *renderer[VIDEO_SOURCE_CHANNEL_MAX];
	SDL_Texture *overlay[VIDEO_SOURCE_CHANNEL_MAX];
#endif
	// audio
	pthread_mutex_t audioMutex;
	bool audioOpened;
#endif
	int videostate;
};

extern struct RTSPConf *rtspconf;

void rtsperror(const char *fmt, ...);
void * rtsp_thread(void *param);

/* internal use only */
int audio_buffer_fill(void *userdata, unsigned char *stream, int ssize);
void audio_buffer_fill_sdl(void *userdata, unsigned char *stream, int ssize);
#ifdef ANDROID
void setRTSPThreadParam(struct RTSPThreadParam *param);
struct RTSPThreadParam * getRTSPThreadParam();
#endif

#endif
