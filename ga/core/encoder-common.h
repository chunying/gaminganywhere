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

/**
 * @file
 * Interfaces for bridging encoders and sink servers: the header.
 */

#ifndef __ENCODER_COMMON_H__
#define __ENCODER_COMMON_H__

#include <pthread.h>

#include "ga-common.h"
#include "ga-avcodec.h"
#include "ga-module.h"

/*
 * Packet format for encoder packet queue.
 *
 * This data structure should be read-only outside encoder-common.cpp
 */
typedef struct encoder_packet_s {
	char *data;		/**< Pointer to the data buffer */
	unsigned size;		/**< Size of the buffer */
	int64_t pts_int64;	/**< Packet timestamp in a 64-bit integer */
	struct timeval pts_tv;	/**< Packet timestamp in \a timeval structure */
	// internal data structure - do not touch
	int padding;		/**< Padding area: internal used */
}	encoder_packet_t;

typedef struct encoder_packet_queue_s {
	pthread_mutex_t mutex;	/**< Per-queue mutex */
	char *buf;		/**< Pointer to the packet queue buffer */
	int bufsize;		/**< Size of the queue buffer */
	int datasize;		/**< Size of occupied data size */
	int head;		/**< Position of queue head */
	int tail;		/**< Position of queue tail */
}	encoder_packet_queue_t;

typedef void (*qcallback_t)(int);

EXPORT int encoder_pts_sync(int samplerate);
EXPORT int encoder_running();
EXPORT int encoder_register_vencoder(ga_module_t *m, void *param);
EXPORT int encoder_register_aencoder(ga_module_t *m, void *param);
EXPORT int encoder_register_sinkserver(ga_module_t *m);
EXPORT ga_module_t *encoder_get_vencoder();
EXPORT ga_module_t *encoder_get_aencoder();
EXPORT ga_module_t *encoder_get_sinkserver();
EXPORT int encoder_register_client(void *ctx);
EXPORT int encoder_unregister_client(void *ctx);

EXPORT int encoder_send_packet(const char *prefix, int channelId, AVPacket *pkt, int64_t encoderPts, struct timeval *ptv);

// encoder packet queue - for async packet delivery
EXPORT int encoder_pktqueue_init(int channels, int qsize);
EXPORT int encoder_pktqueue_reset();
EXPORT int encoder_pktqueue_reset_channel(int channelId);
EXPORT int encoder_pktqueue_size(int channelId);
EXPORT int encoder_pktqueue_append(int channelId, AVPacket *pkt, int64_t encoderPts, struct timeval *ptv);
EXPORT char * encoder_pktqueue_front(int channelId, encoder_packet_t *pkt);
EXPORT void encoder_pktqueue_split_packet(int channelId, char *offset);
EXPORT void encoder_pktqueue_pop_front(int channelId);
EXPORT int encoder_pktqueue_register_callback(int channelId, qcallback_t cb);
EXPORT int encoder_pktqueue_unregister_callback(int channelId, qcallback_t cb);

#endif
