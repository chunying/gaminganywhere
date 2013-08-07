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

#include "vsource.h"
#include "server.h"
#include "rtspserver.h"
#include "encoder-common.h"

#include "ga-common.h"
#include "ga-avcodec.h"
#include "ga-conf.h"
#include "ga-module.h"

#include "pipeline.h"

MODULE EXPORT void * vencoder_threadproc(void *arg);

static struct RTSPConf *rtspconf = NULL;
static int outputW;
static int outputH;

void *
vencoder_threadproc(void *arg) {
	// arg is pointer to source pipe
	// image info
	int iid;
	int iwidth;
	int iheight;
	int rtp_id;
	struct pooldata *data = NULL;
	struct vsource_frame *frame = NULL;
	pipeline *pipe = (pipeline*) arg;
	AVCodecContext *encoder = NULL;
	//
	AVFrame *pic_in = NULL;
	unsigned char *pic_in_buf = NULL;
	int pic_in_size;
	unsigned char *nalbuf = NULL, *nalbuf_a = NULL;
	int nalbuf_size = 0, nalign = 0;
	long long basePts = -1LL, newpts = 0LL, pts = -1LL, ptsSync = 0LL;
	pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	//
	int resolution[2];
	int video_written = 0;
	//
	if(pipe == NULL) {
		ga_error("video encoder: NULL pipeline specified.\n");
		goto video_quit;
	}
	//
	rtspconf = rtspconf_global();
	// init variables
	iid = ((struct vsource_config*) pipe->get_privdata())->id;
	iwidth = video_source_maxwidth(iid);
	iheight = video_source_maxheight(iid);
	rtp_id = ((struct vsource_config*) pipe->get_privdata())->rtp_id;
	//
	outputW = iwidth;	// by default, the same as max resolution
	outputH = iheight;
	if(ga_conf_readints("output-resolution", resolution, 2) == 2) {
		outputW = resolution[0];
		outputH = resolution[1];
	}
	//
	ga_error("video encoder: image source from '%s' (%dx%d) via channel %d, resolution=%dx%d.\n",
		pipe->name(), iwidth, iheight, rtp_id, outputW, outputH);
	//
	encoder = ga_avcodec_vencoder_init(
			NULL,
			rtspconf->video_encoder_codec,
			outputW, outputH,
			rtspconf->video_fps,
			rtspconf->vso);
	if(encoder == NULL) {
		ga_error("video encoder: cannot initialized the encoder.\n");
		goto video_quit;
	}
	//
	nalbuf_size = 100000+12 * outputW * outputH;
	if(ga_malloc(nalbuf_size, (void**) &nalbuf, &nalign) < 0) {
		ga_error("video encoder: buffer allocation failed, terminated.\n");
		goto video_quit;
	}
	nalbuf_a = nalbuf + nalign;
	//
	if((pic_in = avcodec_alloc_frame()) == NULL) {
		ga_error("video encoder: picture allocation failed, terminated.\n");
		goto video_quit;
	}
	pic_in_size = avpicture_get_size(PIX_FMT_YUV420P, outputW, outputH);
	if((pic_in_buf = (unsigned char*) av_malloc(pic_in_size)) == NULL) {
		ga_error("video encoder: picture buffer allocation failed, terminated.\n");
		goto video_quit;
	}
	avpicture_fill((AVPicture*) pic_in, pic_in_buf,
			PIX_FMT_YUV420P, outputW, outputH);
	//ga_error("video encoder: linesize = %d|%d|%d\n", pic_in->linesize[0], pic_in->linesize[1], pic_in->linesize[2]);
	// start encoding
	ga_error("video encoding started: tid=%ld %dx%d@%dfps, nalbuf_size=%d, pic_in_size=%d.\n",
		ga_gettid(),
		iwidth, iheight, rtspconf->video_fps,
		nalbuf_size, pic_in_size);
	//
	pipe->client_register(ga_gettid(), &cond);
	//
	while(encoder_running() > 0) {
		AVPacket pkt;
		int got_packet = 0;
		// wait for notification
		data = pipe->load_data();
		if(data == NULL) {
			int err;
			struct timeval tv;
			struct timespec to;
			gettimeofday(&tv, NULL);
			to.tv_sec = tv.tv_sec+1;
			to.tv_nsec = tv.tv_usec * 1000;
			//
			if((err = pipe->timedwait(&cond, &condMutex, &to)) != 0) {
				ga_error("viedo encoder: image source timed out.\n");
				continue;
			}
			data = pipe->load_data();
			if(data == NULL) {
				ga_error("viedo encoder: unexpected NULL frame received (from '%s', data=%d, buf=%d).\n",
					pipe->name(), pipe->data_count(), pipe->buf_count());
				continue;
			}
		}
		frame = (struct vsource_frame*) data->ptr;
		// handle pts
		if(basePts == -1LL) {
			basePts = frame->imgpts;
			ptsSync = encoder_pts_sync(rtspconf->video_fps);
			newpts = ptsSync;
		} else {
			newpts = ptsSync + frame->imgpts - basePts;
		}
		// XXX: assume always YUV420P
		if(pic_in->linesize[0] == frame->linesize[0]
		&& pic_in->linesize[1] == frame->linesize[1]
		&& pic_in->linesize[2] == frame->linesize[2]) {
			bcopy(frame->imgbuf, pic_in_buf, pic_in_size);
		} else {
			ga_error("video encoder: YUV mode failed - mismatched linesize(s) (src:%d,%d,%d; dst:%d,%d,%d)\n",
				frame->linesize[0], frame->linesize[1], frame->linesize[2],
				pic_in->linesize[0], pic_in->linesize[1], pic_in->linesize[2]);
			pipe->release_data(data);
			goto video_quit;
		}
		pipe->release_data(data);
		// pts must be monotonically increasing
		if(newpts > pts) {
			pts = newpts;
		} else {
			pts++;
		}
		// encode
		pic_in->pts = pts;
		av_init_packet(&pkt);
		pkt.data = nalbuf_a;
		pkt.size = nalbuf_size;
		if(avcodec_encode_video2(encoder, &pkt, pic_in, &got_packet) < 0) {
			ga_error("video encoder: encode failed, terminated.\n");
			goto video_quit;
		}
		if(got_packet) {
			if(pkt.pts == (int64_t) AV_NOPTS_VALUE) {
				pkt.pts = pts;
			}
			pkt.stream_index = 0;
			// send the packet
			if(encoder_send_packet_all("video-encoder",
				rtp_id/*rtspconf->video_id*/, &pkt,
				pkt.pts) < 0) {
				goto video_quit;
			}
			// free unused side-data
			if(pkt.side_data_elems > 0) {
				int i;
				for (i = 0; i < pkt.side_data_elems; i++)
					av_free(pkt.side_data[i].data);
				av_freep(&pkt.side_data);
				pkt.side_data_elems = 0;
			}
			//
			if(video_written == 0) {
				video_written = 1;
				ga_error("first video frame written (pts=%lld)\n", pts);
			}
		}
	}
	//
video_quit:
	if(pipe) {
		pipe->client_unregister(ga_gettid());
		pipe = NULL;
	}
	//
	if(pic_in_buf)	av_free(pic_in_buf);
	if(pic_in)	av_free(pic_in);
	if(nalbuf)	free(nalbuf);
	if(encoder)	ga_avcodec_close(encoder);
	//
	ga_error("video encoder: thread terminated (tid=%ld).\n", ga_gettid());
	//
	return NULL;
}

