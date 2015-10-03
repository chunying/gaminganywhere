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

#include "vsource.h"
#include "rtspconf.h"
#include "encoder-common.h"

#include "ga-common.h"
#include "ga-avcodec.h"
#include "ga-conf.h"
#include "ga-module.h"

#include "dpipe.h"

//// Prevent use of GLOBAL_HEADER to pass parameters, disabled by default
//#define STANDALONE_SDP	1

static struct RTSPConf *rtspconf = NULL;

static int vencoder_initialized = 0;
static int vencoder_started = 0;
static pthread_t vencoder_tid[VIDEO_SOURCE_CHANNEL_MAX];
//// encoders for encoding
static AVCodecContext *vencoder[VIDEO_SOURCE_CHANNEL_MAX];
#ifdef STANDALONE_SDP
//// encoders for generating SDP
/* separate encoder and encoder_sdp because some ffmpeg codecs
 * only generate ctx->extradata when CODEC_FLAG_GLOBAL_HEADER flag
 * is set */
static AVCodecContext *vencoder_sdp[VIDEO_SOURCE_CHANNEL_MAX];
#endif

// specific data for h.264/h.265
static char *_sps[VIDEO_SOURCE_CHANNEL_MAX];
static int _spslen[VIDEO_SOURCE_CHANNEL_MAX];
static char *_pps[VIDEO_SOURCE_CHANNEL_MAX];
static int _ppslen[VIDEO_SOURCE_CHANNEL_MAX];
static char *_vps[VIDEO_SOURCE_CHANNEL_MAX];
static int _vpslen[VIDEO_SOURCE_CHANNEL_MAX];

static int
vencoder_deinit(void *arg) {
	int iid;
	for(iid = 0; iid < video_source_channels(); iid++) {
		if(_sps[iid] != NULL)
			free(_sps[iid]);
		if(_pps[iid] != NULL)
			free(_pps[iid]);
#ifdef STANDALONE_SDP
		if(vencoder_sdp[iid] != NULL)
			ga_avcodec_close(vencoder_sdp[iid]);
#endif
		if(vencoder[iid] != NULL)
			ga_avcodec_close(vencoder[iid]);
#ifdef STANDALONE_SDP
		vencoder_sdp[iid] = NULL;
#endif
		vencoder[iid] = NULL;
	}
	bzero(_sps, sizeof(_sps));
	bzero(_pps, sizeof(_pps));
	bzero(_spslen, sizeof(_spslen));
	bzero(_ppslen, sizeof(_ppslen));
	vencoder_initialized = 0;
	ga_error("video encoder: deinitialized.\n");
	return 0;
}

static int
vencoder_init(void *arg) {
	int iid;
	char *pipefmt = (char*) arg;
	struct RTSPConf *rtspconf = rtspconf_global();
	//
	if(rtspconf == NULL) {
		ga_error("video encoder: no configuration found\n");
		return -1;
	}
	if(vencoder_initialized != 0)
		return 0;
	//
	for(iid = 0; iid < video_source_channels(); iid++) {
		char pipename[64];
		int outputW, outputH;
		dpipe_t *pipe;
		//
		_sps[iid] = _pps[iid] = NULL;
		_spslen[iid] = _ppslen[iid] = 0;
		snprintf(pipename, sizeof(pipename), pipefmt, iid);
		outputW = video_source_out_width(iid);
		outputH = video_source_out_height(iid);
		if((pipe = dpipe_lookup(pipename)) == NULL) {
			ga_error("video encoder: pipe %s is not found\n", pipename);
			goto init_failed;
		}
		ga_error("video encoder: video source #%d from '%s' (%dx%d).\n",
			iid, pipe->name, outputW, outputH, iid);
		vencoder[iid] = ga_avcodec_vencoder_init(NULL,
				rtspconf->video_encoder_codec,
				outputW, outputH,
				rtspconf->video_fps, rtspconf->vso);
		if(vencoder[iid] == NULL)
			goto init_failed;
#ifdef STANDALONE_SDP
		// encoders for SDP generation
		switch(rtspconf->video_encoder_codec->id) {
		case AV_CODEC_ID_H264:
		case AV_CODEC_ID_H265:
		case AV_CODEC_ID_CAVS:
		case AV_CODEC_ID_MPEG4:
			// need ctx with CODEC_FLAG_GLOBAL_HEADER flag
			avc = avcodec_alloc_context3(rtspconf->video_encoder_codec);
			if(avc == NULL)
				goto init_failed;
			avc->flags |= CODEC_FLAG_GLOBAL_HEADER;
			avc = ga_avcodec_vencoder_init(avc,
				rtspconf->video_encoder_codec,
				outputW, outputH,
				rtspconf->video_fps, rtspconf->vso);
			if(avc == NULL)
				goto init_failed;
			ga_error("video encoder: meta-encoder #%d created.\n", iid);
			break;
		default:
			// do nothing
			break;
		}
		vencoder_sdp[iid] = avc;
#endif
	}
	vencoder_initialized = 1;
	ga_error("video encoder: initialized.\n");
	return 0;
init_failed:
	vencoder_deinit(NULL);
	return -1;
}

