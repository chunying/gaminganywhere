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
#include "vsource.h"
#include "asource.h"
#include "rtspconf.h"

#include "ga-hook-common.h"
#include "ga-hook-sdl2audio.h"
#ifndef WIN32
#include "ga-hook-lib.h"
#endif

#include <map>
using namespace std;

#ifndef WIN32
#ifdef __cplusplus
extern "C" {
#endif
int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained);
void SDL_PauseAudio(int pause_on);
void SDL_CloseAudio();
#ifdef __cplusplus
}
#endif
#endif

// for hooking
t_SDL2_OpenAudio	old_SDL2_OpenAudio = NULL;
t_SDL2_PauseAudio	old_SDL2_PauseAudio = NULL;
t_SDL2_CloseAudio	old_SDL2_CloseAudio = NULL;

static void
sdlaudio_hook_symbols() {
#ifndef WIN32
	void *handle = NULL;
	char *ptr, soname[2048];
	if((ptr = getenv("LIBSDL_SO")) == NULL) {
		strncpy(soname, "libSDL-1.2.so.0", sizeof(soname));
	} else {
		strncpy(soname, ptr, sizeof(soname));
	}
	if((handle = dlopen(soname, RTLD_LAZY)) == NULL) {
		ga_error("hook-sdlaudio: '%s' failed: %s\n", soname, strerror(errno));
		exit(-1);
	}
	// for hooking
	old_SDL2_OpenAudio = (t_SDL2_OpenAudio)
				ga_hook_lookup_or_quit(handle, "SDL_OpenAudio");
	old_SDL2_PauseAudio = (t_SDL2_PauseAudio)
				ga_hook_lookup_or_quit(handle, "SDL_PauseAudio");
	old_SDL2_CloseAudio = (t_SDL2_CloseAudio)
				ga_hook_lookup_or_quit(handle, "SDL_CloseAudio");
	if((ptr = getenv("HOOKAUDIO")) == NULL)
		goto quit;
	strncpy(soname, ptr, sizeof(soname));
	// hook indirectly
	if((handle = dlopen(soname, RTLD_LAZY)) != NULL) {
	//////////////////////////////////////////////////
	hook_lib_generic(soname, handle, "SDL_OpenAudio", (void*) hook_SDL2_OpenAudio);
	hook_lib_generic(soname, handle, "SDL_PauseAudio", (void*) hook_SDL2_PauseAudio);
	hook_lib_generic(soname, handle, "SDL_CloseAudio", (void*) hook_SDL2_CloseAudio);
	//
	ga_error("hook-sdlaudio: hooked into %s\n", soname);
	}
quit:
#endif
	return;
}

static void
sdlaudio_dump_audiospec(SDL_AudioSpec *spec) {
	ga_error("SDL_OpenAudio: freq=%d format=%x channels=%d silence=%d samples=%d padding=%d size=%d callback=%p userdata=%p\n",
		spec->freq, spec->format, spec->channels, spec->silence,
		spec->samples, spec->padding, spec->size,
		spec->callback, spec->userdata);
	return;
}

static struct SwrContext *swrctx = NULL;
static unsigned char *audio_buf = NULL;
static struct SDL_AudioSpec audio_spec;
static int audio_buf_samples = 0;

static void (*old_audio_callback)(void *, uint8_t *, int) = NULL;
static void
hook_SDL2_audio_callback(void *userdata, uint8_t *stream, int len) {
	const unsigned char *srcplanes[SWR_CH_MAX];
	unsigned char *dstplanes[SWR_CH_MAX];
#if 0
	ga_error("audio-callback: userdata=%p stream=%p len=%d\n",
			userdata, stream, len);
#endif
	if(old_audio_callback != NULL)
		old_audio_callback(userdata, stream, len);
	// pipe to the encoder
	if(swrctx == NULL || audio_buf == NULL)
		goto quit;
	srcplanes[0] = stream;
	srcplanes[1] = NULL;
	dstplanes[0] = audio_buf;
	dstplanes[1] = NULL;
	swr_convert(swrctx,
			dstplanes, audio_buf_samples,
			srcplanes, audio_spec.samples);
	audio_source_buffer_fill(audio_buf, audio_buf_samples);
quit:
	// silence local outputs
	bzero(stream, len);
	return;
}

static enum AVSampleFormat
SDL2SWR_format(SDL_AudioFormat format) {
	switch(format) {
	case AUDIO_U8:
		return AV_SAMPLE_FMT_U8;
	case AUDIO_S16:
		return AV_SAMPLE_FMT_S16;
	default:
		ga_error("SDL2SWR: format %x is not supported.\n", format);
		exit(-1);
	}
	return AV_SAMPLE_FMT_NONE;
}

