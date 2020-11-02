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

#include <stdio.h>
#include <pthread.h>
#ifndef WIN32
#include <unistd.h>
#endif

#include "ga-common.h"
#include "ga-avcodec.h"

using namespace std;

// avcodec_open/close is not thread-safe
static pthread_mutex_t avcodec_open_mutex = PTHREAD_MUTEX_INITIALIZER;

/*struct SwsContext *
ga_swscale_init(PixelFormat format, int inW, int inH, int outW, int outH) {
	struct SwsContext *swsctx = NULL;
	//
	if((swsctx = sws_getContext(
				inW, inH, format,
				outW, outH, PIX_FMT_YUV420P,
				SWS_BICUBIC, NULL, NULL, NULL)) == NULL) {
		fprintf(stderr, "# ga-swscale-init: cannot create swscale context\n");
	}
	return swsctx;
}*/

AVFormatContext*
ga_format_init(const char *filename) {
	AVOutputFormat *fmt;
	AVFormatContext *ctx;
	//
	if((fmt = av_guess_format(NULL, filename, NULL)) == NULL) {
		if((fmt = av_guess_format("mkv", NULL, NULL)) == NULL) {
			fprintf(stderr, "# cannot find suitable format.\n");
			return NULL;
		}
	}
	if((ctx = avformat_alloc_context()) == NULL) {
		fprintf(stderr, "# create avformat context failed.\n");
		return NULL;
	}
	//
	ctx->oformat = fmt;
	snprintf(ctx->filename, sizeof(ctx->filename), "%s", filename);
	//
	if((fmt->flags & AVFMT_NOFILE) == 0) {
		if(avio_open(&ctx->pb, ctx->filename, AVIO_FLAG_WRITE) < 0) {
			fprintf(stderr, "# cannot create file '%s'\n", ctx->filename);
			return NULL;
		}
	}
	//
	return ctx;
}

AVFormatContext*
ga_rtp_init(const char *url) {
	AVOutputFormat *fmt;
	AVFormatContext *ctx;
	//
	if((fmt = av_guess_format("rtp", NULL, NULL)) == NULL) {
		fprintf(stderr, "# rtp is not supported.\n");
		return NULL;
	}
	if((ctx = avformat_alloc_context()) == NULL) {
		fprintf(stderr, "# create avformat context failed.\n");
		return NULL;
	}
	//
	ctx->oformat = fmt;
	snprintf(ctx->filename, sizeof(ctx->filename), "%s", url);
	//
	//if((fmt->flags & AVFMT_NOFILE) == 0) {
		if(avio_open(&ctx->pb, ctx->filename, AVIO_FLAG_WRITE) < 0) {
			fprintf(stderr, "# cannot create file '%s'\n", ctx->filename);
			return NULL;
		}
	//}
	//
	return ctx;
}

AVStream *
ga_avformat_new_stream(AVFormatContext *ctx, int id, AVCodec *codec) {
	AVStream *st = NULL;
	if(codec == NULL)
		return NULL;
	if((st = avformat_new_stream(ctx, codec)) == NULL)
		return NULL;
	// format specific index
	st->id = id;
	//
	if(ctx->flags & AVFMT_GLOBALHEADER) {
		st->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}
	// some codec will need GLOBAL_HEADER to generate ctx->extradata!
	if(codec->id == AV_CODEC_ID_H264 || codec->id == AV_CODEC_ID_AAC) {
		st->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}
	return st;
}

AVCodec*
ga_avcodec_find_encoder(const char **names, enum AVCodecID cid) {
	AVCodec *codec = NULL;
	if(names != NULL) {
		while(*names != NULL) {
			if((codec = avcodec_find_encoder_by_name(*names)) != NULL)
				return codec;
			names++;
		}
	}
	if(cid != AV_CODEC_ID_NONE)
		return avcodec_find_encoder(cid);
	return NULL;
}

AVCodec*
ga_avcodec_find_decoder(const char **names, enum AVCodecID cid) {
	AVCodec *codec = NULL;
	if(names != NULL) {
		while(*names != NULL) {
			if((codec = avcodec_find_decoder_by_name(*names)) != NULL)
				return codec;
			names++;
		}
	}
	if(cid != AV_CODEC_ID_NONE)
		return avcodec_find_decoder(cid);
	return NULL;
}

