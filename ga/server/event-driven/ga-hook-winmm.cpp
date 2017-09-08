/*
 * Copyright (c) 2012-2015 Chun-Ying Huang
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
#include <map>

#include "ga-common.h"
#include "ga-avcodec.h"
#include "rtspconf.h"
#include "asource.h"
#include "ga-hook-common.h"
#include "ga-hook-winmm.h"

using namespace std;

#define CA_MAX_SAMPLES	32768

static int winmm_bytes_per_sample = 0;
static int winmm_samplerate = 0;
static int winmm_samplesize = 0;
static int ga_samplerate = 0;
static int ga_channels = 0;
static struct SwrContext *swrctx = NULL;
static unsigned char *audio_buf = NULL;

static t_waveOutOpen		old_waveOutOpen = NULL;
static t_waveOutWrite		old_waveOutWrite = NULL;
static t_waveOutClose		old_waveOutClose = NULL;

static map<HWAVEOUT,HWAVEOUT>	winmmHandle;

static enum AVSampleFormat
WINMM2SWR_format(WAVEFORMATEX *w) {
	WAVEFORMATEXTENSIBLE *wex = (WAVEFORMATEXTENSIBLE*) w;
	switch(w->wFormatTag) {
	case WAVE_FORMAT_PCM:
pcm:
		winmm_samplesize = w->wBitsPerSample>>3;
		if(w->wBitsPerSample == 8)
			return AV_SAMPLE_FMT_U8;
		if(w->wBitsPerSample == 16)
			return AV_SAMPLE_FMT_S16;
		if(w->wBitsPerSample == 32)
			return AV_SAMPLE_FMT_S32;
		break;
	case WAVE_FORMAT_IEEE_FLOAT:
ieee_float:
		winmm_samplesize = w->wBitsPerSample>>3;
		if(w->wBitsPerSample == 32)
			return AV_SAMPLE_FMT_FLT;
		if(w->wBitsPerSample == 64)
			return AV_SAMPLE_FMT_DBL;
		break;
	case WAVE_FORMAT_EXTENSIBLE:
		if(wex->SubFormat == KSDATAFORMAT_SUBTYPE_PCM)
			goto pcm;
		if(wex->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
			goto ieee_float;
		ga_error("WINMM2SWR: format %08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX is not supported.\n",
			wex->SubFormat.Data1,
			wex->SubFormat.Data2,
			wex->SubFormat.Data3,
			wex->SubFormat.Data4[0],
			wex->SubFormat.Data4[1],
			wex->SubFormat.Data4[2],
			wex->SubFormat.Data4[3],
			wex->SubFormat.Data4[4],
			wex->SubFormat.Data4[5],
			wex->SubFormat.Data4[6],
			wex->SubFormat.Data4[7]);
		exit(-1);
		break;
	default:
		ga_error("WINMM2SWR: format %x is not supported.\n", w->wFormatTag);
		exit(-1);
	}
	return AV_SAMPLE_FMT_NONE;
}

static int64_t
WINMM2SWR_chlayout(int channels) {
	if(channels == 1)
		return AV_CH_LAYOUT_MONO;
	if(channels == 2)
		return AV_CH_LAYOUT_STEREO;
	ga_error("WINMM2SWR: channel layout (%d) is not supported.\n", channels);
	exit(-1);
	return -1;
}

static int
winmm_create_swrctx(WAVEFORMATEX *w) {
	struct RTSPConf *rtspconf = rtspconf_global();
	int bufreq, samples;
	//
	if(swrctx != NULL)
		swr_free(&swrctx);
	if(audio_buf != NULL)
		free(audio_buf);
	//
	ga_error("hook_winmm: create swr context - format[%x] freq[%d] channels[%d]\n",
		w->wFormatTag, w->nSamplesPerSec, w->nChannels);
	//
	swrctx = swr_alloc_set_opts(NULL,
		rtspconf->audio_device_channel_layout,
		rtspconf->audio_device_format,
		rtspconf->audio_samplerate,
		WINMM2SWR_chlayout(w->nChannels),
		WINMM2SWR_format(w),
		w->nSamplesPerSec, 0, NULL);
	if(swrctx == NULL) {
		ga_error("hook_winmm: cannot create resample context.\n");
		return -1;
	} else {
		ga_error("hook_winmm: resample context (%x,%d,%d) -> (%x,%d,%d)\n",
			(int) WINMM2SWR_chlayout(w->nChannels),
			(int) WINMM2SWR_format(w),
			(int) w->nSamplesPerSec,
			(int) rtspconf->audio_device_channel_layout,
			(int) rtspconf->audio_device_format,
			(int) rtspconf->audio_samplerate);
	}
	if(swr_init(swrctx) < 0) {
		swr_free(&swrctx);
		swrctx = NULL;
		ga_error("hook_winmm: resample context init failed.\n");
		return -1;
	}
	// allocate buffer?
	ga_samplerate = rtspconf->audio_samplerate;
	ga_channels = av_get_channel_layout_nb_channels(rtspconf->audio_device_channel_layout);
	winmm_samplerate = w->nSamplesPerSec;
	winmm_bytes_per_sample = w->wBitsPerSample/8;
	samples = av_rescale_rnd(CA_MAX_SAMPLES,
			rtspconf->audio_samplerate, w->nSamplesPerSec, AV_ROUND_UP);
	bufreq = av_samples_get_buffer_size(NULL,
			rtspconf->audio_channels, samples*2,
			rtspconf->audio_device_format,
			1/*no-alignment*/);
	if((audio_buf = (unsigned char *) malloc(bufreq)) == NULL) {
		ga_error("hook_winmm: cannot allocate resample memory.\n");
		return -1;
	}
	if(audio_source_setup(bufreq, rtspconf->audio_samplerate,
				16/* depends on format */,
				rtspconf->audio_channels) < 0) {
		ga_error("hook_winmm: audio source setup failed.\n");
		return -1;
	}
	ga_error("hook_winmm: max %d samples with %d byte(s) resample buffer allocated.\n",
		samples, bufreq);
	//
	return 0;
}

