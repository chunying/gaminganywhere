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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <map>

#include "asource.h"

#include "ga-common.h"

using namespace std;

static pthread_mutex_t ccmutex = PTHREAD_MUTEX_INITIALIZER;
static map<long,audio_buffer_t*> gClients;
//
static int gChunksize = 0;
static int gSamplerate = 0;
static int gBitspersample = 0;
static int gChannels = 0;

audio_buffer_t *
audio_source_buffer_init() {
	// XXX:	frames, chennels, and bitspersample should be the same as the
	//	configuration -- since these are provided by encoders (clients)
	audio_buffer_t *ab;
	int frames = gChunksize*4;
	int channels = gChannels;
	int bitspersample = gBitspersample;
	if(frames == 0
	|| channels == 0
	|| bitspersample == 0) {
		ga_error("audio source: invalid argument (frames=%d, channels=%d, bitspersample=%d)\n",
			frames, channels, bitspersample);
		return NULL;
	}
	if((ab = (audio_buffer_t*) malloc(sizeof(audio_buffer_t))) == NULL) {
		return NULL;
	}
	bzero(ab, sizeof(audio_buffer_t));
	pthread_mutex_init(&ab->bufmutex, NULL);
	pthread_cond_init(&ab->bufcond, NULL);
	ab->frames = frames;
	ab->channels = channels;
	ab->bitspersample = bitspersample;
	ab->bufsize = frames * channels * bitspersample / 8;
	if((ab->buffer = (unsigned char*) malloc(ab->bufsize)) == NULL) {
		free(ab);
		return NULL;
	}
	return ab;
}

void
audio_source_buffer_deinit(audio_buffer_t *ab) {
	if(ab == NULL)
		return;
	if(ab->buffer != NULL)
		free(ab->buffer);
	free(ab);
	return;
}

void
audio_source_buffer_fill_one(audio_buffer_t *ab, const unsigned char *data, int frames) {
	int headspace, tailspace;
	int framesize;
	if(ab == NULL)
		return;
	if(frames <= 0)
		return;
	framesize = frames * ab->channels * ab->bitspersample / 8;
	pthread_mutex_lock(&ab->bufmutex);
retry:
	headspace = ab->bufhead;
	tailspace = ab->bufsize - ab->buftail;
	if(framesize > headspace + tailspace) {
		ga_error("Audio source: buffer overflow, packet dropped (%d frames)\n", frames);
		pthread_mutex_unlock(&ab->bufmutex);
		pthread_cond_signal(&ab->bufcond);
		return;
	}
	// we GUARANTEE that bufhead <= buftail
	if(framesize <= tailspace) {
		// case #1: tailspace is sufficient, append at the end
		if(data == NULL) {
			bzero(&ab->buffer[ab->buftail], framesize);
		} else {
			bcopy(data, &ab->buffer[ab->buftail], framesize);
		}
		ab->buftail += framesize;
		ab->bframes += frames;
		pthread_mutex_unlock(&ab->bufmutex);
		pthread_cond_signal(&ab->bufcond);
		return;
	}
	// case #2: framesize > tailspace, but overall space is sufficient
	bcopy(&ab->buffer[ab->bufhead], ab->buffer, ab->buftail - ab->bufhead);
	ab->buftail -= ab->bufhead;
	ab->bufhead = 0;
	goto retry;
	// never reach here
	return;
}

void
audio_source_buffer_fill(const unsigned char *data, int frames) {
	map<long,audio_buffer_t*>::iterator mi;
	pthread_mutex_lock(&ccmutex);
	for(mi = gClients.begin(); mi != gClients.end(); mi++) {
		if(mi->second != NULL) {
			audio_source_buffer_fill_one(mi->second, data, frames);
		}
	}
	pthread_mutex_unlock(&ccmutex);
}

int
audio_source_buffer_read(audio_buffer_t *ab, unsigned char *buf, int frames) {
	int copyframe = 0, copysize = 0;
	struct timeval tv;
	struct timespec to;
	//
	if(frames <= 0) {
		return 0;
	}
	//
	pthread_mutex_lock(&ab->bufmutex);
	//
	if(ab->bframes == 0) {
		gettimeofday(&tv, NULL);
		to.tv_sec = tv.tv_sec+1;
		to.tv_nsec = tv.tv_usec * 1000;
		pthread_cond_timedwait(&ab->bufcond, &ab->bufmutex, &to);
	}
	if(ab->bframes >= frames) {
		copyframe = frames;
	} else {
		copyframe = ab->bframes;
	}
	if(copyframe > 0) {
		copysize = copyframe * ab->channels * ab->bitspersample / 8;
		//
		bcopy(&ab->buffer[ab->bufhead], buf, copysize);
		//
		ab->bufhead += copysize;
		ab->bframes -= copyframe;
		ab->bufPts += copyframe;
		if(ab->bframes == 0) {
			ab->bufhead = ab->buftail = 0;
		}
	}
	//
	pthread_mutex_unlock(&ab->bufmutex);
	//
	return copyframe;
}

void
audio_source_buffer_purge(audio_buffer_t *ab) {
	ga_error("audio: buffer purged (%d bytes / %d frames).\n",
		ab->buftail - ab->bufhead, ab->bframes);
	pthread_mutex_lock(&ab->bufmutex);
	ab->bufPts = 0LL;
	ab->bufhead = ab->buftail = ab->bframes = 0;
	pthread_mutex_unlock(&ab->bufmutex);
	return;
}

void
audio_source_client_register(long tid, audio_buffer_t *ab) {
	pthread_mutex_lock(&ccmutex);
	gClients[tid] = ab;
	pthread_mutex_unlock(&ccmutex);
}

void
audio_source_client_unregister(long tid) {
	pthread_mutex_lock(&ccmutex);
	gClients.erase(tid);
	pthread_mutex_unlock(&ccmutex);
}

int
audio_source_client_count() {
	unsigned n;
	pthread_mutex_lock(&ccmutex);
	n = gClients.size();
	pthread_mutex_unlock(&ccmutex);
	return n;
}

int
audio_source_chunksize() {
	return gChunksize;
}

int
audio_source_chunkbytes() {
	return gChunksize * gChannels * gBitspersample / 8;
}

int
audio_source_samplerate() {
	return gSamplerate;
}

int
audio_source_bitspersample() {
	return gBitspersample;
}

int
audio_source_channels() {
	return gChannels;
}

int
audio_source_setup(int chunksize, int samplerate, int bitspersample, int channels) {
	gChunksize = chunksize;
	gSamplerate = samplerate;
	gBitspersample = bitspersample;
	gChannels = channels;
	return 0;
}

