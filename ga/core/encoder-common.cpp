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

#include <pthread.h>
#include <map>
#include <list>

#include "server.h"
#include "vsource.h"
#include "encoder-common.h"

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
//static map<RTSPContext*,RTSPContext*> encoder_clients;
static map<void*, void*> encoder_clients;

static bool threadLaunched = false;

// for pts sync between encoders
static pthread_mutex_t syncmutex = PTHREAD_MUTEX_INITIALIZER;
static bool sync_reset = true;
static struct timeval synctv;

// list of encoders
static ga_module_t *vencoder = NULL;
static ga_module_t *aencoder = NULL;
static void *vencoder_param = NULL;
static void *aencoder_param = NULL;

static int encoder_send_packet_all_default(const char *, int, AVPacket *, int64_t, struct timeval *);
// for ffmpeg-based rtsp server
static int encoder_send_packet_all_ffmpeg(const char *, int, AVPacket *, int64_t, struct timeval *);
static int encoder_send_packet_ffmpeg(const char *, void *ctx, int, AVPacket*, int64_t, struct timeval *);
// for live-555 based rtsp server
static int encoder_send_packet_live(const char *, void *ctx, int, AVPacket*, int64_t, struct timeval *);

static int (*encoder_send_packet_all_internal)(const char *, int, AVPacket *, int64_t, struct timeval *)
			= encoder_send_packet_all_ffmpeg;
static int (*encoder_send_packet_internal)(const char *, void *, int, AVPacket *, int64_t, struct timeval *)
			= encoder_send_packet_ffmpeg;

int
encoder_config_rtspserver(int type) {
	switch(type) {
	case RTSPSERVER_TYPE_FFMPEG:
		encoder_send_packet_all_internal = encoder_send_packet_all_ffmpeg;
		encoder_send_packet_internal = encoder_send_packet_ffmpeg;
		ga_error("encoder: working with ffmpeg-based RTSP server.\n");
		break;
	case RTSPSERVER_TYPE_LIVE:
		encoder_send_packet_all_internal = encoder_send_packet_all_default;
		encoder_send_packet_internal = encoder_send_packet_live;
		ga_error("encoder: working with live555-based RTSP server.\n");
		break;
	case RTSPSERVER_TYPE_NULL:
	default:
		ga_error("unknown rtspserver (%d)\n", type);
		exit(-1);
	}
	return 0;
}

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
encoder_register_vencoder(ga_module_t *m, void *param) {
	if(vencoder != NULL) {
		ga_error("encoder: warning - replace video encoder %s with %s\n",
			vencoder->name, m->name);
	}
	vencoder = m;
	vencoder_param = param;
	ga_error("video encoder: %s registered\n", m->name);
	return 0;
}

int
encoder_register_aencoder(ga_module_t *m, void *param) {
	if(aencoder != NULL) {
		ga_error("encoder warning - replace audio encoder %s with %s\n",
			aencoder->name, m->name);
	}
	aencoder = m;
	aencoder_param = param;
	ga_error("audio encoder: %s registered\n", m->name);
	return 0;
}

ga_module_t *
encoder_get_vencoder() {
	return vencoder;
}

ga_module_t *
encoder_get_aencoder() {
	return aencoder;
}

int
encoder_register_client(void /*RTSPContext*/ *rtsp) {
	pthread_rwlock_wrlock(&encoder_lock);
	if(encoder_clients.size() == 0) {
		// initialize video encoder
		if(vencoder != NULL && vencoder->init != NULL) {
			if(vencoder->init(vencoder_param) < 0) {
				ga_error("video encoder: init failed.\n");
				exit(-1);;
			}
		}
		// initialize audio encoder
		if(aencoder != NULL && aencoder->init != NULL) {
			if(aencoder->init(aencoder_param) < 0) {
				ga_error("audio encoder: init failed.\n");
				exit(-1);
			}
		}
		// must be set before encoder starts!
		threadLaunched = true;
		// start video encoder
		if(vencoder != NULL && vencoder->start != NULL) {
			if(vencoder->start(vencoder_param) < 0) {
				pthread_rwlock_unlock(&encoder_lock);
				ga_error("video encoder: start failed.\n");
				threadLaunched = false;
				exit(-1);
			}
		}
		// start audio encoder
		if(aencoder != NULL && aencoder->start != NULL) {
			if(aencoder->start(aencoder_param) < 0) {
				pthread_rwlock_unlock(&encoder_lock);
				ga_error("audio encoder: start failed.\n");
				threadLaunched = false;
				exit(-1);
			}
		}
	}
	encoder_clients[rtsp] = rtsp;
	ga_error("encoder client registered: total %d clients.\n", encoder_clients.size());
	pthread_rwlock_unlock(&encoder_lock);
	return 0;
}