MMRESULT
hook_waveOutOpen(
		LPHWAVEOUT phwo,
		UINT_PTR uDeviceID,
		LPWAVEFORMATEX pwfx,
		DWORD_PTR dwCallback,
		DWORD_PTR dwCallbackInstance,
		DWORD fdwOpen) {
	// do open
	MMRESULT ret = old_waveOutOpen(phwo, uDeviceID, pwfx, 
				dwCallback, dwCallbackInstance, fdwOpen);
	// query only?
	if(fdwOpen & WAVE_FORMAT_QUERY) {
		return ret;
	}
	// has error?
	if(ret != MMSYSERR_NOERROR)
		return ret;
	// save the format
	if(winmm_create_swrctx(pwfx) < 0) {
		ga_error("hook_winmm: requested an unsupported format on opening\n");
		old_waveOutClose(*phwo);
		return WAVERR_BADFORMAT;
	}
	// save the handle
	winmmHandle[*phwo] = *phwo;
	//
	return ret;
}

MMRESULT
hook_waveOutWrite(HWAVEOUT hwo, LPWAVEHDR pwh, UINT cbwh) {
	const unsigned char *srcplanes[SWR_CH_MAX];
	unsigned char *dstplanes[SWR_CH_MAX];
	int samples, pwh_samples;
	// capture audio here
	if(swrctx == NULL || audio_buf == NULL)
		goto no_capture;
	if(pwh->dwFlags & (WHDR_BEGINLOOP|WHDR_ENDLOOP))
		ga_error("hook_winmm: LOOP is not supported.\n");
	srcplanes[0] = (unsigned char*) pwh->lpData;
	srcplanes[1] = NULL;
	dstplanes[0] = audio_buf;
	dstplanes[1] = NULL;
	pwh_samples = pwh->dwBufferLength / winmm_samplesize;
	samples = av_rescale_rnd(pwh_samples, ga_samplerate,
			winmm_samplerate, AV_ROUND_UP);
	swr_convert(swrctx,
			dstplanes, samples,
			srcplanes, pwh_samples);
	audio_source_buffer_fill(audio_buf, samples);
	bzero(pwh->lpData, pwh->dwBufferLength);
	//
no_capture:
	return old_waveOutWrite(hwo, pwh, cbwh);
}

MMRESULT
hook_waveOutClose(HWAVEOUT hwo) {
	winmmHandle.erase(hwo);
	return old_waveOutClose(hwo);
}

int
hook_winmm() {
	HMODULE hMod;

	if((hMod = GetModuleHandle("WINMM.DLL")) == NULL) {
		if((hMod = LoadLibrary("WINMM.DLL")) == NULL) {
			ga_error("hook_winmm: unable to load winmm.dll.\n");
			return -1;
		} else {
			ga_error("hook_winmm: winmm.dll loaded.\n");
		}
	}
	//
#define	load_hook_function(mod, type, ptr, func)	\
		if((ptr = (type) GetProcAddress(mod, func)) == NULL) { \
			ga_error("hook_winmm: GetProcAddress(%s) failed.\n", func); \
			return -1; \
		}
	load_hook_function(hMod, t_waveOutOpen, old_waveOutOpen, "waveOutOpen");
	load_hook_function(hMod, t_waveOutWrite, old_waveOutWrite, "waveOutWrite");
	load_hook_function(hMod, t_waveOutClose, old_waveOutClose, "waveOutClose");
#undef	load_hook_function

#define	WINMM_DO_HOOK(name)	ga_hook_function(#name, old_##name, hook_##name)
	WINMM_DO_HOOK(waveOutOpen);
	WINMM_DO_HOOK(waveOutWrite);
	WINMM_DO_HOOK(waveOutClose);
#undef	WINMM_DO_HOOK

	ga_error("hook_winmm: done\n");

	return 0;
}

