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

#ifdef GA_EMCC
#include <SDL/SDL.h>
#else
#include <SDL2/SDL.h>
#endif /* GA_EMCC */
#include <pthread.h>

#include "rtspconf.h"
#include "vsource.h"
#include "pipeline.h"

#define	SDL_USEREVENT_CREATE_OVERLAY	0x0001
#define	SDL_USEREVENT_OPEN_AUDIO	0x0002
#define	SDL_USEREVENT_RENDER_IMAGE	0x0004
#define	SDL_USEREVENT_RENDER_TEXT	0x0008

#define SDL_AUDIO_BUFFER_SIZE		2048

extern int image_rendered;

struct RTSPThreadParam {
	const char *url;
	bool running;
	char quitLive555;
	// video
	int width[IMAGE_SOURCE_CHANNEL_MAX];
	int height[IMAGE_SOURCE_CHANNEL_MAX];
	PixelFormat format[IMAGE_SOURCE_CHANNEL_MAX];
	pthread_mutex_t surfaceMutex[IMAGE_SOURCE_CHANNEL_MAX];
#if SDL_VERSION_ATLEAST(2,0,0)
	unsigned int windowId[IMAGE_SOURCE_CHANNEL_MAX];
	SDL_Window *surface[IMAGE_SOURCE_CHANNEL_MAX];
	SDL_Renderer *renderer[IMAGE_SOURCE_CHANNEL_MAX];
	SDL_Texture *overlay[IMAGE_SOURCE_CHANNEL_MAX];
#else
	SDL_Surface *surface[IMAGE_SOURCE_CHANNEL_MAX];
	SDL_Overlay *overlay[IMAGE_SOURCE_CHANNEL_MAX];
#endif
	struct SwsContext *swsctx[IMAGE_SOURCE_CHANNEL_MAX];
	pipeline *pipe[IMAGE_SOURCE_CHANNEL_MAX];
	// audio
	pthread_mutex_t audioMutex;
	bool audioOpened;
};

extern struct RTSPConf *rtspconf;

void rtsperror(const char *fmt, ...);
void * rtsp_thread(void *param);

/* internal use only */
void audio_fill_buffer(void *userdata, unsigned char *stream, int ssize);

#endif