static void *
vencoder_threadproc(void *arg) {
	// arg is pointer to source pipename
	int iid, outputW, outputH;
	vsource_frame_t *frame = NULL;
	char *pipename = (char*) arg;
	dpipe_t *pipe = dpipe_lookup(pipename);
	dpipe_buffer_t *data = NULL;
	AVCodecContext *encoder = NULL;
	//
	AVFrame *pic_in = NULL;
	unsigned char *pic_in_buf = NULL;
	int pic_in_size;
	unsigned char *nalbuf = NULL, *nalbuf_a = NULL;
	int nalbuf_size = 0, nalign = 0;
	long long basePts = -1LL, newpts = 0LL, pts = -1LL, ptsSync = 0LL;
	pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	//
	int video_written = 0;
	//
	if(pipe == NULL) {
		ga_error("video encoder: invalid pipeline specified (%s).\n", pipename);
		goto video_quit;
	}
	//
	rtspconf = rtspconf_global();
	// init variables
	iid = pipe->channel_id;
	encoder = vencoder[iid];
	//
	outputW = video_source_out_width(iid);
	outputH = video_source_out_height(iid);
	//
	encoder_pts_clear(iid);
	//
	nalbuf_size = 100000+12 * outputW * outputH;
	if(ga_malloc(nalbuf_size, (void**) &nalbuf, &nalign) < 0) {
		ga_error("video encoder: buffer allocation failed, terminated.\n");
		goto video_quit;
	}
	nalbuf_a = nalbuf + nalign;
	//
	if((pic_in = av_frame_alloc()) == NULL) {
		ga_error("video encoder: picture allocation failed, terminated.\n");
		goto video_quit;
	}
	pic_in->width = outputW;
	pic_in->height = outputH;
	pic_in->format = PIX_FMT_YUV420P;
	pic_in_size = avpicture_get_size(PIX_FMT_YUV420P, outputW, outputH);
	if((pic_in_buf = (unsigned char*) av_malloc(pic_in_size)) == NULL) {
		ga_error("video encoder: picture buffer allocation failed, terminated.\n");
		goto video_quit;
	}
	avpicture_fill((AVPicture*) pic_in, pic_in_buf,
			PIX_FMT_YUV420P, outputW, outputH);
	//ga_error("video encoder: linesize = %d|%d|%d\n", pic_in->linesize[0], pic_in->linesize[1], pic_in->linesize[2]);
	// start encoding
	ga_error("video encoding started: tid=%ld %dx%d@%dfps, nalbuf_size=%d, pic_in_size=%d.\n",
		ga_gettid(),
		outputW, outputH, rtspconf->video_fps,
		nalbuf_size, pic_in_size);
	//
	while(vencoder_started != 0 && encoder_running() > 0) {
		AVPacket pkt;
		int got_packet = 0;
		// wait for notification
		struct timeval tv;
		struct timespec to;
		gettimeofday(&tv, NULL);
		to.tv_sec = tv.tv_sec+1;
		to.tv_nsec = tv.tv_usec * 1000;
		data = dpipe_load(pipe, &to);
		if(data == NULL) {
			ga_error("viedo encoder: image source timed out.\n");
			continue;
		}
		frame = (vsource_frame_t*) data->pointer;
		// handle pts
		if(basePts == -1LL) {
			basePts = frame->imgpts;
			ptsSync = encoder_pts_sync(rtspconf->video_fps);
			newpts = ptsSync;
		} else {
			newpts = ptsSync + frame->imgpts - basePts;
		}
		// XXX: assume always YUV420P
		if(pic_in->linesize[0] == frame->linesize[0]
		&& pic_in->linesize[1] == frame->linesize[1]
		&& pic_in->linesize[2] == frame->linesize[2]) {
			bcopy(frame->imgbuf, pic_in_buf, pic_in_size);
		} else {
			ga_error("video encoder: YUV mode failed - mismatched linesize(s) (src:%d,%d,%d; dst:%d,%d,%d)\n",
				frame->linesize[0], frame->linesize[1], frame->linesize[2],
				pic_in->linesize[0], pic_in->linesize[1], pic_in->linesize[2]);
			dpipe_put(pipe, data);
			goto video_quit;
		}
		tv = frame->timestamp;
		dpipe_put(pipe, data);
		// pts must be monotonically increasing
		if(newpts > pts) {
			pts = newpts;
		} else {
			pts++;
		}
		// encode
		encoder_pts_put(iid, pts, &tv);
		pic_in->pts = pts;
		av_init_packet(&pkt);
		pkt.data = nalbuf_a;
		pkt.size = nalbuf_size;
		if(avcodec_encode_video2(encoder, &pkt, pic_in, &got_packet) < 0) {
			ga_error("video encoder: encode failed, terminated.\n");
			goto video_quit;
		}
		if(got_packet) {
			if(pkt.pts == (int64_t) AV_NOPTS_VALUE) {
				pkt.pts = pts;
			}
			pkt.stream_index = 0;
#if 0			// XXX: dump naltype
			do {
				int codelen;
				unsigned char *ptr;
				fprintf(stderr, "[XXX-naldump]");
				for(	ptr = ga_find_startcode(pkt.data, pkt.data+pkt.size, &codelen);
					ptr != NULL;
					ptr = ga_find_startcode(ptr+codelen, pkt.data+pkt.size, &codelen)) {
					//
					fprintf(stderr, " (+%d|%d)-%02x", ptr-pkt.data, codelen, ptr[codelen] & 0x1f);
				}
				fprintf(stderr, "\n");
			} while(0);
#endif
			//
			if(pkt.pts != AV_NOPTS_VALUE) {
				if(encoder_ptv_get(iid, pkt.pts, &tv, 0) == NULL) {
					gettimeofday(&tv, NULL);
				}
			} else {
				gettimeofday(&tv, NULL);
			}
			// send the packet
			if(encoder_send_packet("video-encoder",
				iid/*rtspconf->video_id*/, &pkt,
				pkt.pts, &tv) < 0) {
				goto video_quit;
			}
			// free unused side-data
			if(pkt.side_data_elems > 0) {
				int i;
				for (i = 0; i < pkt.side_data_elems; i++)
					av_free(pkt.side_data[i].data);
				av_freep(&pkt.side_data);
				pkt.side_data_elems = 0;
			}
			//
			if(video_written == 0) {
				video_written = 1;
				ga_error("first video frame written (pts=%lld)\n", pts);
			}
		}
	}
	//
video_quit:
	if(pipe) {
		pipe = NULL;
	}
	//
	if(pic_in_buf)	av_free(pic_in_buf);
	if(pic_in)	av_free(pic_in);
	if(nalbuf)	free(nalbuf);
	//
	ga_error("video encoder: thread terminated (tid=%ld).\n", ga_gettid());
	//
	return NULL;
}

