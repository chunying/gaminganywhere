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

#include <stdio.h>
#include <unistd.h>
#include <pulse/simple.h>
#include <pulse/error.h>

#include "ga-common.h"
#include "ga-conf.h"
#include "ga-module.h"
#include "rtspconf.h"
#include "asource.h"

#ifdef ENABLE_AUDIO

#define	PULSEAUDIO_CHUNKSIZE	1024
#define AUDIOBUF_BUFSIZE	16384

static int asource_initialized = 0;
static int asource_started = 0;
static pthread_t asource_tid;

static pa_simple	*pa_ctx = NULL;
static pa_sample_spec	pa_spec;

static int
asource_init(void *arg) {
	int error;
	const char *dev = "auto_null.monitor";
	char pa_devname[64];
	pa_usec_t delay = 0;
	struct RTSPConf *rtspconf = rtspconf_global();
	if(asource_initialized != 0)
		return 0;
	if((delay = ga_conf_readint("audio-init-delay")) > 0) {
		usleep(delay*1000);
	}
	//
	if(rtspconf->audio_device_format != AV_SAMPLE_FMT_S16) {
		ga_error("audio source: unsupported audio format (%d).\n",
			rtspconf->audio_device_format);
		return -1;
	}
	if(rtspconf->audio_device_channel_layout != AV_CH_LAYOUT_STEREO) {
		ga_error("audio source: unsupported channel layout (%llu).\n",
			rtspconf->audio_device_channel_layout);
		return -1;
	}
	if(ga_conf_readv("audio-capture-device", pa_devname, sizeof(pa_devname)) != NULL) {
		dev = pa_devname;
	}
	ga_error("audio source: device name = %s\n", dev);
	//
	bzero(&pa_spec, sizeof(pa_spec));
	pa_spec.channels = rtspconf->audio_channels;
	pa_spec.rate = rtspconf->audio_samplerate;
	pa_spec.format = PA_SAMPLE_S16LE;
	pa_ctx = pa_simple_new(NULL, "gaminganywhere-asource-pulseaudio",
			PA_STREAM_RECORD, dev,
			"gaminganywhere-record-stream", &pa_spec, NULL, NULL, &error);
	if(pa_ctx == NULL) {
		ga_error("audio source: pulseaudio initialization failed - %d:%s.\n",
			error, pa_strerror(error));
		return -1;
	}

	pa_simple_get_latency(pa_ctx, &error);
	if(error) {
		ga_error("audio source: pulseaudio unable to retrieve latency - %d:%s\n",
			error, pa_strerror(error));
	} else {
		ga_error("audio source: pulseaudio latency = %dus\n", delay);
	}
	
	if(audio_source_setup(AUDIOBUF_BUFSIZE, pa_spec.rate, 16, pa_spec.channels) < 0) {
		ga_error("audio source: setup failed.\n");
		pa_simple_free(pa_ctx);
		pa_ctx = NULL;
		return -1;
	}

	asource_initialized = 1;
	ga_error("audio source: setup chunk=%d, bufsize=%d, samplerate=%d, bits-per-sample=%d, channels=%d\n",
		PULSEAUDIO_CHUNKSIZE,
		AUDIOBUF_BUFSIZE,
		pa_spec.rate,
		16,
		pa_spec.channels);

	return 0;
}

static void *
asource_threadproc(void *arg) {
	int error;
	int framesize;
	unsigned char *fbuffer = NULL;
	//
	if(asource_init(NULL) < 0) {
		exit(-1);
	}
	if((fbuffer = (unsigned char*) malloc(PULSEAUDIO_CHUNKSIZE)) == NULL) {
		ga_error("audio source: malloc failed (%d bytes) - %s\n",
			PULSEAUDIO_CHUNKSIZE, strerror(errno));
		exit(-1);
	}
	framesize = pa_spec.channels * 2;	// 2: bytes-per-sample
	//
	ga_error("audio source thread started: tid=%ld\n", ga_gettid());
	//
	while(asource_started != 0) {
		if(pa_simple_read(pa_ctx, fbuffer, PULSEAUDIO_CHUNKSIZE, &error) < 0) {
			ga_error("audio source: pulseaudio read failed (%d)\n", error);
			break;
		}
		audio_source_buffer_fill(fbuffer, PULSEAUDIO_CHUNKSIZE/framesize);
	}
	//
	if(fbuffer)
		free(fbuffer);
	ga_error("audio source: capture thread terminated.\n");
	//
	return NULL;
}

static int 
asource_deinit(void *arg) {
	if(pa_ctx != NULL)
		pa_simple_free(pa_ctx);
	pa_ctx = NULL;
	asource_initialized = 0;
	return 0;
}

static int
asource_start(void *arg) {
	if(asource_started != 0)
		return 0;
	asource_started = 1;
	if(pthread_create(&asource_tid, NULL, asource_threadproc, arg) != 0) {
		asource_started = 0;
		ga_error("audio source: create thread failed.\n");
		return -1;
	}
	pthread_detach(asource_tid);
	return 0;
}

static int
asource_stop(void *arg) {
	if(asource_started == 0)
		return 0;
	asource_started = 0;
	pthread_cancel(asource_tid);
	return 0;
}

ga_module_t *
module_load() {
	static ga_module_t m;
	bzero(&m, sizeof(m));
	m.type = GA_MODULE_TYPE_ASOURCE;
	m.name = strdup("asource-pulseaudio");
	m.init = asource_init;
	m.start = asource_start;
	//m.threadproc = asource_threadproc;
	m.stop = asource_stop;
	m.deinit = asource_deinit;
	return &m;
}

#endif	/* ENABLE_AUDIO */
