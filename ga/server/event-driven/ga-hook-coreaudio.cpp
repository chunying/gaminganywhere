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
#include "ga-avcodec.h"
#include "rtspconf.h"
#include "asource.h"
#include "ga-hook-coreaudio.h"

#define CA_MAX_SAMPLES	32768

static int ca_bytes_per_sample = 0;
static int ca_samplerate = 0;
static int ga_samplerate = 0;
static int ga_channels = 0;
static struct SwrContext *swrctx = NULL;
static unsigned char *audio_buf = NULL;

static t_CoCreateInstance	old_CoCreateInstance = NULL;
static t_EnumAudioEndpoints	old_EnumAudioEndpoints = NULL;
static t_GetDefaultAudioEndpoint old_GetDefaultAudioEndpoint = NULL; /* member of IMMDevice */
static t_GetDevice		old_GetDevice = NULL;
static t_Activate		old_Activate = NULL;	/* member of IMMDevice */
static t_Item			old_Item = NULL;
static t_GetService		old_GetService = NULL;
static t_GetBuffer		old_GetBuffer = NULL;
static t_ReleaseBuffer		old_ReleaseBuffer = NULL;
static t_GetMixFormat		old_GetMixFormat = NULL;

#define	CA_DO_HOOK(name)	\
		DetourTransactionBegin(); \
		DetourUpdateThread(GetCurrentThread()); \
		DetourAttach(&(PVOID&)old_##name, hook_##name); \
		DetourTransactionCommit();

DllExport HRESULT __stdcall
hook_EnumAudioEndpoints( 
		IMMDeviceEnumerator *thiz,
		EDataFlow dataFlow,
		DWORD dwStateMask,
		IMMDeviceCollection **ppDevices)
{
	HRESULT hr;
	hr = old_EnumAudioEndpoints(thiz, dataFlow, dwStateMask, ppDevices);
	if(hr==S_OK && old_Item==NULL) {
		DWORD* pvtbl = (DWORD*) *(DWORD*) *ppDevices;
		old_Item = (t_Item) pvtbl[4];
		CA_DO_HOOK(Item);
	}
	return hr;
}

DllExport HRESULT __stdcall
hook_GetDefaultAudioEndpoint(
		IMMDeviceEnumerator *thiz,
		EDataFlow dataFlow,
		ERole role,
		IMMDevice **ppDevice)
{
	HRESULT hr;
	hr = old_GetDefaultAudioEndpoint(thiz, dataFlow, role, ppDevice);
	if(hr==S_OK && old_Activate==NULL) {
		DWORD* pvtbl = (DWORD*) *(DWORD*) *ppDevice;
		old_Activate = (t_Activate) pvtbl[3];
		CA_DO_HOOK(Activate);
	}
	return hr;
}

DllExport HRESULT __stdcall
hook_GetDevice(	IMMDeviceEnumerator *thiz,
		LPCWSTR pwstrId,
		IMMDevice **ppDevice)
{
	HRESULT hr;
	hr = old_GetDevice(thiz, pwstrId, ppDevice);
	if(hr==S_OK && old_Activate==NULL) {
		DWORD* pvtbl = (DWORD*) *(DWORD*) *ppDevice;
		old_Activate = (t_Activate) pvtbl[3];
		CA_DO_HOOK(Activate);
	}
	return hr;
}

DllExport HRESULT __stdcall
hook_Item(	IMMDeviceCollection *thiz,
		UINT nDevice,
		IMMDevice **ppDevice)
{
	HRESULT hr;
	hr = old_Item(thiz, nDevice, ppDevice);
	if(hr==S_OK && old_Activate==NULL) {
		DWORD* pvtbl = (DWORD*) *(DWORD*) *ppDevice;
		old_Activate = (t_Activate) pvtbl[3];
		CA_DO_HOOK(Activate);
	}
	return  hr;
}

DllExport HRESULT __stdcall
hook_Activate(
		IMMDeviceActivator *thiz,
		REFIID iid,
		DWORD dwClsCtx,
		PROPVARIANT *pActivationParams,
		void **ppInterface
		)
{ 
	const IID IID_IAudioClient = __uuidof(IAudioClient);
	HRESULT hr;
	//
	hr = old_Activate(thiz, iid, dwClsCtx, pActivationParams, ppInterface);
	if(hr==S_OK && iid==IID_IAudioClient) {
		if(old_GetService==NULL){
			DWORD* pvtbl = (DWORD*) *(DWORD*) *ppInterface;
			old_GetService = (t_GetService) pvtbl[14];
			CA_DO_HOOK(GetService);
		}
		if(old_GetMixFormat==NULL){
			DWORD* pvtbl = (DWORD*) *(DWORD*) *ppInterface;
			old_GetMixFormat= (t_GetMixFormat) pvtbl[8];
			CA_DO_HOOK(GetMixFormat);
		}
	}
	return hr;
}


DllExport HRESULT __stdcall
hook_GetService(
		IAudioClient *thiz,
		REFIID iid,
		void **ppv
	       )
{
	const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
	HRESULT hr;
	hr = old_GetService(thiz, iid, ppv);
	if(hr==S_OK && iid==IID_IAudioRenderClient) {
		if(old_ReleaseBuffer==NULL){
			DWORD* pvtbl = (DWORD*) *(DWORD*) *ppv;
			old_ReleaseBuffer = (t_ReleaseBuffer) pvtbl[4];
			CA_DO_HOOK(ReleaseBuffer);
		}
		if(old_GetBuffer==NULL){
			DWORD* pvtbl = (DWORD*) *(DWORD*) *ppv;
			old_GetBuffer = (t_GetBuffer) pvtbl[3];
			CA_DO_HOOK(GetBuffer);
		}
	}
	return hr;
}

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

DllExport HRESULT __stdcall
hook_CoCreateInstance(
		REFCLSID clsid,
		LPUNKNOWN punknown,
		DWORD dwClsContext,
		REFIID iid,
		LPVOID *ppv
	)
{	
	const IID IID_IMMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	HRESULT hr;

	hr = old_CoCreateInstance(clsid, punknown, dwClsContext, iid, ppv);
	
	if(hr != S_OK)
		return hr;

	if(clsid != IID_IMMDeviceEnumerator)
		return hr;

	if(old_EnumAudioEndpoints == NULL) {
		DWORD* pvtbl = (DWORD*) *(DWORD*) *ppv;
		old_EnumAudioEndpoints = (t_EnumAudioEndpoints) pvtbl[3];
		CA_DO_HOOK(EnumAudioEndpoints);
	}

	if(old_GetDefaultAudioEndpoint == NULL) {
		DWORD* pvtbl = (DWORD*) *(DWORD*) *ppv;
		old_GetDefaultAudioEndpoint = (t_GetDefaultAudioEndpoint) pvtbl[4];
		CA_DO_HOOK(GetDefaultAudioEndpoint);
	}
	
	if(old_GetDevice == NULL) {
		DWORD* pvtbl = (DWORD*) *(DWORD*) *ppv;
		old_GetDevice = (t_GetDevice) pvtbl[5];
		CA_DO_HOOK(GetDevice);
	}

	return hr;
}

int
hook_coreaudio() {
	HMODULE hMod;
	if((hMod = LoadLibrary("ole32.dll")) == NULL) {
		ga_error("Load ole32.dll failed.\n");
		return -1;
	}
	if(old_CoCreateInstance != NULL)
		return 0;
	old_CoCreateInstance =
		(t_CoCreateInstance) GetProcAddress(hMod, "CoCreateInstance");
	if(old_CoCreateInstance == NULL) {
		ga_error("GetProcAddress(CoCreateInstance) failed.\n");
		return -1;
	}
	CA_DO_HOOK(CoCreateInstance);
	return 0;
}

