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

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include "rtspconf.h"

#include "ga-common.h"
#include "ga-conf.h"
#include "ga-avcodec.h"

using namespace std;

#define	RTSP_DEF_OBJECT		"/desktop"
#define	RTSP_DEF_TITLE		"Real-Time Desktop"
#define	RTSP_DEF_DISPLAY	":0"
#define	RTSP_DEF_SERVERPORT	554
#define	RTSP_DEF_PROTO		IPPROTO_UDP

#define	RTSP_DEF_CONTROL_ENABLED	0
#define	RTSP_DEF_CONTROL_PORT		555
#define	RTSP_DEF_CONTROL_PROTO		IPPROTO_TCP
#define	RTSP_DEF_SEND_MOUSE_MOTION	0

#define	RTSP_DEF_VIDEO_CODEC	CODEC_ID_H264
#define	RTSP_DEF_VIDEO_FPS	24

#define	RTSP_DEF_AUDIO_CODEC	CODEC_ID_MP3
#define	RTSP_DEF_AUDIO_BITRATE	128000
#define	RTSP_DEF_AUDIO_SAMPLERATE 44100
#define	RTSP_DEF_AUDIO_CHANNELS	2
#define	RTSP_DEF_AUDIO_DEVICE_FORMAT	AV_SAMPLE_FMT_S16
#define	RTSP_DEF_AUDIO_DEVICE_CH_LAYOUT	AV_CH_LAYOUT_STEREO
#define	RTSP_DEF_AUDIO_CODEC_FORMAT	AV_SAMPLE_FMT_S16
#define	RTSP_DEF_AUDIO_CODEC_CH_LAYOUT	AV_CH_LAYOUT_STEREO

#define	DELIM	" \t\n\r"

static struct RTSPConf globalConf;

struct RTSPConf *
rtspconf_global() {
	return &globalConf;
}

int
rtspconf_init(struct RTSPConf *conf) {
	if(conf == NULL)
		return -1;
	memset(conf, 0, sizeof(struct RTSPConf));
	conf->initialized = 1;
	strncpy(conf->object, RTSP_DEF_OBJECT, RTSPCONF_OBJECT_SIZE);
	strncpy(conf->title, RTSP_DEF_TITLE, RTSPCONF_TITLE_SIZE);
	strncpy(conf->display, RTSP_DEF_DISPLAY, RTSPCONF_DISPLAY_SIZE);
	conf->serverport = RTSP_DEF_SERVERPORT;
	conf->proto = RTSP_DEF_PROTO;
	// controller
	conf->ctrlenable = RTSP_DEF_CONTROL_ENABLED;
	conf->ctrlport = RTSP_DEF_CONTROL_PORT;
	conf->ctrlproto = RTSP_DEF_CONTROL_PROTO;
	conf->sendmousemotion = RTSP_DEF_SEND_MOUSE_MOTION;
	//
	conf->video_fps = RTSP_DEF_VIDEO_FPS;
	conf->audio_bitrate = RTSP_DEF_AUDIO_BITRATE;
	conf->audio_samplerate = RTSP_DEF_AUDIO_SAMPLERATE;
	conf->audio_channels = RTSP_DEF_AUDIO_CHANNELS;
	conf->audio_device_format = RTSP_DEF_AUDIO_DEVICE_FORMAT;
	conf->audio_device_channel_layout = RTSP_DEF_AUDIO_DEVICE_CH_LAYOUT;
	conf->audio_codec_format = RTSP_DEF_AUDIO_CODEC_FORMAT;
	conf->audio_device_channel_layout = RTSP_DEF_AUDIO_CODEC_CH_LAYOUT;
	//
	//conf->vgo = new vector<string>;
	conf->vso = new vector<string>;
	//
	return 0;
}

static int
rtspconf_load_codec(const char *key, const char *value,
	const char **names, AVCodec **codec, AVCodec *(*finder)(const char **, enum AVCodecID)) {
	//
	int idx = 0;
	char buf[1024], *saveptr;
	const char *token;
	//
	strncpy(buf, value, sizeof(buf));
	if((token = strtok_r(buf, DELIM, &saveptr)) == NULL) {
		ga_error("# RTSP[config]: no codecs specified for %s.\n", key);
		return -1;
	}
	do {
		names[idx] = strdup(token);
	} while(++idx<RTSPCONF_CODECNAME_SIZE && (token=strtok_r(NULL, DELIM, &saveptr))!=NULL);
	names[idx] = NULL;
	//
	if((*codec = finder(names, AV_CODEC_ID_NONE)) == NULL) {
		ga_error("# RTSP[config]: no available %s codecs (%s).\n", key, value);
		return -1;
	}
	//
	ga_error("# RTSP[config]: %s = %s (%s)\n", key, (*codec)->name,
		(*codec)->long_name == NULL ? "N/A" : (*codec)->long_name);
	return 0;
}

