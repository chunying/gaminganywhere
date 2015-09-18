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

#ifndef __ANDROID_DECODERS_H__
#define	__ANDROID_DECODERS_H__

#include "rtspclient.h"

// Ref: http://developer.android.com/reference/android/media/MediaCodec.html#BUFFER_FLAG_SYNC_FRAME
#define BUFFER_FLAG_SYNC_FRAME		0x01
#define BUFFER_FLAG_CODEC_CONFIG	0x02
#define BUFFER_FLAG_END_OF_STREAM	0x04

int android_prepare_audio(RTSPThreadParam *rtspParam, const char *mime, bool builtinDecoder);
int android_decode_audio(RTSPThreadParam *rtspParam, unsigned char *buffer, int bufsize, struct timeval pts);
int android_config_h264_sprop(RTSPThreadParam *rtspParam, const char *sprop);
int android_decode_h264(RTSPThreadParam *rtspParam, unsigned char *buffer, int bufsize, struct timeval pts, bool marker);
int android_decode_vp8(RTSPThreadParam *rtspParam, unsigned char *buffer, int bufsize, struct timeval pts, bool marker);

#endif

