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

#include "ga-common.h"
#include "ga-conf.h"
#include "server.h"
#include "asource.h"
#include "asource-system.h"

#ifdef ENABLE_AUDIO

#ifdef WIN32
#include "ga-win32-wasapi.h"
#else
#include "ga-alsa.h"
#endif

static bool initialized = false;
#ifdef WIN32
static struct Xcap_wasapi_param audioparam;
#else
static struct Xcap_alsa_param audioparam;
#endif

int
asource_init(void *arg) {
	int delay = 0;
	struct RTSPConf *rtspconf = rtspconf_global();
	if(initialized)
		return 0;
	//
	if((delay = ga_conf_readint("audio-init-delay")) > 0) {
		usleep(delay*1000);
	}
	//
	audioparam.channels = rtspconf->audio_channels;
	audioparam.samplerate = rtspconf->audio_samplerate;
	if(rtspconf->audio_device_format == AV_SAMPLE_FMT_S16) {
#ifdef WIN32
#else
		audioparam.format = SND_PCM_FORMAT_S16_LE;
#endif
		audioparam.bits_per_sample = 16;
	} else {
		ga_error("audio source: unsupported audio format (%d).\n",
			rtspconf->audio_device_format);
		return -1;
	}
	if(rtspconf->audio_device_channel_layout != AV_CH_LAYOUT_STEREO) {
		ga_error("audio source: unsupported channel layout (%llu).\n",
			rtspconf->audio_device_channel_layout);
		return -1;
	}
#ifdef WIN32
	if(ga_wasapi_init(&audioparam) < 0) {
		ga_error("WASAPI: initialization failed.\n");
		return -1;
	}
#else
	if((audioparam.handle = ga_alsa_init(&audioparam.sndlog)) == NULL) {
		ga_error("ALSA: initialization failed.\n");
		return -1;
	}
	if(ga_alsa_set_param(&audioparam) < 0) {
		ga_alsa_close(audioparam.handle, audioparam.sndlog);
		ga_error("ALSA: cannot set parameters\n");
		return -1;
	}
	do {
		snd_pcm_sframes_t delay;
		if(snd_pcm_delay(audioparam.handle, &delay) == 0) {
			ga_error("ALSA init: pcm delay = %d\n", delay);
		} else {
			ga_error("ALSA init: unable to retrieve pcm delay\n");
		}
	} while(0);
#endif
	if(audio_source_setup(audioparam.chunk_size, audioparam.samplerate, audioparam.bits_per_sample, audioparam.channels) < 0) {
		ga_error("audio source: setup failed.\n");
#ifdef WIN32
		ga_wasapi_close(&audioparam);
#else
		ga_alsa_close(audioparam.handle, audioparam.sndlog);
#endif
		return -1;
	}
	initialized = true;
	ga_error("audio source: setup chunk=%d, samplerate=%d, bps=%d, channels=%d\n",
		audioparam.chunk_size,
		audioparam.samplerate,
		audioparam.bits_per_sample,
		audioparam.channels);
	return 0;
}

void *
asource_threadproc(void *arg) {
	int r;
	unsigned char *fbuffer;
	//
	if(asource_init(NULL) < 0) {
		exit(-1);
	}
	if((fbuffer = (unsigned char*) malloc(audioparam.chunk_bytes)) == NULL) {
		ga_error("Audio source: malloc failed (%d bytes) - %s\n",
			audioparam.chunk_bytes, strerror(errno));
		exit(-1);
	}
	//
	ga_error("Audio source thread started: tid=%ld\n", ga_gettid());
	//
	while(true) {
#ifdef WIN32
		r = ga_wasapi_read(&audioparam, fbuffer, audioparam.chunk_size);
		if(r < 0) {
			ga_error("Audio source: WASAPI read failed.\n");
			break;
		}
#else
		r = snd_pcm_readi(audioparam.handle, fbuffer, audioparam.chunk_size);
		if(r == -EAGAIN) {
			snd_pcm_wait(audioparam.handle, 1000);
			continue;
		} else if(r < 0) {
			ga_error("Audio source: ALSA read failed - %s\n",
				snd_strerror(r));
			break;
		}
#endif
		audio_source_buffer_fill(fbuffer, r);
	}
	//
	ga_error("audio capture thread terminated.\n");
	//
	return NULL;
}

void
asource_deinit(void *arg) {
#ifdef WIN32
	ga_wasapi_close(&audioparam);
#else
	ga_alsa_close(audioparam.handle, audioparam.sndlog);
#endif
	return;
}

#endif	/* ENABLE_AUDIO */
