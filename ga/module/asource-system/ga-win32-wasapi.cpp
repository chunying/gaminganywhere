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

#include "asource.h"

#include "ga-win32-wasapi.h"

#define REFTIMES_PER_SEC	10000000	// 1s, in 100-ns unit
#define REFTIMES_PER_MILLISEC	10000
#define REQUESTED_DURATION	2000000		// 200ms, in 100-ns unit
// XXX: Small REQUESTED_DURATION sometimes may cause problems ..

#define EXIT_ON_ERROR(hres, mesg)  \
		if (FAILED(hres)) { ga_error(mesg); goto quit; }

#define SAFE_RELEASE(punk)  \
		if ((punk) != NULL)  \
		{ (punk)->Release(); (punk) = NULL; }

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

static int
check_wave_format(ga_wasapi_param *wparam) {
	WAVEFORMATEX *pwfx = wparam->pwfx;
	WAVEFORMATEXTENSIBLE *ext = (WAVEFORMATEXTENSIBLE*) wparam->pwfx;
	//
	if(pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		if(ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
			wparam->isFloat = 1;
		} else if(ext->SubFormat != KSDATAFORMAT_SUBTYPE_PCM) {
			OLECHAR *guidstr;
			char *in, *out, guid2[256];
			StringFromCLSID(ext->SubFormat, &guidstr);
			// because GUID is UTF-16LE?
			for(in = (char*) guidstr, out = guid2; *in; in += 2) {
				*out++ = *in;
			}
			*out = '\0';
			//
			ga_error("WAVEFORMATEXTENSIBLE: non-PCM is not supported (Format GUID=%s)\n", guid2);
			CoTaskMemFree(guidstr);
			return -1;
		}
	} else if(pwfx->wFormatTag != WAVE_FORMAT_PCM) {
		ga_error("WAVEFORMATEX: non-PCM is not supported\n");
		return -1;
	}
	if(pwfx->nChannels != 2) {
		ga_error("WAVEFORMATEX: channels = %d (!=2)\n",
			pwfx->nChannels);
		return -1;
	}
	ga_error("WAVEFORMATEX: samplerate=%d, bitspersample=%d\n",
		pwfx->nSamplesPerSec, pwfx->wBitsPerSample);
	//
	if(wparam->samplerate != pwfx->nSamplesPerSec) {
		ga_error("WAVEFORMATEX: audio sample rate mismatch (%d != %d)\n",
			wparam->samplerate,
			pwfx->nSamplesPerSec);
		return -1;
	}
	//
	if(wparam->isFloat) {
		if(wparam->bits_per_sample != 16) {
			ga_error("WAVEFORMATEX: [float] audio bits per sample mismatch (%d != 16)\n",
				wparam->bits_per_sample);
		}
	} else if(wparam->bits_per_sample != pwfx->wBitsPerSample) {
		ga_error("WAVEFORMATEX: audio bits per sample mismatch (%d != %d)\n",
			wparam->bits_per_sample,
			pwfx->wBitsPerSample);
		return -1;
	}
	return 0;
}