int
rtspconf_parse(struct RTSPConf *conf) {
	char *ptr, buf[1024];
	int v;
	//
	if(conf == NULL)
		return -1;
	//
	rtspconf_init(conf);
	//
	if((ptr = ga_conf_readv("server-name", buf, sizeof(buf))) != NULL) {
		conf->servername = strdup(ptr);
	}
	//
	if((ptr = ga_conf_readv("base-object", buf, sizeof(buf))) != NULL) {
		strncpy(conf->object, ptr, RTSPCONF_OBJECT_SIZE);
	}
	//
	if((ptr = ga_conf_readv("title", buf, sizeof(buf))) != NULL) {
		strncpy(conf->title, ptr, RTSPCONF_TITLE_SIZE);
	}
	//
	if((ptr = ga_conf_readv("display", buf, sizeof(buf))) != NULL) {
		strncpy(conf->display, ptr, RTSPCONF_DISPLAY_SIZE);
	}
	//
	v = ga_conf_readint("server-port");
	if(v <= 0 || v >= 65536) {
		ga_error("# RTSP[config]: invalid server port %d\n", v);
		return -1;
	}
	conf->serverport = v;
	//
	ptr = ga_conf_readv("proto", buf, sizeof(buf));
	if(ptr == NULL || strcmp(ptr, "tcp") != 0) {
		conf->proto = IPPROTO_UDP;
		ga_error("# RTSP[config]: using 'udp' for RTP flows.\n");
	} else {
		conf->proto = IPPROTO_TCP;
		ga_error("# RTSP[config]: using 'tcp' for RTP flows.\n");
	}
	//
	conf->ctrlenable = ga_conf_readbool("control-enabled", 0);
	//
	if(conf->ctrlenable != 0) {
		//
		v = ga_conf_readint("control-port");
		if(v <= 0 || v >= 65536) {
			ga_error("# RTSP[config]: invalid control port %d\n", v);
			return -1;
		}
		conf->ctrlport = v;
		ga_error("# RTSP[config]: controller port = %d\n", conf->ctrlport);
		//
		ptr = ga_conf_readv("control-proto", buf, sizeof(buf));
		if(ptr == NULL || strcmp(ptr, "tcp") != 0) {
			conf->ctrlproto = IPPROTO_UDP;
			ga_error("# RTSP[config]: controller via 'udp' protocol.\n");
		} else {
			conf->ctrlproto = IPPROTO_TCP;
			ga_error("# RTSP[config]: controller via 'tcp' protocol.\n");
		}
		//
		conf->sendmousemotion  = ga_conf_readbool("control-send-mouse-motion", 1);
	}
	// video-encoder, audio-encoder, video-decoder, and audio-decoder
	if((ptr = ga_conf_readv("video-encoder", buf, sizeof(buf))) != NULL) {
		if(rtspconf_load_codec("video-encoder", ptr,
			(const char**) conf->video_encoder_name,
			&conf->video_encoder_codec,
			ga_avcodec_find_encoder) < 0)
			return -1;
	}
#if 0
	if((ptr = ga_conf_readv("video-decoder", buf, sizeof(buf))) != NULL) {
		if(rtspconf_load_codec("video-decoder", ptr,
			(const char**) conf->video_decoder_name,
			&conf->video_decoder_codec,
			ga_avcodec_find_decoder) < 0)
			return -1;
	}
#endif
	if((ptr = ga_conf_readv("audio-encoder", buf, sizeof(buf))) != NULL) {
		if(rtspconf_load_codec("audio-encoder", ptr,
			(const char**) conf->audio_encoder_name,
			&conf->audio_encoder_codec,
			ga_avcodec_find_encoder) < 0)
			return -1;
	}
#if 0
	if((ptr = ga_conf_readv("audio-decoder", buf, sizeof(buf))) != NULL) {
		if(rtspconf_load_codec("audio-decoder", ptr,
			(const char**) conf->audio_decoder_name,
			&conf->audio_decoder_codec,
			ga_avcodec_find_decoder) < 0)
			return -1;
	}
#endif
	//
	v = ga_conf_readint("video-fps");
	if(v <= 0 || v > 120) {
		ga_error("# RTSP[conf]: video-fps out-of-range %d (valid: 1-120)\n", v);
		return -1;
	}
	conf->video_fps = v;
	//
	ptr = ga_conf_readv("video-renderer", buf, sizeof(buf));
	if(ptr != NULL && strcmp(ptr, "software")==0) {
		conf->video_renderer_software = 1;
	} else {
		conf->video_renderer_software = 0;
	}
	//
	v = ga_conf_readint("audio-bitrate");
	if(v <= 0 || v > 1024000) {
		ga_error("# RTSP[config]: audio-bitrate out-of-range %d (valid: 1-1024000)\n", v);
		return -1;
	}
	conf->audio_bitrate = v;
	//
	v = ga_conf_readint("audio-samplerate");
	if(v <= 0 || v > 1024000) {
		ga_error("# RTSP[config]: audio-samplerate out-of-range %d (valid: 1-1024000)\n", v);
		return -1;
	}
	conf->audio_samplerate = v;
	//
	v = ga_conf_readint("audio-channels");
	if(v < 1) {
		ga_error("# RTSP[config]: audio-channels must be greater than zero (%d).\n", v);
		return -1;
	}
	conf->audio_channels = v;
	//
	if((ptr = ga_conf_readv("audio-device-format", buf, sizeof(buf))) == NULL) {
		ga_error("# RTSP[config]: no audio device format specified.\n");
		return -1;
	}
	if((conf->audio_device_format = av_get_sample_fmt(ptr)) == AV_SAMPLE_FMT_NONE) {
		ga_error("# RTSP[config]: unsupported audio device format '%s'\n", ptr);
		return -1;
	}
	//
	if((ptr = ga_conf_readv("audio-device-channel-layout", buf, sizeof(buf))) == NULL) {
		ga_error("# RTSP[config]: no audio device channel layout specified.\n");
		return -1;
	}
	if((conf->audio_device_channel_layout = av_get_channel_layout(ptr)) == 0) {
		ga_error("# RTSP[config]: unsupported audio device channel layout '%s'\n", ptr);
		return -1;
	}
	//
	if((ptr = ga_conf_readv("audio-codec-format", buf, sizeof(buf))) == NULL) {
		ga_error("# RTSP[config]: no audio codec format specified.\n");
		return -1;
	}
	if((conf->audio_codec_format = av_get_sample_fmt(ptr)) == AV_SAMPLE_FMT_NONE) {
		ga_error("# RTSP[config]: unsupported audio codec format '%s'\n", ptr);
		return -1;
	}
	//
	if((ptr = ga_conf_readv("audio-codec-channel-layout", buf, sizeof(buf))) == NULL) {
		ga_error("# RTSP[config]: no audio codec channel layout specified.\n");
		return -1;
	}
	if((conf->audio_codec_channel_layout = av_get_channel_layout(ptr)) == 0) {
		ga_error("# RTSP[config]: unsupported audio codec channel layout '%s'\n", ptr);
		return -1;
	}
	// LAST: video-specific parameters
	if(ga_conf_mapsize("video-specific") > 0) {
		//
		ga_conf_mapreset("video-specific");
		for(	ptr = ga_conf_mapkey("video-specific", buf, sizeof(buf));
			ptr != NULL;
			ptr = ga_conf_mapnextkey("video-specific", buf, sizeof(buf))) {
			//
			char *val, valbuf[1024];
			val = ga_conf_mapvalue("video-specific", valbuf, sizeof(valbuf));
			if(val == NULL || *val == '\0')
				continue;
			conf->vso->push_back(ptr);
			conf->vso->push_back(val);
			ga_error("# RTSP[config]: video specific option: %s = %s\n",
				ptr, val);
		}
	}
	return 0;
}

void
rtspconf_resolve_server(struct RTSPConf *conf, const char *servername) {
	struct in_addr addr;
	struct hostent *hostEnt;
	if((addr.s_addr = inet_addr(servername)) == INADDR_NONE) {
		if((hostEnt = gethostbyname(servername)) == NULL) {
			bzero(&conf->sin.sin_addr, sizeof(conf->sin.sin_addr));
			return;
		}
		bcopy(hostEnt->h_addr, (char *) &addr.s_addr, hostEnt->h_length);
	}
	conf->sin.sin_addr = addr;
	return;
}

