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

#include <pthread.h>
#include <map>

#include "server.h"
#include "vsource.h"
//#include "filter-rgb2yuv.h"
#include "encoder-common.h"
//#include "encoder-video.h"
//#include "encoder-audio.h"
//#include "encoder-video2.h"
//#include "encoder-audio2.h"

#ifdef PIPELINE_FILTER
#define	SRCPIPEFORMAT	F_RGB2YUV_PIPEFORMAT
#else
#define	SRCPIPEFORMAT	ISOURCE_PIPEFORMAT
#endif

using namespace std;

#ifdef GA_EMCC
static pthread_rwlock_t encoder_lock;
#else
static pthread_rwlock_t encoder_lock = PTHREAD_RWLOCK_INITIALIZER;
#endif
static map<RTSPContext*,RTSPContext*> encoder_clients;

static bool threadLaunched = false;
static pthread_t vethreadId[IMAGE_SOURCE_CHANNEL_MAX];	// multi-channel support
static pthread_t aethreadId;

// for pts sync between encoders
static pthread_mutex_t syncmutex = PTHREAD_MUTEX_INITIALIZER;
static bool sync_reset = true;
static struct timeval synctv;

// list of encoders
static map<void *, void* (*)(void *)> vencoder;
static map<void *, void* (*)(void *)> aencoder;

int
encoder_pts_sync(int samplerate) {
	struct timeval tv;
	long long us;
	int ret;
	//
	pthread_mutex_lock(&syncmutex);
	if(sync_reset) {
		gettimeofday(&synctv, NULL);
		sync_reset = false; 
		pthread_mutex_unlock(&syncmutex);
		return 0;
	}
	gettimeofday(&tv, NULL);
	us = tvdiff_us(&tv, &synctv);
	pthread_mutex_unlock(&syncmutex);
	ret = (int) (0.000001 * us * samplerate);
	return ret > 0 ? ret : 0;
}

int
encoder_running() {
	return threadLaunched ? 1 : 0;
}

int
encoder_register_vencoder(void *(threadproc)(void *), void *arg) {
	vencoder[arg] = threadproc;
	return 0;
}

int
encoder_register_aencoder(void *(threadproc)(void *), void *arg) {
	aencoder[arg] = threadproc;
	return 0;
}

int
encoder_register_client(RTSPContext *rtsp) {
	int vcount = 0;
	pthread_rwlock_wrlock(&encoder_lock);
	if(encoder_clients.size() == 0) {
		map<void *, void* (*)(void *)>::iterator mi;
		// must be set before encoder starts!
		threadLaunched = true;
		// start video encoder threads
		for(mi = vencoder.begin(); mi != vencoder.end(); mi++) {
			if(pthread_create(&vethreadId[vcount++], NULL, mi->second,
					pipeline::lookup((const char *) mi->first)) != 0) {
				pthread_rwlock_unlock(&encoder_lock);
				ga_error("encoder-registration: start video encoder thread(%d) failed.\n", vcount);
				threadLaunched = false;
				return -1;
			}
		}
		// start audio encoder threads
		if((mi = aencoder.begin()) != aencoder.end()) {
			if(pthread_create(&aethreadId, NULL, mi->second, mi->first) != 0) {
				pthread_rwlock_unlock(&encoder_lock);
				ga_error("encoder-registration: start audio encoder thread failed.\n");
				threadLaunched = false;
				return -1;
			}
		}
	}
	encoder_clients[rtsp] = rtsp;
	ga_error("encoder client registered: total %d clients.\n", encoder_clients.size());
	pthread_rwlock_unlock(&encoder_lock);
	return 0;
}

int
encoder_unregister_client(RTSPContext *rtsp) {
	int i;
	void *ignored;
	pthread_rwlock_wrlock(&encoder_lock);
	encoder_clients.erase(rtsp);
	ga_error("encoder client unregistered: %d clients left.\n", encoder_clients.size());
	if(encoder_clients.size() == 0) {
		threadLaunched = false;
		ga_error("encoder: no more clients, quitting ...\n");
		for(i = 0; i < video_source_channels(); i++) {
			pthread_join(vethreadId[i], &ignored);
		}
#ifdef ENABLE_AUDIO
		pthread_join(aethreadId, &ignored);
#endif
		ga_error("encoder: all threads terminated.\n");
		// reset sync pts
		pthread_mutex_lock(&syncmutex);
		sync_reset = true;
		pthread_mutex_unlock(&syncmutex);
	}
	pthread_rwlock_unlock(&encoder_lock);
	return 0;
}

