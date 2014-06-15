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

#ifndef __ENCODER_COMMON_H__
#define __ENCODER_COMMON_H__

#include "rtspserver.h"
#include "ga-common.h"
#include "ga-avcodec.h"
#include "ga-module.h"

#define DISCRETE_FRAMER		/* use discrete framer */

enum GARTSPServerType {
	RTSPSERVER_TYPE_NULL = 0,
	RTSPSERVER_TYPE_FFMPEG,
	RTSPSERVER_TYPE_LIVE
};

// this data structure should be read-only outside this file
typedef struct encoder_packet_s {
	char *data;
	unsigned size;
	int64_t pts_int64;
	struct timeval pts_tv;
	// internal data structure - do not touch
	//int pos;
	int padding;
}	encoder_packet_t;

typedef struct encoder_packet_queue_s {
	pthread_mutex_t mutex;
	char *buf;
	int bufsize, datasize;
	int head, tail;
}	encoder_packet_queue_t;

typedef void (*qcallback_t)(int);

EXPORT int encoder_config_rtspserver(int type);
EXPORT int encoder_pts_sync(int samplerate);
EXPORT int encoder_running();
EXPORT int encoder_register_vencoder(ga_module_t *m, void *param);
EXPORT int encoder_register_aencoder(ga_module_t *m, void *param);
EXPORT ga_module_t *encoder_get_vencoder();
EXPORT ga_module_t *encoder_get_aencoder();
EXPORT int encoder_register_client(void *ctx);
EXPORT int encoder_unregister_client(void *ctx);

EXPORT int encoder_send_packet(const char *prefix, void *ctx, int channelId, AVPacket *pkt, int64_t encoderPts, struct timeval *ptv);
EXPORT int encoder_send_packet_all(const char *prefix, int channelId, AVPacket *pkt, int64_t encoderPts, struct timeval *ptv);

// encoder packet queue - for async packet delivery
EXPORT int encoder_pktqueue_init(int channels, int qsize);
EXPORT int encoder_pktqueue_size(int channelId);
EXPORT int encoder_pktqueue_append(int channelId, AVPacket *pkt, int64_t encoderPts, struct timeval *ptv);
EXPORT char * encoder_pktqueue_front(int channelId, encoder_packet_t *pkt);
EXPORT void encoder_pktqueue_split_packet(int channelId, char *offset);
EXPORT void encoder_pktqueue_pop_front(int channelId);
EXPORT int encoder_pktqueue_register_callback(int channelId, qcallback_t cb);
EXPORT int encoder_pktqueue_unregister_callback(int channelId, qcallback_t cb);

#endif
