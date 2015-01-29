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

#include "ga-common.h"
#include "ga-avcodec.h"
#include "rtspconf.h"
#include "asource.h"
#include "ga-hook-common.h"
#include "ga-hook-coreaudio.h"

#define CA_MAX_SAMPLES	32768

static int ca_bytes_per_sample = 0;
static int ca_samplerate = 0;
static int ga_samplerate = 0;
static int ga_channels = 0;
static struct SwrContext *swrctx = NULL;
static unsigned char *audio_buf = NULL;

static t_GetBuffer		old_GetBuffer = NULL;
static t_ReleaseBuffer		old_ReleaseBuffer = NULL;
static t_GetMixFormat		old_GetMixFormat = NULL;

#define	CA_DO_HOOK(name)	ga_hook_function(#name, old_##name, hook_##name)

static enum AVSampleFormat
CA2SWR_format(WAVEFORMATEX *w) {
	WAVEFORMATEXTENSIBLE *wex = (WAVEFORMATEXTENSIBLE*) w;
	switch(w->wFormatTag) {
	case WAVE_FORMAT_PCM:
pcm:
		if(w->wBitsPerSample == 8)
			return AV_SAMPLE_FMT_U8;
		if(w->wBitsPerSample == 16)
			return AV_SAMPLE_FMT_S16;
		if(w->wBitsPerSample == 32)
			return AV_SAMPLE_FMT_S32;
		break;
	case WAVE_FORMAT_IEEE_FLOAT:
ieee_float:
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
		ga_error("CA2SWR: format %08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX is not supported.\n",
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
		ga_error("CA2SWR: format %x is not supported.\n", w->wFormatTag);
		exit(-1);
	}
	return AV_SAMPLE_FMT_NONE;
}

static int64_t
CA2SWR_chlayout(int channels) {
	if(channels == 1)
		return AV_CH_LAYOUT_MONO;
	if(channels == 2)
		return AV_CH_LAYOUT_STEREO;
	ga_error("CA2SWR: channel layout (%d) is not supported.\n", channels);
	exit(-1);
	return -1;
}

static int
ca_create_swrctx(WAVEFORMATEX *w) {
	struct RTSPConf *rtspconf = rtspconf_global();
	int bufreq, samples;
	//
	if(swrctx != NULL)
		swr_free(&swrctx);
	if(audio_buf != NULL)
		free(audio_buf);
	//
	ga_error("CoreAudio: create swr context - format[%x] freq[%d] channels[%d]\n",
		w->wFormatTag, w->nSamplesPerSec, w->nChannels);
	//
	swrctx = swr_alloc_set_opts(NULL,
		rtspconf->audio_device_channel_layout,
		rtspconf->audio_device_format,
		rtspconf->audio_samplerate,
		CA2SWR_chlayout(w->nChannels),
		CA2SWR_format(w),
		w->nSamplesPerSec, 0, NULL);
	if(swrctx == NULL) {
		ga_error("CoreAudio: cannot create resample context.\n");
		return -1;
	} else {
		ga_error("CoreAudio: resample context (%x,%d,%d) -> (%x,%d,%d)\n",
			(int) CA2SWR_chlayout(w->nChannels),
			(int) CA2SWR_format(w),
			(int) w->nSamplesPerSec,
			(int) rtspconf->audio_device_channel_layout,
			(int) rtspconf->audio_device_format,
			(int) rtspconf->audio_samplerate);
	}
	if(swr_init(swrctx) < 0) {
		swr_free(&swrctx);
		swrctx = NULL;
		ga_error("CoreAudio: resample context init failed.\n");
		return -1;
	}
	// allocate buffer?
	ga_samplerate = rtspconf->audio_samplerate;
	ga_channels = av_get_channel_layout_nb_channels(rtspconf->audio_device_channel_layout);
	ca_samplerate = w->nSamplesPerSec;
	ca_bytes_per_sample = w->wBitsPerSample/8;
	samples = av_rescale_rnd(CA_MAX_SAMPLES,
			rtspconf->audio_samplerate, w->nSamplesPerSec, AV_ROUND_UP);
	bufreq = av_samples_get_buffer_size(NULL,
			rtspconf->audio_channels, samples*2,
			rtspconf->audio_device_format,
			1/*no-alignment*/);
	if((audio_buf = (unsigned char *) malloc(bufreq)) == NULL) {
		ga_error("CoreAudio: cannot allocate resample memory.\n");
		return -1;
	}
	if(audio_source_setup(bufreq, rtspconf->audio_samplerate,
				16/* depends on format */,
				rtspconf->audio_channels) < 0) {
		ga_error("CoreAudio: audio source setup failed.\n");
		return -1;
	}
	ga_error("CoreAudio: max %d samples with %d byte(s) resample buffer allocated.\n",
		samples, bufreq);
	//
	return 0;
}

DllExport HRESULT __stdcall
hook_GetMixFormat( 
		IAudioClient *thiz,
		WAVEFORMATEX **ppDeviceFormat)
{
	HRESULT hr;
	hr = old_GetMixFormat(thiz, ppDeviceFormat);
	// init audio here
	if(hr != S_OK)
		return hr;
	if(ca_create_swrctx(*ppDeviceFormat) < 0) {
		ga_error("CoreAudio: GetMixFormat returns an unsupported format\n");
		exit(-1);
		return E_INVALIDARG;
	}
	//
	return hr;
}

static char *buffer_data = NULL;
static unsigned int buffer_frames = 0;

DllExport HRESULT __stdcall
hook_ReleaseBuffer( 
		IAudioRenderClient *thiz,
		UINT32 NumFramesWritten,
		DWORD dwFlags)
{
	const unsigned char *srcplanes[SWR_CH_MAX];
	unsigned char *dstplanes[SWR_CH_MAX];
	int samples;
	// capture audio here
	if(swrctx == NULL || buffer_data == NULL || audio_buf == NULL)
		goto no_capture;
	srcplanes[0] = (unsigned char*) buffer_data;
	srcplanes[1] = NULL;
	dstplanes[0] = audio_buf;
	dstplanes[1] = NULL;
	samples = av_rescale_rnd(NumFramesWritten,
			ga_samplerate,
			ca_samplerate, AV_ROUND_UP);
	swr_convert(swrctx,
			dstplanes, samples,
			srcplanes, NumFramesWritten);
	audio_source_buffer_fill(audio_buf, samples);
	bzero(buffer_data, NumFramesWritten * ca_bytes_per_sample);
	dwFlags |= AUDCLNT_BUFFERFLAGS_SILENT;
	//
no_capture:
	buffer_data = NULL;
	return old_ReleaseBuffer(thiz, NumFramesWritten, dwFlags);
}


DllExport HRESULT __stdcall
hook_GetBuffer( 
		IAudioRenderClient *thiz,
		UINT32 NumFramesRequested,
		BYTE **ppData)
{	
	HRESULT hr;
	hr = old_GetBuffer(thiz, NumFramesRequested, ppData);
	if(hr == S_OK) {
		buffer_data = (char*) *ppData;
		buffer_frames = NumFramesRequested;
	}
	return S_OK;
}


int
hook_coreaudio() {
	int ret = -1;

	HRESULT hr;
	IMMDeviceEnumerator *deviceEnumerator = NULL;
	IMMDevice *device = NULL;
	IAudioClient *audioClient = NULL;
	IAudioRenderClient *renderClient = NULL;
	WAVEFORMATEX *pwfx = NULL;

	// obtain core-audio objects and functions
#define	RET_ON_ERROR(hr, prefix) if(hr!=S_OK) { ga_error("[core-audio] %s failed (%08x).\n", prefix, hr); goto hook_ca_quit; }
	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**) &deviceEnumerator);
	RET_ON_ERROR(hr, "CoCreateInstance");

	hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
	RET_ON_ERROR(hr, "GetDefaultAudioEndpoint");

	hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**) &audioClient);
	RET_ON_ERROR(hr, "Activate");

	hr = audioClient->GetMixFormat(&pwfx);
	RET_ON_ERROR(hr, "GetMixFormat");

	hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000/*REFTIME_PER_SEC*/, 0, pwfx, NULL);
	RET_ON_ERROR(hr, "Initialize");

	hr = audioClient->GetService(__uuidof(IAudioRenderClient), (void**) &renderClient);
	RET_ON_ERROR(hr, "GetService[IAudioRenderClient]");
#undef	RET_ON_ERROR

	// do hook stuff
	old_GetMixFormat = (t_GetMixFormat) ((comobj_t*) audioClient)->vtbl->func[8];
	CA_DO_HOOK(GetMixFormat);

	old_GetBuffer = (t_GetBuffer) ((comobj_t*) renderClient)->vtbl->func[3];
	CA_DO_HOOK(GetBuffer);

	old_ReleaseBuffer = (t_ReleaseBuffer) ((comobj_t*) renderClient)->vtbl->func[4];
	CA_DO_HOOK(ReleaseBuffer);

	ret = 0;

	ga_error("hook_coreaudio: done\n");

hook_ca_quit:
	if(renderClient)	{ renderClient->Release();	renderClient = NULL;		}
	if(pwfx)		{ CoTaskMemFree(pwfx);		pwfx= NULL;			}
	if(audioClient)		{ audioClient->Release();	audioClient = NULL;		}
	if(device)		{ device->Release();		device = NULL;			}
	if(deviceEnumerator)	{ deviceEnumerator->Release();	deviceEnumerator = NULL;	}

	return ret;
}

