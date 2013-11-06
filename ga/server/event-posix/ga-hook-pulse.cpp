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
#include <errno.h>
#include <pthread.h>
#ifndef WIN32
#include <dlfcn.h>
#endif

#include "ga-common.h"
#include "ga-conf.h"
#include "ga-avcodec.h"
#include "vsource.h"
#include "asource.h"
#include "rtspconf.h"

#include "ga-hook-common.h"
#include "ga-hook-lib.h"
#include "ga-hook-pulse.h"

#include <map>
using namespace std;

#define	PA_MAX_SAMPLES		8192

// for hooking
// simple
t_pa_simple_new			old_pa_simple_new = NULL;
t_pa_simple_free		old_pa_simple_free = NULL;
t_pa_simple_write		old_pa_simple_write = NULL;
// async
t_pa_stream_new			old_pa_stream_new = NULL;
t_pa_stream_new_extended	old_pa_stream_new_extended = NULL;
t_pa_stream_new_with_proplist	old_pa_stream_new_with_proplist = NULL;
t_pa_stream_connect_playback	old_pa_stream_connect_playback = NULL;
t_pa_stream_set_write_callback	old_pa_stream_set_write_callback = NULL;
t_pa_stream_write		old_pa_stream_write = NULL;

// my data
static map<pa_stream*, pa_stream_request_cb_t> pawCallback;
static map<pa_stream*, pa_sample_spec> paSpecs;
static pa_simple *pa_simple_main = NULL;
static pa_stream *pa_stream_main = NULL;
//
static int pa_bytes_per_sample = 0;
static int pa_samplerate = 0;
static int ga_samplerate = 0;
static int ga_channels = 0;
static struct SwrContext *swrctx = NULL;
static unsigned char *audio_buf = NULL;
//

