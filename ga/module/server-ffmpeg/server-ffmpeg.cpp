/*
 * Copyright (c) 2013-2015 Chun-Ying Huang
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
#include <pthread.h>
#ifndef WIN32
#include <unistd.h>
#include <netinet/in.h>
#include <sys/syscall.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <arpa/inet.h>
#endif	/* ! WIN32 */

#include "ga-common.h"
#include "ga-module.h"
#include "encoder-common.h"
#include "rtspconf.h"

#include "server-ffmpeg.h"
#include "rtspserver.h"

#include <map>
using namespace std;

#ifdef WIN32
static SOCKET server_socket = INVALID_SOCKET;	/**< The server socket */
#else
static int server_socket = -1;			/**< The server socket */
#endif
static pthread_t server_tid;
static int server_started = 0;
static pthread_rwlock_t cclock = PTHREAD_RWLOCK_INITIALIZER;
static map<void *, void *> client_context;

int
ff_server_register_client(void *ccontext) {
	if(encoder_register_client(ccontext) < 0)
		return -1;
	//
	pthread_rwlock_wrlock(&cclock);
	client_context[ccontext] = ccontext;
	pthread_rwlock_unlock(&cclock);
	return 0;
}

int
ff_server_unregister_client(void *ccontext) {
	if(encoder_unregister_client(ccontext) < 0)
		return -1;
	//
	pthread_rwlock_wrlock(&cclock);
	client_context.erase(ccontext);
	pthread_rwlock_unlock(&cclock);
	return 0;
}

static void *
ff_server_main(void *arg) {
#ifdef WIN32
	SOCKET cs;
	int csinlen;
#else
	int cs;
	socklen_t csinlen;
#endif
	struct sockaddr_in csin;
	//
	server_started = 1;
	//
	do {
		pthread_t thread;
		//
		csinlen = sizeof(csin);
		bzero(&csin, sizeof(csin));
		if((cs = accept(server_socket, (struct sockaddr*) &csin, &csinlen)) < 0) {
			perror("accept");
			return (void *) -1;
		}
		// tunning sending window
		do {
			int sndwnd = 8388608;	// 8MB
			if(setsockopt(cs, SOL_SOCKET, SO_SNDBUF, &sndwnd, sizeof(sndwnd)) == 0) {
				ga_error("ffmpeg-server: set TCP sending buffer success.\n");
			} else {
				ga_error("ffmpeg-server: set TCP sending buffer failed.\n");
			}
		} while(0);
		//
		pthread_cancel_init();
		if(pthread_create(&thread, NULL, rtspserver, &cs) != 0) {
			close(cs);
			ga_error("ffmpeg-server: cannot create service thread.\n");
			continue;
		}
		pthread_detach(thread);
	} while(server_started);
	//
	return (void *) 0;
}

static int
ff_server_init(void *arg) {
	struct sockaddr_in sin;
	struct RTSPConf *conf = rtspconf_global();
	//
	if((server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		perror("socket");
		return -1;
	}
	//
	do {
#ifdef WIN32
		BOOL val = 1;
		setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*) &val, sizeof(val));
#else
		int val = 1;
		setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
#endif
	} while(0);
	//
	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(conf->serverport);
	//
	if(bind(server_socket, (struct sockaddr*) &sin, sizeof(sin)) < 0) {
		perror("bind");
		return -1;
	}
	if(listen(server_socket, 256) < 0) {
		perror("listen");
		return -1;
	}
	return 0;
}

static int
ff_server_start(void *arg) {
	if(pthread_create(&server_tid, NULL, ff_server_main, NULL) != 0) {
		ga_error("start ffmpeg-server failed.\n");
		return -1;
	}
	return 0;
}

static int
ff_server_stop(void *arg) {
	void *x;
	server_started = 0;
	pthread_cancel(server_tid);
	ga_error("wait for ffmpeg-server termination ...\n");
	pthread_join(server_tid, &x);
	return 0;
}

static int
ff_server_deinit(void *arg) {
#ifdef WIN32
	if(server_socket != INVALID_SOCKET)	{ closesocket(server_socket); }
	server_socket = INVALID_SOCKET;
#else
	if(server_socket >= 0)		{ close(server_socket); }
	server_socket = -1;
#endif
	return 0;
}

static int
ff_server_send_packet_1(const char *prefix, void *ctx, int channelId, AVPacket *pkt, int64_t encoderPts, struct timeval *ptv) {
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
ff_server_send_packet(const char *prefix, int channelId, AVPacket *pkt, int64_t encoderPts, struct timeval *ptv) {
	map<void*, void*>::iterator mi;
	pthread_rwlock_rdlock(&cclock);
	for(mi = client_context.begin(); mi != client_context.end(); mi++) {
		ff_server_send_packet_1(prefix, mi->second, channelId, pkt, encoderPts, ptv);
	}
	pthread_rwlock_unlock(&cclock);
	return 0;
}

ga_module_t *
module_load() {
	static ga_module_t m;
	//
	bzero(&m, sizeof(m));
	m.type = GA_MODULE_TYPE_SERVER;
	m.name = strdup("ffmpeg-rtsp-server");
	m.init = ff_server_init;
	m.start = ff_server_start;
	m.stop = ff_server_stop;
	m.deinit = ff_server_deinit;
	m.send_packet = ff_server_send_packet;
	//
	encoder_register_sinkserver(&m);
	//
	return &m;
}

