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
#include <sys/time.h>

#include "ga-common.h"
#include "ga-alsa.h"

static snd_output_t *sndlog = NULL;

int
ga_alsa_set_param(struct ga_alsa_param *param) {
	snd_pcm_hw_params_t *hwparams = NULL;
	snd_pcm_sw_params_t *swparams = NULL;
	size_t bits_per_sample;
	unsigned int rate;
	unsigned int buffer_time = 500000;	// in the unit of microsecond
	unsigned int period_time = 125000;	// = buffer_time/4;
	int monotonic = 0;
	snd_pcm_uframes_t start_threshold, stop_threshold;
	int err;
	//
	snd_pcm_hw_params_alloca(&hwparams);
	snd_pcm_sw_params_alloca(&swparams);
	if((err = snd_pcm_hw_params_any(param->handle, hwparams)) < 0) {
		ga_error("ALSA: set_param - no configurations available\n");
		return -1;
	}
	if((err = snd_pcm_hw_params_set_access(param->handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		ga_error("ALSA: set_param - access type (interleaved) not available\n");
		return -1;
	}
	if((err = snd_pcm_hw_params_set_format(param->handle, hwparams, param->format)) < 0) {
		ga_error("ALSA: set_param - unsupported sample format.\n");
		return -1;
	}
	if((err = snd_pcm_hw_params_set_channels(param->handle, hwparams, param->channels)) < 0) {
		ga_error("ALSA: set_param - channles count not available\n");
		return -1;
	}
	rate = param->samplerate;
	if((err = snd_pcm_hw_params_set_rate_near(param->handle, hwparams, &rate, 0)) < 0) {
		ga_error("ALSA: set_param - set rate failed.\n");
		return -1;
	}
	if((double)param->samplerate*1.05 < rate || (double)param->samplerate*0.95 > rate) {
		ga_error("ALSA: set_param/warning - inaccurate rate (req=%iHz, got=%iHz)\n", param->samplerate, rate);
	}
	//
	period_time = buffer_time/4;
	if((err = snd_pcm_hw_params_set_period_time_near(param->handle, hwparams, &period_time, 0)) < 0) {
		ga_error("ALSA: set_param - set period time failed.\n");
		return -1;
	}
	if((err = snd_pcm_hw_params_set_buffer_time_near(param->handle, hwparams, &buffer_time, 0)) < 0) {
		ga_error("ALSA: set_param - set buffer time failed.\n");
		return -1;
	}
	//
	monotonic = snd_pcm_hw_params_is_monotonic(hwparams);
	if((err = snd_pcm_hw_params(param->handle, hwparams)) < 0) {
		ga_error("ALSA: set_param - unable to install hw params:");
		snd_pcm_hw_params_dump(hwparams, sndlog);
		return -1;
	}
	snd_pcm_hw_params_get_period_size(hwparams, &param->chunk_size, 0);
	snd_pcm_hw_params_get_buffer_size(hwparams, &param->buffer_size);
	if(param->chunk_size == param->buffer_size) {
		ga_error("ALSA: set_param - cannot use period equal to buffer size (%lu==%lu)\n",
			param->chunk_size, param->buffer_size);
		return -1;
	}
	//
	snd_pcm_sw_params_current(param->handle, swparams);
	err = snd_pcm_sw_params_set_avail_min(param->handle, swparams, param->chunk_size);
	// start_delay = 1 for capture
	start_threshold = (double) param->samplerate * /*start_delay=*/ 1 / 1000000;
	if(start_threshold < 1)				start_threshold = 1;
	if(start_threshold > param->buffer_size)	start_threshold = param->buffer_size;
	if((err = snd_pcm_sw_params_set_start_threshold(param->handle, swparams, start_threshold)) < 0) {
		ga_error("ALSA: set_param - set start threshold failed.\n");
		return -1;
	}
	// stop_delay = 0
	stop_threshold = param->buffer_size;
	if((err = snd_pcm_sw_params_set_stop_threshold(param->handle, swparams, stop_threshold)) < 0) {
		ga_error("ALSA: set_param - set stop threshold failed.\n");
		return -1;
	}
	//
	if(snd_pcm_sw_params(param->handle, swparams) < 0) {
		ga_error("ALSA: set_param - unable to install sw params:");
		snd_pcm_sw_params_dump(swparams, sndlog);
		return -1;
	}

	bits_per_sample = snd_pcm_format_physical_width(param->format);
	if(param->bits_per_sample != bits_per_sample) {
		ga_error("ALSA: set_param - BPS/HW configuration mismatched %d != %d)\n",
			param->bits_per_sample, bits_per_sample);
	}
	param->bits_per_frame = param->bits_per_sample * param->channels;
	param->chunk_bytes = param->chunk_size * param->bits_per_frame / 8;

	return 0;
}

snd_pcm_t *
ga_alsa_init(snd_output_t **pout) {
	snd_pcm_t *handle;
	int err;
	//
	if(pout) {
		if(snd_output_stdio_attach(pout, stderr, 0) < 0) {
			ga_error("cannot attach stderr\n");
			return NULL;
		}
	}
	// XXX: debug
	do {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		fprintf(stderr, "%ld.%06ld\n", tv.tv_sec, tv.tv_usec);
	} while(0);
	if((err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		ga_error("open failed: %s\n", snd_strerror(err));
		snd_output_close(*pout);
		return NULL;
	}
	return handle;
}

void
ga_alsa_close(snd_pcm_t *handle, snd_output_t *pout) {
	if(pout)
		snd_output_close(pout);
	if(handle)
		snd_pcm_close(handle);
}