static int
vencoder_start(void *arg) {
	int iid;
	char *pipefmt = (char*) arg;
#define	MAXPARAMLEN	64
	static char pipename[VIDEO_SOURCE_CHANNEL_MAX][MAXPARAMLEN];
	if(vencoder_started != 0)
		return 0;
	vencoder_started = 1;
	for(iid = 0; iid < video_source_channels(); iid++) {
		snprintf(pipename[iid], MAXPARAMLEN, pipefmt, iid);
		if(pthread_create(&vencoder_tid[iid], NULL, vencoder_threadproc, pipename[iid]) != 0) {
			vencoder_started = 0;
			ga_error("video encoder: create thread failed.\n");
			return -1;
		}
	}
	ga_error("video encdoer: all started (%d)\n", iid);
	return 0;
}

static int
vencoder_stop(void *arg) {
	int iid;
	void *ignored;
	if(vencoder_started == 0)
		return 0;
	vencoder_started = 0;
	for(iid = 0; iid < video_source_channels(); iid++) {
		pthread_join(vencoder_tid[iid], &ignored);
	}
	ga_error("video encdoer: all stopped (%d)\n", iid);
	return 0;
}

static void *
vencoder_raw(void *arg, int *size) {
#if defined __APPLE__
	int64_t in = (int64_t) arg;
	int iid = (int) (in & 0xffffffffLL);
#elif defined __x86_64__
	int iid = (long long) arg;
#else
	int iid = (int) arg;
#endif
	if(vencoder_initialized == 0)
		return NULL;
	if(size)
		*size = sizeof(vencoder[iid]);
	return vencoder[iid];
}