int
encoder_unregister_client(void /*RTSPContext*/ *rtsp) {
	pthread_rwlock_wrlock(&encoder_lock);
	encoder_clients.erase(rtsp);
	ga_error("encoder client unregistered: %d clients left.\n", encoder_clients.size());
	if(encoder_clients.size() == 0) {
		threadLaunched = false;
		ga_error("encoder: no more clients, quitting ...\n");
		if(vencoder != NULL && vencoder->stop != NULL)
			vencoder->stop(vencoder_param);
		if(vencoder != NULL && vencoder->deinit != NULL)
			vencoder->deinit(vencoder_param);
#ifdef ENABLE_AUDIO
		if(aencoder != NULL && aencoder->stop != NULL)
			aencoder->stop(aencoder_param);
		if(aencoder != NULL && aencoder->deinit != NULL)
			aencoder->deinit(aencoder_param);
#endif
		// reset sync pts
		pthread_mutex_lock(&syncmutex);
		sync_reset = true;
		pthread_mutex_unlock(&syncmutex);
	}
	pthread_rwlock_unlock(&encoder_lock);
	return 0;
}

int
encoder_send_packet(const char *prefix, void *ctx, int channelId, AVPacket *pkt, int64_t encoderPts, struct timeval *ptv) {
	return encoder_send_packet_internal(prefix, ctx, channelId, pkt, encoderPts, ptv);
}

