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

EXPORT int encoder_pts_sync(int samplerate);
EXPORT int encoder_running();
EXPORT int encoder_register_vencoder(void* (*threadproc)(void *), void *arg);
EXPORT int encoder_register_aencoder(void* (*threadproc)(void *), void *arg);
EXPORT int encoder_register_client(RTSPContext *rtsp);
EXPORT int encoder_unregister_client(RTSPContext *rtsp);

EXPORT int encoder_send_packet(const char *prefix, struct RTSPContext *rtsp, int channelId, AVPacket *pkt, int64_t encoderPts);
EXPORT int encoder_send_packet_all(const char *prefix, int channelId, AVPacket *pkt, int64_t encoderPts);

#endif