/* find startcode: XXX: only 00 00 00 01 - a simplified version */
static unsigned char *
find_startcode(unsigned char *data, unsigned char *end) {
	unsigned char *r;
	for(r = data; r < end - 4; r++) {
		if(r[0] == 0
		&& r[1] == 0
		&& r[2] == 0
		&& r[3] == 1)
			return r;
	}
	return end;
}

static int
h264or5_get_vparam(int type, int channelId, unsigned char *data, int datalen) {
	int ret = -1;
	unsigned char *r;
	unsigned char *sps = NULL, *pps = NULL, *vps = NULL;
	int spslen = 0, ppslen = 0, vpslen = 0;
	if(_sps[channelId] != NULL)
		return 0;
	r = find_startcode(data, data + datalen);
	while(r < data + datalen) {
		unsigned char nal_type;
		unsigned char *r1;
#if 0
		if(sps != NULL && pps != NULL)
			break;
#endif
		while(0 == (*r++))
			;
		r1 = find_startcode(r, data + datalen);
		if(type == 265) {
			nal_type = ((*r)>>1) & 0x3f;
			if(nal_type == 32) {		// VPS
				vps = r;
				vpslen = r1 - r;
			} else if(nal_type == 33) {	// SPS
				sps = r;
				spslen = r1 - r;
			} else if(nal_type == 34) {	// PPS
				pps = r;
				ppslen = r1 - r;
			}
		} else {
			// assume default is 264
			nal_type = *r & 0x1f;
			if(nal_type == 7) {		// SPS
				sps = r;
				spslen = r1 - r;
			} else if(nal_type == 8) {	// PPS
				pps = r;
				ppslen = r1 - r;
			}
		}
		r = r1;
	}
	if(sps != NULL && pps != NULL) {
		// alloc and copy SPS
		if((_sps[channelId] = (char*) malloc(spslen)) == NULL)
			goto error_get_h264or5_vparam;
		_spslen[channelId] = spslen;
		bcopy(sps, _sps[channelId], spslen);
		// alloc and copy PPS
		if((_pps[channelId] = (char*) malloc(ppslen)) == NULL) {
			goto error_get_h264or5_vparam;
		}
		_ppslen[channelId] = ppslen;
		bcopy(pps, _pps[channelId], ppslen);
		// alloc and copy VPS
		if(vps != NULL) {
			if((_vps[channelId] = (char*) malloc(vpslen)) == NULL) {
				goto error_get_h264or5_vparam;
			}
			_vpslen[channelId] = vpslen;
			bcopy(vps, _vps[channelId], vpslen);
		}
		//
		if(type == 265) {
			if(vps == NULL)
				goto error_get_h264or5_vparam;
			ga_error("video encoder: h.265/found sps@%d(%d); pps@%d(%d); vps@%d(%d)\n",
				sps-data, _spslen[channelId],
				pps-data, _ppslen[channelId],
				vps-data, _vpslen[channelId]);
		} else {
			ga_error("video encoder: h.264/found sps@%d(%d); pps@%d(%d)\n",
				sps-data, _spslen[channelId],
				pps-data, _ppslen[channelId]);
		}
		//
		ret = 0;
	}
	return ret;
error_get_h264or5_vparam:
	if(_sps[channelId])	free(_sps[channelId]);
	if(_pps[channelId])	free(_pps[channelId]);
	if(_vps[channelId])	free(_vps[channelId]);
	_sps[channelId]    = _pps[channelId]    = _vps[channelId]    = NULL;
	_spslen[channelId] = _ppslen[channelId] = _vpslen[channelId] = 0;
	return -1;
}