static int
encoder_send_packet_ffmpeg(const char *prefix, void *ctx, int channelId, AVPacket *pkt, int64_t encoderPts, struct timeval *ptv) {
	int iolen;
	uint8_t *iobuf;
	RTSPContext *rtsp = (RTSPContext*) ctx;
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

static int
encoder_send_packet_live(const char *prefix, void *ctx, int channelId, AVPacket *pkt, int64_t encoderPts, struct timeval *ptv) {
	encoder_pktqueue_append(channelId, pkt, encoderPts, ptv);
	return -1;
}

int
encoder_send_packet_all(const char *prefix, int channelId, AVPacket *pkt, int64_t encoderPts, struct timeval *ptv) {
	return encoder_send_packet_all_internal(prefix, channelId, pkt, encoderPts, ptv);
}

static int
encoder_send_packet_all_ffmpeg(const char *prefix, int channelId, AVPacket *pkt, int64_t encoderPts, struct timeval *ptv) {
	map<void*,void*>::iterator mi;
again:
	if(pthread_rwlock_tryrdlock(&encoder_lock) != 0) {
		if(threadLaunched == false)
			return -1;
		goto again;
	}
	for(mi = encoder_clients.begin(); mi != encoder_clients.end(); mi++) {
		if(((RTSPContext*) mi->second)->state != SERVER_STATE_PLAYING)
			continue;
		if(encoder_send_packet(prefix, mi->second, channelId, pkt, encoderPts, ptv) < 0) {
			//rtsp_cleanup(mi->second, -1);
		}
	}
	pthread_rwlock_unlock(&encoder_lock);
	return 0;
}

static int
encoder_send_packet_all_default(const char *prefix, int channelId, AVPacket *pkt, int64_t encoderPts, struct timeval *ptv) {
	map<void*,void*>::iterator mi;
again:
	if(pthread_rwlock_tryrdlock(&encoder_lock) != 0) {
		if(threadLaunched == false)
			return -1;
		goto again;
	}
	for(mi = encoder_clients.begin(); mi != encoder_clients.end(); mi++) {
		encoder_send_packet(prefix, mi->second, channelId, pkt, encoderPts, ptv);
	}
	pthread_rwlock_unlock(&encoder_lock);
	return 0;
}

// encoder packet queue functions - for async packet delivery
static encoder_packet_queue_t pktqueue[VIDEO_SOURCE_CHANNEL_MAX+1];
static list<encoder_packet_t> pktlist[VIDEO_SOURCE_CHANNEL_MAX+1];
static map<qcallback_t,qcallback_t>queue_cb[VIDEO_SOURCE_CHANNEL_MAX+1];

int
encoder_pktqueue_init(int channels, int qsize) {
	int i;
	for(i = 0; i < channels; i++) {
		if(pktqueue[i].buf != NULL)
			free(pktqueue[i].buf);
		//
		bzero(&pktqueue[i], sizeof(encoder_packet_queue_t));
		pthread_mutex_init(&pktqueue[i].mutex, NULL);
		if((pktqueue[i].buf = (char *) malloc(qsize)) == NULL) {
			ga_error("encoder: initialized packet queue#%d failed (%d bytes)\n",
				i, qsize);
			exit(-1);
		}
		pktqueue[i].bufsize = qsize;
		pktqueue[i].datasize = 0;
		pktqueue[i].head = 0;
		pktqueue[i].tail = 0;
	}
	ga_error("encoder: packet queue initialized (%dx%d bytes)\n", channels, qsize);
	return 0;
}

int
encoder_pktqueue_size(int channelId) {
	return pktqueue[channelId].datasize;
}

int
encoder_pktqueue_append(int channelId, AVPacket *pkt, int64_t encoderPts, struct timeval *ptv) {
	encoder_packet_queue_t *q = &pktqueue[channelId];
	encoder_packet_t qp;
	map<qcallback_t,qcallback_t>::iterator mi;
	int padding = 0;
	pthread_mutex_lock(&q->mutex);
size_check:
	// size checking
	if(q->datasize + pkt->size > q->bufsize) {
		pthread_mutex_unlock(&q->mutex);
		ga_error("encoder: packet queue #%d full, packet dropped (%d+%d)\n",
			channelId, q->datasize, pkt->size);
		return -1;
	}
	// end-of-buffer space is not sufficient
	if(q->bufsize - q->tail < pkt->size) {
		if(pktlist[channelId].size() == 0) {
			q->datasize = q->tail = q->head = 0;
		} else {
			padding = q->bufsize - q->tail;
			pktlist[channelId].back().padding = padding;
			q->datasize += padding;
			q->tail = 0;
		}
		goto size_check;
	}
	bcopy(pkt->data, q->buf + q->tail, pkt->size);
	//
	qp.data = q->buf + q->tail;
	qp.size = pkt->size;
	qp.pts_int64 = pkt->pts;
	if(ptv != NULL) {
		qp.pts_tv = *ptv;
	} else {
		gettimeofday(&qp.pts_tv, NULL);
	}
	//qp.pos = q->tail;
	qp.padding = 0;
	//
	q->tail += pkt->size;
	q->datasize += pkt->size;
	pktlist[channelId].push_back(qp);
	//
	if(q->tail == q->bufsize)
		q->tail = 0;
	//
	pthread_mutex_unlock(&q->mutex);
	// notify client
	for(mi = queue_cb[channelId].begin(); mi != queue_cb[channelId].end(); mi++) {
		mi->second(channelId);
	}
	//
	return 0;
}

char *
encoder_pktqueue_front(int channelId, encoder_packet_t *pkt) {
	encoder_packet_queue_t *q = &pktqueue[channelId];
	pthread_mutex_lock(&q->mutex);
	if(pktlist[channelId].size() == 0) {
		pthread_mutex_unlock(&q->mutex);
		return NULL;
	}
	*pkt = pktlist[channelId].front();
	pthread_mutex_unlock(&q->mutex);
	return pkt->data;
}

void
encoder_pktqueue_split_packet(int channelId, char *offset) {
	encoder_packet_queue_t *q = &pktqueue[channelId];
	encoder_packet_t *pkt, newpkt;
	pthread_mutex_lock(&q->mutex);
	// has packet?
	if(pktlist[channelId].size() == 0)
		goto quit_split_packet;
	pkt = &pktlist[channelId].front();
	// offset must be in the middle
	if(offset <= pkt->data || offset >= pkt->data + pkt->size)
		goto quit_split_packet;
	// split the packet: the new one
	newpkt = *pkt;
	newpkt.size = offset - pkt->data;
	newpkt.padding = 0;
	//
	pkt->data = offset;
	pkt->size -= newpkt.size;
	//
	pktlist[channelId].push_front(newpkt);
	//
	pthread_mutex_unlock(&q->mutex);
	return;
quit_split_packet:
	pthread_mutex_unlock(&q->mutex);
	return;
}

void
encoder_pktqueue_pop_front(int channelId) {
	encoder_packet_queue_t *q = &pktqueue[channelId];
	encoder_packet_t qp;
	pthread_mutex_lock(&q->mutex);
	if(pktlist[channelId].size() == 0) {
		pthread_mutex_unlock(&q->mutex);
		return;
	}
	qp = pktlist[channelId].front();
	pktlist[channelId].pop_front();
	// update the packet queue
	q->head += qp.size;
	q->head += qp.padding;
	q->datasize -= qp.size;
	q->datasize -= qp.padding;
	if(q->head == q->bufsize) {
		q->head = 0;
	}
	if(q->head == q->tail) {
		q->head = q->tail = 0;
	}
	//
	pthread_mutex_unlock(&q->mutex);
	return;
}

int
encoder_pktqueue_register_callback(int channelId, qcallback_t cb) {
	queue_cb[channelId][cb] = cb;
	ga_error("encoder: pktqueue #%d callback registered (%p)\n", channelId, cb);
	return 0;
}

int
encoder_pktqueue_unregister_callback(int channelId, qcallback_t cb) {
	queue_cb[channelId].erase(cb);
	return 0;
}