static void
pulse_hook_symbols() {
#ifndef WIN32
	void *handle = NULL, *hSimple = NULL, *hPulse = NULL;
	char *ptr, soname[2048];
	// hSimple: pulse-simple
	if((ptr = getenv("LIBPULSESIMPLE_SO")) == NULL) {
		strncpy(soname, "libpulse-simple.so", sizeof(soname));
	} else {
		strncpy(soname, ptr, sizeof(soname));
	}
	if((hSimple = dlopen(soname, RTLD_LAZY)) == NULL) {
		ga_error("dlopen '%s' failed: %s\n", soname, strerror(errno));
		exit(-1);
	}
	// hPulse: pulse
	if((ptr = getenv("LIBPULSE_SO")) == NULL) {
		strncpy(soname, "libpulse.so", sizeof(soname));
	} else {
		strncpy(soname, ptr, sizeof(soname));
	}
	if((hPulse = dlopen(soname, RTLD_LAZY)) == NULL) {
		ga_error("dlopen '%s' failed: %s\n", soname, strerror(errno));
		exit(-1);
	}
	// simple
	old_pa_simple_new = (t_pa_simple_new)
				ga_hook_lookup_or_quit(hSimple, "pa_simple_new");
	old_pa_simple_free = (t_pa_simple_free)
				ga_hook_lookup_or_quit(hSimple, "pa_simple_free");
	old_pa_simple_write = (t_pa_simple_write)
				ga_hook_lookup_or_quit(hSimple, "pa_simple_write");
	// async
	old_pa_stream_new = (t_pa_stream_new)
				ga_hook_lookup_or_quit(hPulse, "pa_stream_new");
	old_pa_stream_new_extended = (t_pa_stream_new_extended)
				ga_hook_lookup_or_quit(hPulse, "pa_stream_new_extended");
	old_pa_stream_new_with_proplist = (t_pa_stream_new_with_proplist)
				ga_hook_lookup_or_quit(hPulse, "pa_stream_new_with_proplist");
	old_pa_stream_connect_playback = (t_pa_stream_connect_playback)
				ga_hook_lookup_or_quit(hPulse, "pa_stream_connect_playback");
	old_pa_stream_set_write_callback = (t_pa_stream_set_write_callback)
				ga_hook_lookup_or_quit(hPulse, "pa_stream_set_write_callback");
	old_pa_stream_write = (t_pa_stream_write)
				ga_hook_lookup_or_quit(hPulse, "pa_stream_write");
	// register via dlsym?
	if((ptr = getenv("HOOKAUDIO")) == NULL)
		goto quit;
	strncpy(soname, ptr, sizeof(soname));
	// hook indirectly
	if((handle = dlopen(soname, RTLD_LAZY)) != NULL) {
	//////////////////////////////////////////////////
	hook_lib_generic(soname, handle, "pa_simple_new", (void*) hook_pa_simple_new);
	hook_lib_generic(soname, handle, "pa_simple_free", (void*) hook_pa_simple_free);
	hook_lib_generic(soname, handle, "pa_simple_write", (void*) hook_pa_simple_write);
	//
	hook_lib_generic(soname, handle, "pa_stream_new", (void*) hook_pa_stream_new);
	hook_lib_generic(soname, handle, "pa_stream_new_extended", (void*) hook_pa_stream_new_extended);
	hook_lib_generic(soname, handle, "pa_stream_new_with_proplist", (void*) hook_pa_stream_new_with_proplist);
	hook_lib_generic(soname, handle, "pa_stream_connect_playback", (void*) hook_pa_stream_connect_playback);
	hook_lib_generic(soname, handle, "pa_stream_set_write_callback", (void*) hook_pa_stream_set_write_callback);
	hook_lib_generic(soname, handle, "pa_stream_write", (void*) hook_pa_stream_write);
	//////////////////////////////////////////////////
	ga_error("hook-pulse: hooked into %s\n", soname);
	}
	// hook via libdl
	register_dlsym_hooks("pa_simple_new", (void*) hook_pa_simple_new);
	register_dlsym_hooks("pa_simple_free", (void*) hook_pa_simple_free);
	register_dlsym_hooks("pa_simple_write", (void*) hook_pa_simple_write);
	//
	register_dlsym_hooks("pa_stream_new", (void*) hook_pa_stream_new);
	register_dlsym_hooks("pa_stream_new_extended", (void*) hook_pa_stream_new_extended);
	register_dlsym_hooks("pa_stream_new_with_proplist", (void*) hook_pa_stream_new_with_proplist);
	register_dlsym_hooks("pa_stream_connect_playback", (void*) hook_pa_stream_connect_playback);
	register_dlsym_hooks("pa_stream_set_write_callback", (void*) hook_pa_stream_set_write_callback);
	register_dlsym_hooks("pa_stream_write", (void*) hook_pa_stream_write);
	// redirect specific libraries
	if(hook_libdl(soname) < 0) {
		ga_error("hook-pulse: dlsym hook failed.\n");
		exit(-1);
	} else {
		ga_error("hook-pulse: dlsym hook for %s successfully\n", soname);
	}
	//
quit:
#endif
	return;
}

__attribute__((constructor))
static void
pulse_hook_loaded(void) {
	ga_error("ga-hook-pulse loaded!\n");
	pulse_hook_symbols();
	return;
}

static int PA_bytes_per_sample(pa_sample_format_t format) {
	switch(format) {
	case PA_SAMPLE_U8:
	case PA_SAMPLE_ALAW:
	case PA_SAMPLE_ULAW:
		return 1;
	case PA_SAMPLE_S16LE:
	case PA_SAMPLE_S16BE:
		return 2;
	case PA_SAMPLE_S24LE:
	case PA_SAMPLE_S24BE:
		return 3;
	case PA_SAMPLE_FLOAT32LE:
	case PA_SAMPLE_FLOAT32BE:
	case PA_SAMPLE_S32LE:
	case PA_SAMPLE_S32BE:
	case PA_SAMPLE_S24_32LE:
	case PA_SAMPLE_S24_32BE:
		return 4;
	default:
		return -1;
	}
	return -1;
}

