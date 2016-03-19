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
#ifndef WIN32
#include <unistd.h>
#endif

#include "vsource.h"	// for getting the current audio-id
#include "asource.h"
#include "rtspconf.h"
#include "encoder-common.h"

#include "ga-common.h"
#include "ga-conf.h"
#include "ga-avcodec.h"
#include "ga-module.h"

//MODULE EXPORT void * aencoder_threadproc(void *arg);

static int aencoder_initialized = 0;
static int aencoder_started = 0;
static pthread_t aencoder_tid;

// internal configuration
static int rtp_id = -1;
// for audio encoding
static AVCodecContext *encoder = NULL;
static AVCodecContext *encoder_sdp = NULL;
static int dstlines[SWR_CH_MAX];	// max SWR_CH_MAX (32) channels
static int source_size = -1;
static int encoder_size = -1;
// for audio conversion
static SwrContext *swrctx = NULL;
static const unsigned char *srcplanes[SWR_CH_MAX];
static unsigned char *dstplanes[SWR_CH_MAX];
static unsigned char *convbuf = NULL;

static int
aencoder_deinit(void *arg) {
	if(aencoder_initialized == 0)
		return 0;
	if(convbuf)	free(convbuf);
	if(swrctx)	swr_free(&swrctx);
	if(encoder)	ga_avcodec_close(encoder);
	if(encoder_sdp)	ga_avcodec_close(encoder_sdp);
	//
	swrctx = NULL;
	convbuf = NULL;
	encoder = NULL;
	encoder_sdp = NULL;
	source_size = encoder_size = -1;
	//
	aencoder_initialized = 0;
	ga_error("audio encoder: deinitialized.\n");
	//
	return 0;
}

static int
aencoder_init(void *arg) {
	struct RTSPConf *rtspconf = rtspconf_global();
	rtp_id = video_source_channels();
	if(aencoder_initialized != 0)
		return 0;
	if(rtspconf == NULL) {
		ga_error("audio encoder: no valid global configuration available.\n");
		return -1;
	}
	// no duplicated initialization
	if(encoder != NULL) {
		ga_error("audio encoder: has been initialized.\n");
		return 0;
	}
	// alloc encoder
	encoder = ga_avcodec_aencoder_init(
			NULL,
			rtspconf->audio_encoder_codec,
			rtspconf->audio_bitrate,
			rtspconf->audio_samplerate,
			rtspconf->audio_channels,
			rtspconf->audio_codec_format,
			rtspconf->audio_codec_channel_layout);
	if(encoder == NULL) {
		ga_error("audio encoder: cannot initialized the encoder.\n");
		goto init_failed;
	}
	// encoder for SDP generation
	switch(rtspconf->audio_encoder_codec->id) {
	case AV_CODEC_ID_AAC:
		// need ctx with CODEC_FLAG_GLOBAL_HEADER flag
		encoder_sdp = avcodec_alloc_context3(rtspconf->audio_encoder_codec);
		if(encoder_sdp == NULL)
			goto init_failed;
		encoder_sdp->flags |= CODEC_FLAG_GLOBAL_HEADER;
		if(encoder_sdp == NULL)
			goto init_failed;
		encoder_sdp = ga_avcodec_aencoder_init(encoder_sdp,
				rtspconf->audio_encoder_codec,
				rtspconf->audio_bitrate,
				rtspconf->audio_samplerate,
				rtspconf->audio_channels,
				rtspconf->audio_codec_format,
				rtspconf->audio_codec_channel_layout);
		ga_error("audio encoder: meta-encoder #%d created.\n");
		break;
	default:
		// do nothing
		break;
	}
	// estimate sizes
	source_size = av_samples_get_buffer_size(NULL,
			rtspconf->audio_channels,
			encoder->frame_size,
			rtspconf->audio_device_format, 1/*no-alignment*/);
	encoder_size = av_samples_get_buffer_size(dstlines,
			encoder->channels,
			encoder->frame_size,
			encoder->sample_fmt, 1/*no-alignment*/);
#if 1
	do {
		int i = 0;
		while(dstlines[i] > 0) {
			ga_error("audio encoder: encoder_size=%d, frame_size=%d, dstlines[%d] = %d\n",
				encoder_size, encoder->frame_size, i, dstlines[i]);
			i++;
		}
	} while(0);
#endif
	// need live format conversion?
	if(rtspconf->audio_device_format != encoder->sample_fmt) {
		if((swrctx = swr_alloc_set_opts(NULL, 
				encoder->channel_layout,
				encoder->sample_fmt,
				encoder->sample_rate,
				rtspconf->audio_device_channel_layout,
				rtspconf->audio_device_format,
				rtspconf->audio_samplerate,
				0, NULL)) == NULL) {
			ga_error("audio encoder: cannot allocate swrctx.\n");
			goto init_failed;
		}
		if(swr_init(swrctx) < 0) {
			ga_error("audio encoder: cannot initialize swrctx.\n");
			goto init_failed;
		}
		//
		if((convbuf = (unsigned char*) malloc(encoder_size)) == NULL) {
			ga_error("audio encoder: cannot allocate conversion buffer.\n");
			goto init_failed;
		}
		bzero(convbuf, encoder_size);
		//
		dstplanes[0] = convbuf;
		if(av_sample_fmt_is_planar(encoder->sample_fmt) != 0) {
			// planar
			int i;
			for(i = 1; i < encoder->channels; i++) {
				dstplanes[i] = dstplanes[i-1] + dstlines[i-1];
			}
			dstplanes[i] = NULL;
		} else {
			dstplanes[1] = NULL;
		}
		ga_error("audio encoder: on-the-fly audio format conversion enabled.\n");
		ga_error("audio encoder: convert from %dch(%llx)@%dHz (%s) to %dch(%lld)@%dHz (%s).\n",
			rtspconf->audio_channels, rtspconf->audio_device_channel_layout, rtspconf->audio_samplerate,
			av_get_sample_fmt_name(rtspconf->audio_device_format),
			encoder->channels, encoder->channel_layout, encoder->sample_rate,
			av_get_sample_fmt_name(encoder->sample_fmt));
	}
	//
	aencoder_initialized = 1;
	ga_error("audio encoder: initialized.\n");
	//
	return 0;
init_failed:
	aencoder_deinit(NULL);
	return -1;
}