int
ga_wasapi_init(ga_wasapi_param *wasapi) {
	REFERENCE_TIME hnsRequestedDuration;
	int ret = 0;
	HRESULT hr;
	//
	hnsRequestedDuration = REQUESTED_DURATION;
	//
	hr = CoCreateInstance(
			CLSID_MMDeviceEnumerator, NULL,
			CLSCTX_ALL, IID_IMMDeviceEnumerator,
			(void**) &wasapi->pEnumerator);
	EXIT_ON_ERROR(hr, "WASAPI: CoCreateInstance failed.\n");

	hr = wasapi->pEnumerator->GetDefaultAudioEndpoint(
			eRender, eConsole, &wasapi->pDevice);
	EXIT_ON_ERROR(hr, "WASAPI: GetDefaultAudioEndpoint failed.\n");

	hr = wasapi->pDevice->Activate(
			IID_IAudioClient, CLSCTX_ALL,
			NULL, (void**) &wasapi->pAudioClient);
	EXIT_ON_ERROR(hr, "WASAPI: Activate device failed.\n");

	hr = wasapi->pAudioClient->GetMixFormat(&wasapi->pwfx);
	EXIT_ON_ERROR(hr, "WASAPI: GetMixFormat failed.\n");

	// XXX: check pwfx against the audio configuration
	if(check_wave_format(wasapi) < 0)
		goto quit;

	hr = wasapi->pAudioClient->Initialize(
			AUDCLNT_SHAREMODE_SHARED,
			AUDCLNT_STREAMFLAGS_LOOPBACK/*0*/,
			hnsRequestedDuration,
			0,
			wasapi->pwfx,
			NULL);
	EXIT_ON_ERROR(hr, "WASAPI: Initialize audio client failed.\n");

	// Get the size of the allocated buffer.
	hr = wasapi->pAudioClient->GetBufferSize(&wasapi->bufferFrameCount);
	EXIT_ON_ERROR(hr, "WASAPI: Get buffer size failed.\n");

	wasapi->hnsActualDuration = (UINT32) ((double) REFTIMES_PER_SEC *
		wasapi->bufferFrameCount / wasapi->pwfx->nSamplesPerSec);
	wasapi->bufferFillInt = (DWORD) (wasapi->hnsActualDuration/REFTIMES_PER_MILLISEC/2);

	hr = wasapi->pAudioClient->GetService(
			IID_IAudioCaptureClient,
			(void**)&wasapi->pCaptureClient);
	EXIT_ON_ERROR(hr, "WASAPI: Get service failed.\n");

	// sync configurations with other platforms
	wasapi->chunk_size = (wasapi->bufferFrameCount)/2;
	wasapi->bits_per_frame = wasapi->bits_per_sample * wasapi->channels;
	wasapi->chunk_bytes = wasapi->chunk_size * wasapi->bits_per_frame;

	// start capture
	hr = wasapi->pAudioClient->Start();
	EXIT_ON_ERROR(hr, "WASAPI: Cannot start audio client.\n");
	gettimeofday(&wasapi->initialTimestamp, NULL);

	return 0;
quit:
	CoTaskMemFree(wasapi->pwfx);
	SAFE_RELEASE(wasapi->pEnumerator);
	SAFE_RELEASE(wasapi->pDevice);
	SAFE_RELEASE(wasapi->pAudioClient);
	SAFE_RELEASE(wasapi->pCaptureClient);
	return -1;
}