int
encoder_send_packet(const char *prefix, RTSPContext *rtsp, int channelId, AVPacket *pkt, int64_t encoderPts) {
	int iolen;
	uint8_t *iobuf;
	//
	if(rtsp->fmtctx[channelId] == NULL) {
		// not initialized - disabled?
		return 0;
	}
	if(encoderPts != (int64_t) AV_NOPTS_VALUE) {
		pkt->pts = av_rescale_q(encoderPts,
				rtsp->encoder[channelId]->time_base,
				rtsp->stream[channelId]->time_base);
	}
#ifdef HOLE_PUNCHING
	if(ffio_open_dyn_packet_buf(&rtsp->fmtctx[channelId]->pb, rtsp->mtu) < 0) {
		ga_error("%s: buffer allocation failed.\n", prefix);
		return -1;
	}
	if(av_write_frame(rtsp->fmtctx[channelId], pkt) != 0) {
		ga_error("%s: write failed.\n", prefix);
		return -1;
	}
	iolen = avio_close_dyn_buf(rtsp->fmtctx[channelId]->pb, &iobuf);
	if(rtsp->lower_transport[channelId] == RTSP_LOWER_TRANSPORT_TCP) {
		if(rtsp_write_bindata(rtsp, channelId, iobuf, iolen) < 0) {
			av_free(iobuf);
			ga_error("%s: RTSP write failed.\n", prefix);
			return -1;
		}
	} else {
		if(rtp_write_bindata(rtsp, channelId, iobuf, iolen) < 0) {
			av_free(iobuf);
			ga_error("%s: RTP write failed.\n", prefix);
			return -1;
		}
	}
	av_free(iobuf);
#else
	if(rtsp->lower_transport[channelId] == RTSP_LOWER_TRANSPORT_TCP) {
		//if(avio_open_dyn_buf(&rtsp->fmtctx[channelId]->pb) < 0)
		if(ffio_open_dyn_packet_buf(&rtsp->fmtctx[channelId]->pb, rtsp->mtu) < 0) {
			ga_error("%s: buffer allocation failed.\n", prefix);
			return -1;
		}
	}
	if(av_write_frame(rtsp->fmtctx[channelId], pkt) != 0) {
		ga_error("%s: write failed.\n", prefix);
		return -1;
	}
	if(rtsp->lower_transport[channelId] == RTSP_LOWER_TRANSPORT_TCP) {
		int iolen;
		uint8_t *iobuf;
		iolen = avio_close_dyn_buf(rtsp->fmtctx[channelId]->pb, &iobuf);
		if(rtsp_write_bindata(rtsp, channelId, iobuf, iolen) < 0) {
			av_free(iobuf);
			ga_error("%s: write failed.\n", prefix);
			return -1;
		}
		av_free(iobuf);
	}
#endif
	return 0;
}

int
encoder_send_packet_all(const char *prefix, int channelId, AVPacket *pkt, int64_t encoderPts) {
	map<RTSPContext*,RTSPContext*>::iterator mi;
	//pthread_rwlock_rdlock(&encoder_lock);
again:
	if(pthread_rwlock_tryrdlock(&encoder_lock) != 0) {
		if(threadLaunched == false)
			return -1;
		goto again;
	}
	for(mi = encoder_clients.begin(); mi != encoder_clients.end(); mi++) {
		if(mi->second->state != SERVER_STATE_PLAYING)
			continue;
		if(encoder_send_packet(prefix, mi->second, channelId, pkt, encoderPts) < 0) {
			//rtsp_cleanup(mi->second, -1);
		}
	}
	pthread_rwlock_unlock(&encoder_lock);
	return 0;
}