static void *
aencoder_threadproc(void *arg) {
	struct RTSPConf *rtspconf = rtspconf_global();
	int r, frameunit;
	// input frame
	AVFrame frame0, *snd_in = &frame0;
	int got_packet;
	// buffer used to store encoder outputs
	unsigned char *buf = NULL;
	int bufsize;
	// buffer used to store captured data
	unsigned char *samples = NULL;
	int nsamples, samplebytes, maxsamples, samplesize;
	int offset;
	// for a/v sync
#ifdef WIN32
	LARGE_INTEGER baseT, currT, freq;
#else
	struct timeval baseT, currT;
#endif
	struct timeval tv;
	long long pts = -1LL, newpts = 0LL, ptsOffset = 0LL, ptsSync = 0LL;
	//
	audio_buffer_t *ab = NULL;
	int audio_written = 0;
	int buffer_purged = 0;
	//
	nsamples = 0;
	samplebytes = 0;
	maxsamples = encoder->frame_size;
	samplesize = encoder->frame_size * audio_source_channels() * audio_source_bitspersample() / 8;
	//
	encoder_pts_clear(rtp_id);
	//
	if((ab = audio_source_buffer_init()) == NULL) {
		ga_error("audio encoder: cannot initialize audio source buffer.\n");
		return NULL;
	}
	audio_source_client_register(ga_gettid(), ab);
	//
	if((samples = (unsigned char*) malloc(samplesize)) == NULL) {
		ga_error("audio encoder: cannot allocate sample buffer (%d bytes), terminated.\n", samplesize);
		goto audio_quit;
	}
	//
	bufsize = samplesize;
	if((buf = (unsigned char*) malloc(bufsize)) == NULL) {
		ga_error("audio encoder: cannot allocate encoding buffer (%d bytes), terminated.\n", bufsize);
		goto audio_quit;
	}
	//
	frameunit = audio_source_channels() * audio_source_bitspersample() / 8;
	//
	bzero(snd_in, sizeof(*snd_in));
	av_frame_unref(snd_in);
	// start encoding
	ga_error("audio encoding started: tid=%ld channels=%d, frames=%d (%d/%d bytes), chunk_size=%ld (%d bytes), delay=%d\n",
		ga_gettid(),
		encoder->channels, encoder->frame_size,
		encoder->frame_size * encoder->channels * audio_source_bitspersample() / 8,
		encoder_size,
		audio_source_chunksize(),	//audio->chunk_size
		audio_source_chunkbytes(),	//audio->chunk_bytes
		encoder->delay);
	//
#ifdef WIN32
	QueryPerformanceFrequency(&freq);
#endif
	//
	while(aencoder_started != 0 && encoder_running() > 0) {
		//
		if(buffer_purged == 0) {
			audio_source_buffer_purge(ab);
			buffer_purged = 1;
		}
		// read audio frames
		r = audio_source_buffer_read(ab, samples + samplebytes, maxsamples - nsamples);
		gettimeofday(&tv, NULL);
		if(r <= 0) {
			usleep(1000);
			continue;
		}
#ifdef WIN32
		QueryPerformanceCounter(&currT);
#else
		gettimeofday(&currT, NULL);
#endif
		if(pts == -1LL) {
			baseT = currT;
			ptsSync = encoder_pts_sync(rtspconf->audio_samplerate);
			pts = newpts = ptsSync;
			ptsOffset = r;
		} else {
#ifdef WIN32
			newpts = ptsSync + pcdiff_us(currT, baseT, freq) * rtspconf->audio_samplerate / 1000000LL;
#else
			newpts = ptsSync + tvdiff_us(&currT, &baseT) * rtspconf->audio_samplerate / 1000000LL;
#endif
			newpts -= r;
			newpts -= ptsOffset;
		}
		//
		if(newpts > pts) {
			pts = newpts;
		}
		// encode
		nsamples += r;
		samplebytes += r*frameunit;
		offset = 0;
		while(nsamples >= encoder->frame_size) {
			AVPacket pkt1, *pkt = &pkt1;
			unsigned char *srcbuf;
			int srcsize;
			//
			av_init_packet(pkt);
			snd_in->nb_samples = encoder->frame_size;
			snd_in->format = encoder->sample_fmt;
			snd_in->channel_layout = encoder->channel_layout;
			//
			srcbuf = samples+offset;
			srcsize = source_size;
			//
			if(swrctx != NULL) {
				// format conversion: using libswresample/swr_convert
				// assume source is always in packed (interleaved) format
				srcplanes[0] = srcbuf;
				srcplanes[1] = NULL;
				swr_convert(swrctx, dstplanes, encoder->frame_size,
						    srcplanes, encoder->frame_size);
				srcbuf = convbuf;
				srcsize = encoder_size;
			}
			//
			if(avcodec_fill_audio_frame(snd_in, encoder->channels,
					encoder->sample_fmt, srcbuf/*samples+offset*/,
					srcsize/*encoder_size*/, 1/*no-alignment*/) < 0) {
				// error
				ga_error("DEBUG: avcodec_fill_audio_frame failed.\n");
			}
			snd_in->pts = pts;
			encoder_pts_put(rtp_id, pts, &tv);
			//
			pkt->data = buf;
			pkt->size = bufsize;
			got_packet = 0;
			if(avcodec_encode_audio2(encoder, pkt, snd_in, &got_packet) != 0) {
				ga_error("audio encoder: encoding failed, terminated\n");
				goto audio_quit;
			}
			if(got_packet == 0/* || encoder->coded_frame == NULL*/)
				goto drop_audio_frame;
			// pts rescale is done in encoder_send_packet
			// XXX: some encoder does not produce pts ...
			if(pkt->pts == (int64_t) AV_NOPTS_VALUE) {
				pkt->pts = pts;
			}
			//
#if 0			// XXX: not working since ffmpeg 2.0?
			if(encoder->coded_frame->key_frame)
				pkt->flags |= AV_PKT_FLAG_KEY;
#endif
			if(snd_in->extended_data && snd_in->extended_data != snd_in->data)
				av_freep(snd_in->extended_data);
			pkt->stream_index = 0;
			//
			if(encoder_ptv_get(rtp_id, pkt->pts, &tv, rtspconf->audio_samplerate) == NULL) {
				gettimeofday(&tv, NULL);
			}
			// send the packet
			if(encoder_send_packet("audio-encoder",
				rtp_id/*rtspconf->audio_id*/, pkt,
				/*encoder->coded_frame->*/pkt->pts == AV_NOPTS_VALUE ? pts : /*encoder->coded_frame->*/pkt->pts,
				&tv) < 0) {
				goto audio_quit;
			}
			//
			if(audio_written == 0) {
				audio_written = 1;
				ga_error("first audio frame written (pts=%lld)\n", pts);
			}
drop_audio_frame:
			nsamples -= encoder->frame_size;
			offset += encoder->frame_size * frameunit;
			pts += encoder->frame_size;
		}
		// if something has been processed
		if(offset > 0) {
			if(samplebytes-offset > 0) {
				bcopy(&samples[offset], samples, samplebytes-offset);
			}
			samplebytes -= offset;
		}
	}
audio_quit:
	audio_source_client_unregister(ga_gettid());
	audio_source_buffer_deinit(ab);
	//
	if(samples)	free(samples);
	if(buf)		free(buf);
	aencoder_deinit(NULL);
	ga_error("audio encoder: thread terminated (tid=%ld).\n", ga_gettid());
	//
	return NULL;
}

