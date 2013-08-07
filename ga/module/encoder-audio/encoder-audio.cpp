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

#include "vsource.h"	// for getting the current audio-id
#include "asource.h"
#include "server.h"
#include "rtspserver.h"
#include "encoder-common.h"

#include "ga-common.h"
#include "ga-avcodec.h"
#include "ga-module.h"

MODULE EXPORT void * aencoder_threadproc(void *arg);

static struct RTSPConf *rtspconf = NULL;

void *
aencoder_threadproc(void *arg) {
	AVCodecContext *encoder = NULL;
	SwrContext *swrctx = NULL;
	//
	struct AudioBuffer *ab;
	int r, frameunit;
	// input frame
	AVFrame frame0, *snd_in = &frame0;
	int got_packet, source_size, encoder_size;
	// buffer used to store encoder outputs
	unsigned char *buf = NULL;
	int bufsize;
	// buffer used to store captured data
	unsigned char *samples = NULL;
	int nsamples, samplebytes, maxsamples, samplesize;
	int offset;
	// buffer used to convert samples - in cases that they are different
	int dstlines[SWR_CH_MAX];	// max SWR_CH_MAX (32) channels
	const unsigned char *srcplanes[SWR_CH_MAX];
	unsigned char *dstplanes[SWR_CH_MAX];
	unsigned char *convbuf = NULL;
	//
	int rtp_id = video_source_channels();
	// for a/v sync
#ifdef WIN32
	LARGE_INTEGER baseT, currT, freq;
#else
	struct timeval baseT, currT;
#endif
	long long pts = -1LL, newpts = 0LL, ptsOffset = 0LL, ptsSync = 0LL;
	//
	int audio_dropping = 0;
	int audio_written = 0;
	int buffer_purged = 0;
	//
	rtspconf = rtspconf_global();
	//
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
		goto audio_quit;
	}
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
			fprintf(stderr, "encoder_size=%d, frame_size=%d, dstlines[%d] = %d\n",
				encoder_size, encoder->frame_size, i, dstlines[i]);
			i++;
		}
	} while(0);
#endif
	// create converter & memory if necessary
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
			goto audio_quit;
		}
		if(swr_init(swrctx) < 0) {
			ga_error("audio encoder: cannot initialize swrctx.\n");
			goto audio_quit;
		}
		//
		if((convbuf = (unsigned char*) malloc(encoder_size)) == NULL) {
			ga_error("audio encoder: cannot allocate conversion buffer.\n");
			goto audio_quit;
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
		ga_error("audio decoder: on-the-fly audio format conversion enabled.\n");
		ga_error("audio decoder: convert from %dch(%llx)@%dHz (%s) to %dch(%lld)@%dHz (%s).\n",
			rtspconf->audio_channels, rtspconf->audio_device_channel_layout, rtspconf->audio_samplerate,
			av_get_sample_fmt_name(rtspconf->audio_device_format),
			encoder->channels, encoder->channel_layout, encoder->sample_rate,
			av_get_sample_fmt_name(encoder->sample_fmt));
	}
	//
	nsamples = 0;
	samplebytes = 0;
	maxsamples = encoder->frame_size;
	samplesize = encoder->frame_size * audio_source_channels() * audio_source_bitspersample() / 8;
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
	avcodec_get_frame_defaults(snd_in);
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
	while(encoder_running() > 0) {
		//
		if(buffer_purged == 0) {
			audio_source_buffer_purge(ab);
			buffer_purged = 1;
		}
		// read audio frames
		r = audio_source_buffer_read(ab, samples + samplebytes, maxsamples - nsamples);
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
					srcsize/*encoder_size*/, 1/*no-alignment*/) != 0) {
				// error
				ga_error("DEBUG: avcodec_fill_audio_frame failed.\n");
			}
			snd_in->pts = pts;
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
			// pts rescale is done in encoder_send_packet_all
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
			// send the packet
			if(encoder_send_packet_all("audio-encoder",
				rtp_id/*rtspconf->audio_id*/, pkt,
				/*encoder->coded_frame->*/pkt->pts == AV_NOPTS_VALUE ? pts : /*encoder->coded_frame->*/pkt->pts) < 0) {
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
	//
	if(samples)	free(samples);
	if(buf)		free(buf);
	if(convbuf)	free(convbuf);
	if(swrctx)	swr_free(&swrctx);
	if(encoder)	ga_avcodec_close(encoder);
	//
	ga_error("audio encoder: thread terminated (tid=%ld).\n", ga_gettid());
	//
	return NULL;
}

