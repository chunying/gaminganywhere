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

#ifndef __RTSPCONF_H__
#define __RTSPCONF_H__

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#ifdef __cplusplus
}
#endif

#ifndef WIN32
#include <netinet/in.h>
#endif

#include <string>
#include <vector>

#include "ga-common.h"

#define	RTSPCONF_OBJECT_SIZE	128
#define	RTSPCONF_TITLE_SIZE	256
#define	RTSPCONF_DISPLAY_SIZE	16
#define	RTSPCONF_PROTO_SIZE	8
#define	RTSPCONF_CODECNAME_SIZE	8

struct RTSPConf {
	int initialized;
	char object[RTSPCONF_OBJECT_SIZE];
	char title[RTSPCONF_TITLE_SIZE];
	char display[RTSPCONF_DISPLAY_SIZE];
	char *servername;
	struct sockaddr_in sin;
	int serverport;
	char proto;		// transport layer tcp = 6; udp = 17
	// for controller
	int ctrlenable;
	int ctrlport;
	char ctrlproto;		// transport layer tcp = 6; udp = 17
	int sendmousemotion;
	//
	char *video_encoder_name[RTSPCONF_CODECNAME_SIZE+1];
	AVCodec *video_encoder_codec;
	char *video_decoder_name[RTSPCONF_CODECNAME_SIZE+1];
	AVCodec *video_decoder_codec;
	int video_fps;
	int video_renderer_software;	// 0 - use HW renderer, otherwise SW
	//
	char *audio_encoder_name[RTSPCONF_CODECNAME_SIZE+1];
	AVCodec *audio_encoder_codec;
	char *audio_decoder_name[RTSPCONF_CODECNAME_SIZE+1];
	AVCodec *audio_decoder_codec;
	int audio_bitrate;
	int audio_samplerate;
	int audio_channels;	// XXX: AVFrame->channels is int64_t, use with care
	AVSampleFormat audio_device_format;
	int64_t audio_device_channel_layout;
	AVSampleFormat audio_codec_format;
	int64_t audio_codec_channel_layout;
#ifdef ANDROID
	int builtin_video_decoder;
	int builtin_audio_decoder;
#elif defined __APPLE__
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
	int builtin_video_decoder;
	int builtin_audio_decoder;
#endif
#endif
	//std::vector<std::string> *vgo;	// video generic options
	std::vector<std::string> *vso;	// video specific options
};

EXPORT struct RTSPConf * rtspconf_global();
EXPORT int rtspconf_init(struct RTSPConf *conf);
EXPORT int rtspconf_parse(struct RTSPConf *conf);
EXPORT void rtspconf_resolve_server(struct RTSPConf *conf, const char *servername);
//EXPORT int rtspconf_load(const char *filename, struct RTSPConf *conf);
//EXPORT int rtspconf_load_with_URL(const char *filename, struct RTSPConf *conf, const char *rtspURL);

#endif