static int64_t
SDL2SWR_chlayout(uint8_t channels) {
	if(channels == 1)
		return AV_CH_LAYOUT_MONO;
	if(channels == 2)
		return AV_CH_LAYOUT_STEREO;
	ga_error("SDL2SWR: channel layout (%d) is not supported.\n", channels);
	exit(-1);
}

int
hook_SDL2_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained) {
	int ret;
	if(old_SDL2_OpenAudio == NULL) {
		sdlaudio_hook_symbols();
	}
	//
	sdlaudio_dump_audiospec(desired);
	if(desired->callback != NULL) {
		old_audio_callback = desired->callback;
		desired->callback = hook_SDL2_audio_callback;
	}
	//
	ret = old_SDL2_OpenAudio(desired, obtained);
	//
	if(ret >= 0) {
		struct RTSPConf *rtspconf = rtspconf_global();
		int bufreq = 0;
		//
		//obtained->callback = old_audio_callback;
		if(obtained == NULL)
			obtained = desired;
		// release everything
		if(swrctx != NULL)
			swr_free(&swrctx);
		if(audio_buf != NULL)
			free(audio_buf);
		// create resample context
		swrctx = swr_alloc_set_opts(NULL,
				rtspconf->audio_device_channel_layout,
				rtspconf->audio_device_format,
				rtspconf->audio_samplerate,
				SDL2SWR_chlayout(obtained->channels),
				SDL2SWR_format(obtained->format),
				obtained->freq,
				0, NULL);
		if(swrctx == NULL) {
			ga_error("SDL_OpenAudio: cannot create resample context.\n");
			exit(-1);
		} else {
			ga_error("SDL_OpenAudio: resample context (%x,%d,%d) -> (%x,%d,%d)\n",
				(int) SDL2SWR_chlayout(obtained->channels),
				(int) SDL2SWR_format(obtained->format),
				(int) obtained->freq,
				(int) rtspconf->audio_device_channel_layout,
				(int) rtspconf->audio_device_format,
				(int) rtspconf->audio_samplerate);
		}
		if(swr_init(swrctx) < 0) {
			ga_error("SDL_OpenAudio: resample context init failed.\n");
			exit(-1);
		}
		// allocate resample buffer
		audio_buf_samples = av_rescale_rnd(obtained->samples,
					rtspconf->audio_samplerate,
					obtained->freq, AV_ROUND_UP);
		bufreq = av_samples_get_buffer_size(NULL,
				rtspconf->audio_channels,
				audio_buf_samples*2,
				rtspconf->audio_device_format,
				1/*no-alignment*/);
		if((audio_buf = (unsigned char*) malloc(bufreq)) == NULL) {
			ga_error("SDL_OpenAudio: cannot allocate resample memory.\n");
			exit(-1);
		}
		ga_error("SDL_OpenAudio: %d samples with %d byte(s) resample buffer allocated.\n",
				audio_buf_samples, bufreq);
		// setup GA audio source
		if(audio_source_setup(bufreq, rtspconf->audio_samplerate,
				/* XXX: We support only U8 and S16 now */
				obtained->format == AUDIO_S16 ? 16 : 8,
				rtspconf->audio_channels) < 0) {
			ga_error("SDL_OpenAudio: audio source setup failed.\n");
			exit(-1);
		}
		ga_error("SDL_OpenAudio: audio source configured: %d, %d, %d, %d.\n",
			bufreq, rtspconf->audio_samplerate,
			obtained->format == AUDIO_S16 ? 16 : 8,
			rtspconf->audio_channels);
		//
		bcopy(obtained, &audio_spec, sizeof(audio_spec));
	} else {
		ga_error("SDL_OpenAudio: returned %d\n", ret);
	}
	return ret;
}

void
hook_SDL2_PauseAudio(int pause_on) {
	if(old_SDL2_PauseAudio == NULL) {
		sdlaudio_hook_symbols();
	}
	ga_error("SDL_PauseAudio: pause_on = %d\n", pause_on);
	return old_SDL2_PauseAudio(pause_on);
}

void
hook_SDL2_CloseAudio() {
	if(old_SDL2_CloseAudio == NULL) {
		sdlaudio_hook_symbols();
	}
	ga_error("SDL_CloseAudio\n");
	return old_SDL2_CloseAudio();
}

#ifndef WIN32	/* POSIX interfaces */
int
SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained) {
	return hook_SDL2_OpenAudio(desired, obtained);
}

void
SDL_PauseAudio(int pause_on) {
	hook_SDL2_PauseAudio(pause_on);
	return;
}

void
SDL_CloseAudio() {
	hook_SDL2_CloseAudio();
	return;
}

__attribute__((constructor))
static void
sdlaudio_hook_loaded(void) {
	ga_error("hook-sdlaudio loaded!\n");
	sdlaudio_hook_symbols();
	return;
}
#endif	/* ! WIN32 */

