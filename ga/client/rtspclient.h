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
#include "pipeline.h"

#define	SDL_USEREVENT_CREATE_OVERLAY	0x0001
#define	SDL_USEREVENT_OPEN_AUDIO	0x0002
#define	SDL_USEREVENT_RENDER_IMAGE	0x0004
#define	SDL_USEREVENT_RENDER_TEXT	0x0008

#define SDL_AUDIO_BUFFER_SIZE		2048

extern int image_rendered;

#define	RTSP_VIDEOSTATE_NULL	0
 
#ifdef ANDROID
#define	IMAGE_SOURCE_CHANNEL_MAX	2
#endif

struct RTSPThreadParam {
	const char *url;
	bool running;
#ifdef ANDROID
	bool rtpOverTCP;
#endif
	char quitLive555;
	// video
	int width[IMAGE_SOURCE_CHANNEL_MAX];
	int height[IMAGE_SOURCE_CHANNEL_MAX];
	PixelFormat format[IMAGE_SOURCE_CHANNEL_MAX];
#ifdef ANDROID
	JNIEnv *jnienv;
	pthread_mutex_t surfaceMutex[IMAGE_SOURCE_CHANNEL_MAX];
	struct SwsContext *swsctx[IMAGE_SOURCE_CHANNEL_MAX];
	pipeline *pipe[IMAGE_SOURCE_CHANNEL_MAX];
#else
	pthread_mutex_t surfaceMutex[IMAGE_SOURCE_CHANNEL_MAX];
#if 1	// only support SDL2
	unsigned int windowId[IMAGE_SOURCE_CHANNEL_MAX];
	SDL_Window *surface[IMAGE_SOURCE_CHANNEL_MAX];
	SDL_Renderer *renderer[IMAGE_SOURCE_CHANNEL_MAX];
	SDL_Texture *overlay[IMAGE_SOURCE_CHANNEL_MAX];
#endif
	struct SwsContext *swsctx[IMAGE_SOURCE_CHANNEL_MAX];
	pipeline *pipe[IMAGE_SOURCE_CHANNEL_MAX];
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
