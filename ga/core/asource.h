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

#ifndef __ASOURCE_H__
#define __ASOURCE_H__

#include <pthread.h>

#include "ga-common.h"

typedef struct audio_buffer_s {
	pthread_mutex_t bufmutex;
	pthread_cond_t bufcond;
	long long bufPts;
	int frames, channels, bitspersample;
	int bufsize, bufhead, buftail, bframes;
	unsigned char *buffer;
}	audio_buffer_t;

EXPORT audio_buffer_t * audio_source_buffer_init();
EXPORT void audio_source_buffer_deinit(audio_buffer_t *ab);
EXPORT void audio_source_buffer_fill_one(audio_buffer_t *ab, const unsigned char *data, int frames);
EXPORT void audio_source_buffer_fill(const unsigned char *data, int frames);
EXPORT int audio_source_buffer_read(audio_buffer_t *ab, unsigned char *buf, int frames);
EXPORT void audio_source_buffer_purge(audio_buffer_t *ab);
EXPORT void audio_source_client_register(long tid, audio_buffer_t *ab);
EXPORT void audio_source_client_unregister(long tid);
EXPORT int audio_source_client_count();

EXPORT int audio_source_chunksize();
EXPORT int audio_source_chunkbytes();
EXPORT int audio_source_samplerate();
EXPORT int audio_source_bitspersample();
EXPORT int audio_source_channels();
EXPORT int audio_source_setup(int chunksize, int samplerate, int bitspersample, int channels);

#endif
