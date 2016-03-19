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

#ifndef __XCAP_WIN32_WASAPI_H__
#define __XCAP_WIN32_WASAPI_H__

#include <pthread.h>

#include "ga-common.h"

#include <mmdeviceapi.h>
#include <audioclient.h>

struct ga_wasapi_param {
	UINT32 bufferFrameCount;
	REFERENCE_TIME hnsActualDuration;
	DWORD bufferFillInt;
	//
	IMMDeviceEnumerator *pEnumerator;
	IMMDevice *pDevice;
	IAudioClient *pAudioClient;
	IAudioCaptureClient *pCaptureClient;
	WAVEFORMATEX *pwfx;
	//
	struct timeval initialTimestamp;
	struct timeval firstRead;
	struct timeval silenceFrom;
	UINT64 fillSilence;
	UINT64 trimmedFrames;
	//
	LARGE_INTEGER startTime;
	int isFloat;
	int format;
	int channels;
	int samplerate;
	int bits_per_sample;	// S16 = 16-bits; Float = 32-bits
	int chunk_size;		// # of frames in a read
	int bits_per_frame;	// bits_per_sample * # of channels
	int chunk_bytes;	// chunk_size * bits_per_frame
	// DEBUG
	struct timeval lastTv;
	int sframes;
	int frames;
	int slept;
};

int ga_wasapi_init(ga_wasapi_param *wasapi);
int ga_wasapi_read(ga_wasapi_param *wasapi, unsigned char *wbuf, int wframes);
int ga_wasapi_close(ga_wasapi_param *wasapi);

#endif
