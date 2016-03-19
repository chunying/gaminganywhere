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

#ifndef __GA_AVCODEC_H__
#define __GA_AVCODEC_H__

#ifdef __cplusplus
extern "C" {
#endif
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/base64.h>
#ifdef __cplusplus
}
#endif

#ifndef SWR_CH_MAX
#define	SWR_CH_MAX	32
#endif

#include <map>
#include <string>
#include <vector>

EXPORT AVFormatContext* ga_format_init(const char *filename);
EXPORT AVFormatContext* ga_rtp_init(const char *url);
EXPORT AVStream* ga_avformat_new_stream(AVFormatContext *ctx, int id, AVCodec *codec);
EXPORT AVCodec* ga_avcodec_find_encoder(const char **names, enum AVCodecID cid = AV_CODEC_ID_NONE);
EXPORT AVCodec* ga_avcodec_find_decoder(const char **names, enum AVCodecID cid = AV_CODEC_ID_NONE);
EXPORT AVCodecContext*	ga_avcodec_vencoder_init(AVCodecContext *ctx, AVCodec *codec, int width, int height, int fps, std::vector<std::string> *vso = NULL);
EXPORT AVCodecContext*	ga_avcodec_aencoder_init(AVCodecContext *ctx, AVCodec *codec, int bitrate, int samplerate, int channels, AVSampleFormat format, uint64_t chlayout);
EXPORT void ga_avcodec_close(AVCodecContext *ctx);

#endif