static enum AVSampleFormat
PA2SWR_format(pa_sample_format_t format) {
	switch(format) {
	case PA_SAMPLE_U8:
		return AV_SAMPLE_FMT_U8;
	case PA_SAMPLE_S16LE:
	case PA_SAMPLE_S16BE:
		return AV_SAMPLE_FMT_S16;
	default:
		ga_error("PA2SWR: format %x is not supported.\n", format);
		exit(-1);
	}
	return AV_SAMPLE_FMT_NONE;
}

static int64_t
PA2SWR_chlayout(uint8_t channels) {
	if(channels == 1)
		return AV_CH_LAYOUT_MONO;
	if(channels == 2)
		return AV_CH_LAYOUT_STEREO;
	ga_error("PA2SWR: channel layout (%d) is not supported.\n", channels);
	exit(-1);
	return -1;
}

static int
pa_create_swrctx(pa_sample_format_t format, int freq, int channels) {
	struct RTSPConf *rtspconf = rtspconf_global();
	int bufreq, samples;
	//
	if(swrctx != NULL)
		swr_free(&swrctx);
	if(audio_buf != NULL)
		free(audio_buf);
	//
	ga_error("PulseAudio: create swr context - format[%d] freq[%d] channels[%d]\n",
		format, freq, channels);
	//
	swrctx = swr_alloc_set_opts(NULL,
		rtspconf->audio_device_channel_layout,
		rtspconf->audio_device_format,
		rtspconf->audio_samplerate,
		PA2SWR_chlayout(channels),
		PA2SWR_format(format),
		freq, 0, NULL);
	if(swrctx == NULL) {
		ga_error("PulseAudio: cannot create resample context.\n");
		return -1;
	} else {
		ga_error("PulseAudio: resample context (%x,%d,%d) -> (%x,%d,%d)\n",
			(int) PA2SWR_chlayout(channels),
			(int) PA2SWR_format(format),
			(int) freq,
			(int) rtspconf->audio_device_channel_layout,
			(int) rtspconf->audio_device_format,
			(int) rtspconf->audio_samplerate);
	}
	if(swr_init(swrctx) < 0) {
		swr_free(&swrctx);
		swrctx = NULL;
		ga_error("PulseAudio: resample context init failed.\n");
		return -1;
	}
	// allocate buffer?
	ga_samplerate = rtspconf->audio_samplerate;
	ga_channels = av_get_channel_layout_nb_channels(rtspconf->audio_device_channel_layout);
	pa_samplerate = freq;
	pa_bytes_per_sample = PA_bytes_per_sample(format);
	samples = av_rescale_rnd(PA_MAX_SAMPLES,
			rtspconf->audio_samplerate, freq, AV_ROUND_UP);
	bufreq = av_samples_get_buffer_size(NULL,
			rtspconf->audio_channels, samples*2,
			rtspconf->audio_device_format,
			1/*no-alignment*/);
	if((audio_buf = (unsigned char *) malloc(bufreq)) == NULL) {
		ga_error("PulseAudio: cannot allocate resample memory.\n");
		return -1;
	}
	if(audio_source_setup(bufreq, rtspconf->audio_samplerate,
				16/* depends on format */,
				rtspconf->audio_channels) < 0) {
		ga_error("PulseAudio: audio source setup failed.\n");
		return -1;
	}
	ga_error("PulseAudio: max %d samples with %d byte(s) resample buffer allocated.\n",
		samples, bufreq);
	//
	return 0;
}