static int
aencoder_start(void *arg) {
	if(aencoder_started != 0)
		return 0;
	aencoder_started = 1;
	if(pthread_create(&aencoder_tid, NULL, aencoder_threadproc, arg) != 0) {
		aencoder_started = 0;
		ga_error("audio source: create thread failed.\n");
		return -1;
	}
	//pthread_detach(aencoder_tid);
	return 0;
}

static int
aencoder_stop(void *arg) {
	void *ignored;
	if(aencoder_started == 0)
		return 0;
	aencoder_started = 0;
	//pthread_cancel(aencoder_tid);
	pthread_join(aencoder_tid, &ignored);
	return 0;
}

ga_module_t *
module_load() {
	static ga_module_t m;
	struct RTSPConf *rtspconf = rtspconf_global();
	char mime[64];
	bzero(&m, sizeof(m));
	m.type = GA_MODULE_TYPE_AENCODER;
	m.name = strdup("ffmpeg-audio-encoder");
	if(ga_conf_readv("audio-mimetype", mime, sizeof(mime)) != NULL) {
		m.mimetype = strdup(mime);
	}
	m.init = aencoder_init;
	m.start = aencoder_start;
	//m.threadproc = aencoder_threadproc;
	m.stop = aencoder_stop;
	m.deinit = aencoder_deinit;
	return &m;
}