int
ga_wasapi_read(ga_wasapi_param *wasapi, unsigned char *wbuf, int wframes) {
	int i, copysize = 0, copyframe = 0;
	HRESULT hr;
	UINT32 packetLength, numFramesAvailable;
	BYTE *pData;
	DWORD flags;
	UINT64 framePos;
	int srcunit = wasapi->bits_per_sample / 8;
	int dstunit = audio_source_bitspersample() / 8;
	struct timeval beforeSleep, afterSleep;
	bool filldata = false;
	// frame statistics 
	struct timeval currtv;
	//
	if(wasapi->firstRead.tv_sec == 0) {
		gettimeofday(&wasapi->firstRead, NULL);
		wasapi->trimmedFrames = (UINT64) (1.0 * wasapi->samplerate *
			tvdiff_us(&wasapi->firstRead, &wasapi->initialTimestamp) /
			1000000);
		wasapi->silenceFrom = wasapi->firstRead;
		ga_error("WASAPI: estimated trimmed frames = %lld\n",
			wasapi->trimmedFrames);
	}
	//
	gettimeofday(&currtv, NULL);
	if(wasapi->lastTv.tv_sec == 0) {
		gettimeofday(&wasapi->lastTv, NULL);
		wasapi->frames = 0;
		wasapi->sframes = 0;
		wasapi->slept = 0;
	} else if(tvdiff_us(&currtv, &wasapi->lastTv) >= 1000000) {
#if 0
		ga_error(
			"Frame statistics: s=%d, ns=%d, sum=%d (sleep=%d)\n",
			wasapi->sframes, wasapi->frames,
			wasapi->sframes + wasapi->frames,
			wasapi->slept);
#endif
		wasapi->lastTv = currtv;
		wasapi->frames = wasapi->sframes = wasapi->slept = 0;
	}
	//
	if(wasapi->fillSilence > 0) {
		if(wasapi->fillSilence <= wframes) {
			copyframe = (int) wasapi->fillSilence;
		} else {
			copyframe = wframes;
		}
		copysize = copyframe * wasapi->channels * dstunit;
		ZeroMemory(wbuf, copysize);
		//
		wasapi->fillSilence -= copyframe;
		wframes -= copyframe;
		wasapi->sframes += copyframe;
		if(wframes <= 0) {
			return copyframe;
		}
	}
	//
	hr = wasapi->pCaptureClient->GetNextPacketSize(&packetLength);
	EXIT_ON_ERROR(hr, "WASAPI: Get packet size failed.\n");
	//
	if(packetLength == 0) {
		Sleep(wasapi->bufferFillInt);
		gettimeofday(&afterSleep, NULL);
		//
		wasapi->slept++;
		hr = wasapi->pCaptureClient->GetNextPacketSize(&packetLength);
		EXIT_ON_ERROR(hr, "WASAPI: Get packet size failed.\n");
		if(packetLength == 0) {
			// fill silence
			double silenceFrame = 1.0 *
				tvdiff_us(&afterSleep, &wasapi->silenceFrom) *
				wasapi->samplerate / 1000000.0;
			wasapi->fillSilence += (UINT64) silenceFrame;
			wasapi->silenceFrom = afterSleep;
		}
	}
	//
	while(packetLength != 0 && wframes >= (int) packetLength) {
		hr = wasapi->pCaptureClient->GetBuffer(&pData,
			&numFramesAvailable, &flags, &framePos, NULL);
		EXIT_ON_ERROR(hr, "WASAPI: Get buffer failed.\n");
		
		if(packetLength != numFramesAvailable) {
			ga_error("WARNING: packetLength(%d) != numFramesAvailable(%d)\n",
				packetLength, numFramesAvailable);
		}

		if(flags & AUDCLNT_BUFFERFLAGS_SILENT) {
			wasapi->sframes += numFramesAvailable;
			ZeroMemory(&wbuf[copysize], numFramesAvailable * wasapi->channels * dstunit);
			//ga_error("WASAPI-DEBUG: write slience (%d).\n", numFramesAvailable);
		} else {
			wasapi->frames += numFramesAvailable;
			if(wasapi->isFloat) {
				float *r = (float*) (pData);
				short *w = (short*) (&wbuf[copysize]);
				int cc = numFramesAvailable * wasapi->channels;
				for(i = 0; i < cc; i++) {
					*w = (short) (*r * 32768.0);
					r++;
					w++;
				}
			} else {
				CopyMemory(&wbuf[copysize], pData, numFramesAvailable * wasapi->channels * dstunit);
			}
			//ga_error("WASAPI-DEBUG: write data (%d).\n", numFramesAvailable);
		}

		wframes -= numFramesAvailable;
		copyframe += numFramesAvailable;
		copysize += numFramesAvailable * wasapi->channels * dstunit;

		hr = wasapi->pCaptureClient->ReleaseBuffer(numFramesAvailable);
		EXIT_ON_ERROR(hr, "WASAPI: Release buffer failed.\n");

		hr = wasapi->pCaptureClient->GetNextPacketSize(&packetLength);
		EXIT_ON_ERROR(hr, "WASAPI: Get packet size failed.\n");

		filldata = true;
	}
	//
	if(filldata) {
		gettimeofday(&wasapi->silenceFrom, NULL);
	}
	//
	return copyframe;
quit:
	return -1;
}

int
ga_wasapi_close(ga_wasapi_param *wasapi) {
	//
	wasapi->pAudioClient->Stop();
	//
	//
	CoTaskMemFree(wasapi->pwfx);
	SAFE_RELEASE(wasapi->pEnumerator);
	SAFE_RELEASE(wasapi->pDevice);
	SAFE_RELEASE(wasapi->pAudioClient);
	SAFE_RELEASE(wasapi->pCaptureClient);
	return 0;
}