pa_simple*
hook_pa_simple_new(const char * server,
		const char * name,
		pa_stream_direction_t dir,
		const char * dev,
		const char * stream_name,
		const pa_sample_spec * ss,
		const pa_channel_map * map,
		const pa_buffer_attr * attr,
		int * error) {
	pa_simple *ret;
	if(old_pa_simple_new == NULL)
		pulse_hook_symbols();
	ga_error("pa_simple_new: server[%s] name[%s] dev[%s] stream_name[%s]\n",
			server, name, dev, stream_name);
	ret = old_pa_simple_new(server, name, dir, dev, stream_name, ss, map, attr, error);
	if(pa_simple_main == NULL && pa_stream_main == NULL) {
		pa_simple_main = ret;
		pa_create_swrctx(ss->format, ss->rate, ss->channels);
	}
	return ret;
}

void
hook_pa_simple_free(pa_simple * s) {
	if(old_pa_simple_free == NULL)
		pulse_hook_symbols();
	ga_error("pa_simple_free\n");
	old_pa_simple_free(s);
	if(pa_simple_main == s) {
		pa_simple_main = NULL;
	}
	return;
}

int
hook_pa_simple_write(pa_simple * s,
		const void * data,
		size_t bytes,
		int * error) {
	if(old_pa_simple_write == NULL)
		pulse_hook_symbols();
	if(pa_simple_main == s && pa_stream_main == NULL) {
		ga_error("pa_simple_write: %d bytes\n", bytes);
	}
	return old_pa_simple_write(s, data, bytes, error);
}

// async

pa_stream*
hook_pa_stream_new(pa_context * c,
		const char * name,
		const pa_sample_spec * ss,
		const pa_channel_map * map) {
	pa_stream *ret;
	if(old_pa_stream_new == NULL)
		pulse_hook_symbols();
	ga_error("pa_stream_new: name = %s\n", name);
	ret = old_pa_stream_new(c, name, ss, map);
	if(ret != NULL) {
		paSpecs[ret] = *ss;
	}
	return ret;
}

pa_stream*
hook_pa_stream_new_extended(pa_context * c,
		const char * name,
		pa_format_info *const * formats,
		unsigned int n_formats,
		pa_proplist * p) {
	// XXX: do nothing now
	if(old_pa_stream_new_extended == NULL)
		pulse_hook_symbols();
	ga_error("ps_stream_new_extended: name = %s\n", name);
	return old_pa_stream_new_extended(c, name, formats, n_formats, p);
}

pa_stream*
hook_pa_stream_new_with_proplist(pa_context * c,
		const char * name,
		const pa_sample_spec * ss,
		const pa_channel_map * map,
		pa_proplist * p) {
	pa_stream *ret;
	if(old_pa_stream_new_with_proplist == NULL)
		pulse_hook_symbols();
	ga_error("pa_stream_new_with_proplist: name = %s\n", name);
	ret = old_pa_stream_new_with_proplist(c, name, ss, map, p);
	if(ret != NULL) {
		paSpecs[ret] = *ss;
	}
	return ret;
}

int
hook_pa_stream_connect_playback(pa_stream * s,
		const char * dev,
		const pa_buffer_attr * attr,
		pa_stream_flags_t flags,
		const pa_cvolume * volume,
		pa_stream * sync_stream) {
	int ret;
	if(old_pa_stream_connect_playback == NULL)
		pulse_hook_symbols();
	ga_error("pa_stream_connect_playback: stream[%p] dev[%s]\n", s, dev);
	ret = old_pa_stream_connect_playback(s, dev, attr, flags, volume, sync_stream);
	if(ret >= 0) {
		map<pa_stream*, pa_sample_spec>::iterator mi;
		pa_stream_main = s;
		if((mi = paSpecs.find(s)) != paSpecs.end()) {
			pa_create_swrctx(
				mi->second.format,
				mi->second.rate,
				mi->second.channels);
		}
	}
	return ret;
}

void
hook_pa_stream_set_write_callback(pa_stream * p,
		pa_stream_request_cb_t cb,
		void * userdata) {
	if(old_pa_stream_set_write_callback == NULL)
		pulse_hook_symbols();
	ga_error("pa_stream_set_write_callback: stream[%p] cb[%p] userdata[%p]\n",
			p, cb, userdata);
	old_pa_stream_set_write_callback(p, cb, userdata);
	return;
}