static AVCodecContext *
vencoder_opt_get_encoder(int cid) {
	AVCodecContext *ve = NULL;
	if(vencoder_initialized == 0)
		return NULL;
#ifdef STANDALONE_SDP
	ve = vencoder_sdp[cid] ? vencoder_sdp[cid] : vencoder[cid];
#else
	ve = vencoder[cid];
#endif
	return ve;
}

static int
vencoder_ioctl(int command, int argsize, void *arg) {
	int ret = 0;
	ga_ioctl_buffer_t *buf = (ga_ioctl_buffer_t*) arg;
	AVCodecContext *ve = NULL;
	//
	switch(command) {
	case GA_IOCTL_GETSPS:
	case GA_IOCTL_GETPPS:
	case GA_IOCTL_GETVPS:
		if((ve = vencoder_opt_get_encoder(buf->id)) == NULL)
			return GA_IOCTL_ERR_BADID;
		if(argsize != sizeof(ga_ioctl_buffer_t))
			return GA_IOCTL_ERR_INVALID_ARGUMENT;
		if(ve->extradata_size <= 0)
			return GA_IOCTL_ERR_NOTFOUND;
		if(ve->codec_id != AV_CODEC_ID_H264 && ve->codec_id != AV_CODEC_ID_H265)
			return GA_IOCTL_ERR_NOTSUPPORTED;
		if(ve->codec_id == AV_CODEC_ID_H264 && command == GA_IOCTL_GETVPS)
			return GA_IOCTL_ERR_NOTSUPPORTED;
		if(h264or5_get_vparam(ve->codec_id == AV_CODEC_ID_H264 ? 264 : 265,
				buf->id, ve->extradata, ve->extradata_size) < 0) {
			return GA_IOCTL_ERR_NOTFOUND;
		}
		if(command == GA_IOCTL_GETSPS) {
			if(buf->size < _spslen[buf->id])
				return GA_IOCTL_ERR_BUFFERSIZE;
			buf->size = _spslen[buf->id];
			bcopy(_sps[buf->id], buf->ptr, buf->size);
		} else if(command == GA_IOCTL_GETPPS) {
			if(buf->size < _ppslen[buf->id])
				return GA_IOCTL_ERR_BUFFERSIZE;
			buf->size = _ppslen[buf->id];
			bcopy(_pps[buf->id], buf->ptr, buf->size);
		} else if(command == GA_IOCTL_GETVPS) {
			if(buf->size < _vpslen[buf->id])
				return GA_IOCTL_ERR_BUFFERSIZE;
			buf->size = _vpslen[buf->id];
			bcopy(_vps[buf->id], buf->ptr, buf->size);
		}
		break;
	default:
		ret = GA_IOCTL_ERR_NOTSUPPORTED;
		break;
	}
	return ret;
}

ga_module_t *
module_load() {
	static ga_module_t m;
	//struct RTSPConf *rtspconf = rtspconf_global();
	char mime[64];
	//
	bzero(&m, sizeof(m));
	m.type = GA_MODULE_TYPE_VENCODER;
	m.name = strdup("ffmpeg-video-encoder");
	if(ga_conf_readv("video-mimetype", mime, sizeof(mime)) != NULL) {
		m.mimetype = strdup(mime);
	}
	m.init = vencoder_init;
	m.start = vencoder_start;
	//m.threadproc = vencoder_threadproc;
	m.stop = vencoder_stop;
	m.deinit = vencoder_deinit;
	//
	m.raw = vencoder_raw;
	m.ioctl = vencoder_ioctl;
	return &m;
}