AVCodecContext*
ga_avcodec_vencoder_init(AVCodecContext *ctx, AVCodec *codec, int width, int height, int fps, vector<string> *vso) {
	AVDictionary *opts = NULL;

	if(codec == NULL) {
		return NULL;
	}
	if(ctx == NULL) {
		if((ctx = avcodec_alloc_context3(codec)) == NULL) {
			return NULL;
		}
	}
	// parameters
	/* always enable GLOBAL HEADER
	 * - required header should be passed via something like
	 * - sprop-parameter-sets in SDP descriptions */
	ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
#ifdef WIN32
	ctx->time_base.num = 1;
	ctx->time_base.den = fps;
#else
	ctx->time_base = (AVRational) {1, fps};
#endif
	ctx->pix_fmt = AV_PIX_FMT_YUV420P;
	ctx->width = width;
	ctx->height = height;

#if 0
        av_dict_set(&opts, "profile", "baseline", 0);
	av_dict_set(&opts, "preset", "superfast", 0);
	av_dict_set(&opts, "tune", "zerolatency", 0);
	av_dict_set(&opts, "intra-refresh", "1", 0);
	av_dict_set(&opts, "slice-max-size", "1500", 0);
#endif
	if(vso != NULL) {
		unsigned i, n = vso->size();
		for(i = 0; i < n; i += 2) {
			av_dict_set(&opts, (*vso)[i].c_str(), (*vso)[i+1].c_str(), 0);
			ga_error("vencoder-init: option %s = %s\n",
				(*vso)[i].c_str(),
				(*vso)[i+1].c_str());
		}
	} else {
		ga_error("vencoder-init: using default video encoder parameter.\n");
	}

	pthread_mutex_lock(&avcodec_open_mutex);
	if(avcodec_open2(ctx, codec, &opts) != 0) {
		avcodec_close(ctx);
		av_free(ctx);
		pthread_mutex_unlock(&avcodec_open_mutex);
		ga_error("vencoder-init: Failed to initialize encoder for codec \"%s\"\n", codec->name);
		return NULL;
	}
	pthread_mutex_unlock(&avcodec_open_mutex);

	return ctx;
}

AVCodecContext*
ga_avcodec_aencoder_init(AVCodecContext *ctx, AVCodec *codec, int bitrate, int samplerate, int channels, AVSampleFormat format, uint64_t chlayout) {
	AVDictionary *opts = NULL;

	if(codec == NULL) {
		return NULL;
	}
	if(ctx == NULL) {
		if((ctx = avcodec_alloc_context3(codec)) == NULL) {
			fprintf(stderr, "# audio-encoder: cannot allocate context\n");
			return NULL;
		}
	}
	// parameters
	ctx->thread_count = 1;
	ctx->bit_rate = bitrate;
	ctx->sample_fmt = format;	//AV_SAMPLE_FMT_S16;
	ctx->sample_rate = samplerate;
	ctx->channels = channels;
	ctx->channel_layout = chlayout;
#ifdef WIN32
	ctx->time_base.num = 1;
	ctx->time_base.den = ctx->sample_rate;
#else
	ctx->time_base = (AVRational) {1, ctx->sample_rate};
#endif

	pthread_mutex_lock(&avcodec_open_mutex);
	if(avcodec_open2(ctx, codec, &opts) != 0) {
		avcodec_close(ctx);
		av_free(ctx);
		pthread_mutex_unlock(&avcodec_open_mutex);
		fprintf(stderr, "# audio-encoder: open codec failed.\n");
		return NULL;
	}
	pthread_mutex_unlock(&avcodec_open_mutex);

	return ctx;
}

void
ga_avcodec_close(AVCodecContext *ctx) {
	if(ctx == NULL)
		return;
	pthread_mutex_lock(&avcodec_open_mutex);
	avcodec_close(ctx);
	pthread_mutex_unlock(&avcodec_open_mutex);
	return;
}