int
hook_pa_stream_write(pa_stream * p,
		const void * data,
		size_t nbytes,
		pa_free_cb_t free_cb,
		int64_t offset,
		pa_seek_mode_t seek) {
	//
	int srcsamples, dstsamples;
	const unsigned char *srcplanes[SWR_CH_MAX];
	unsigned char *dstplanes[SWR_CH_MAX];
	//
	if(old_pa_stream_write == NULL)
		pulse_hook_symbols();
	//
	if(p == pa_stream_main
	&& nbytes > 0 && offset == 0 && seek == PA_SEEK_RELATIVE) do {
		//ga_error("pa_stream_write: %d bytes (offset %lld)\n", nbytes, offset);
		if(swrctx == NULL || audio_buf == NULL)
			break;
		srcplanes[0] = (const unsigned char *) data;
		srcplanes[1] = NULL;
		dstplanes[0] = audio_buf;
		dstplanes[1] = NULL;
		srcsamples = nbytes / pa_bytes_per_sample;
		dstsamples = av_rescale_rnd(srcsamples,
				ga_samplerate, pa_samplerate, AV_ROUND_UP);
		swr_convert(swrctx,
			dstplanes, dstsamples,
			srcplanes, srcsamples);
		audio_source_buffer_fill(audio_buf, dstsamples/ga_channels);
		//
		bzero((void *) data, nbytes);
	} while(0);
	//
	return old_pa_stream_write(p, data, nbytes, free_cb, offset, seek);
}

#ifndef WIN32	/* POSIX interfaces */
pa_simple*
pa_simple_new(const char * server,
		const char * name,
		pa_stream_direction_t dir,
		const char * dev,
		const char * stream_name,
		const pa_sample_spec * ss,
		const pa_channel_map * map,
		const pa_buffer_attr * attr,
		int * error) {
	return hook_pa_simple_new(server, name, dir, dev, name, ss, map, attr, error);
}

void
pa_simple_free(pa_simple * s) {
	hook_pa_simple_free(s);
	return;
}

int
pa_simple_write(pa_simple * s,
		const void * data,
		size_t bytes,
		int * error) {
	return hook_pa_simple_write(s, data, bytes, error);
}

pa_stream*
pa_stream_new(pa_context * c,
		const char * name,
		const pa_sample_spec * ss,
		const pa_channel_map * map) {
	return hook_pa_stream_new(c, name, ss, map);
}

pa_stream*
pa_stream_new_extended(pa_context * c,
		const char * name,
		pa_format_info *const * formats,
		unsigned int n_formats,
		pa_proplist * p) {
	return hook_pa_stream_new_extended(c, name, formats, n_formats, p);
}

pa_stream*
pa_stream_new_with_proplist(pa_context * c,
		const char * name,
		const pa_sample_spec * ss,
		const pa_channel_map * map,
		pa_proplist * p) {
	return hook_pa_stream_new_with_proplist(c, name, ss, map, p);
}

int
pa_stream_connect_playback(pa_stream * s,
		const char * dev,
		const pa_buffer_attr * attr,
		pa_stream_flags_t flags,
		const pa_cvolume * volume,
		pa_stream * sync_stream) {
	return hook_pa_stream_connect_playback(s, dev, attr, flags, volume, sync_stream);
}

void
pa_stream_set_write_callback(pa_stream * p,
		pa_stream_request_cb_t cb,
		void * userdata) {
	hook_pa_stream_set_write_callback(p, cb, userdata);
	return;
}

int
pa_stream_write(pa_stream * p,
		const void * data,
		size_t nbytes,
		pa_free_cb_t free_cb,
		int64_t offset,
		pa_seek_mode_t seek) {
	return hook_pa_stream_write(p, data, nbytes, free_cb, offset, seek);
}

#endif

