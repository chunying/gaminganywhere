/*
 * Copyright (c) 2013-2014 Chun-Ying Huang
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

#ifdef __cplusplus
extern "C" {
#endif
#include <x264.h>
#ifdef __cplusplus
}
#endif

static struct RTSPConf *rtspconf = NULL;

static int vencoder_initialized = 0;
static int vencoder_started = 0;
static pthread_t vencoder_tid[VIDEO_SOURCE_CHANNEL_MAX];
static pthread_mutex_t vencoder_reconf_mutex[VIDEO_SOURCE_CHANNEL_MAX];
static ga_ioctl_reconfigure_t vencoder_reconf[VIDEO_SOURCE_CHANNEL_MAX];
//// encoders for encoding
static x264_t* vencoder[VIDEO_SOURCE_CHANNEL_MAX];

// specific data for h.264
static char *_sps[VIDEO_SOURCE_CHANNEL_MAX];
static int _spslen[VIDEO_SOURCE_CHANNEL_MAX];
static char *_pps[VIDEO_SOURCE_CHANNEL_MAX];
static int _ppslen[VIDEO_SOURCE_CHANNEL_MAX];

//#define	SAVEENC	"save.264"
#ifdef SAVEENC
static FILE *fsaveenc = NULL;
#endif

static int
vencoder_deinit(void *arg) {
	int iid;
#ifdef SAVEENC
	if(fsaveenc != NULL) {
		fclose(fsaveenc);
		fsaveenc = NULL;
	}
#endif
	for(iid = 0; iid < video_source_channels(); iid++) {
		if(_sps[iid] != NULL)
			free(_sps[iid]);
		if(_pps[iid] != NULL)
			free(_pps[iid]);
		if(vencoder[iid] != NULL)
			x264_encoder_close(vencoder[iid]);
		pthread_mutex_destroy(&vencoder_reconf_mutex[iid]);
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

static int /* XXX: we need this because many GA config values are in bits, not Kbits */
ga_x264_param_parse_bit(x264_param_t *params, const char *name, const char *bitvalue) {
	int v = strtol(bitvalue, NULL, 0);
	char kbit[64];
	snprintf(kbit, sizeof(kbit), "%d", v / 1000);
	return x264_param_parse(params, name, kbit);
}

static int
vencoder_init(void *arg) {
	int iid;
	char *pipefmt = (char*) arg;
	struct RTSPConf *rtspconf = rtspconf_global();
	char profile[16], preset[16], tune[16];
	char x264params[1024];
	char tmpbuf[64];
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
		x264_param_t params;
		//
		_sps[iid] = _pps[iid] = NULL;
		_spslen[iid] = _ppslen[iid] = 0;
		pthread_mutex_init(&vencoder_reconf_mutex[iid], NULL);
		vencoder_reconf[iid].id = -1;
		//
		snprintf(pipename, sizeof(pipename), pipefmt, iid);
		outputW = video_source_out_width(iid);
		outputH = video_source_out_height(iid);
		if(outputW % 4 != 0 || outputH % 4 != 0) {
			ga_error("video encoder: unsupported resolutin %dx%d\n", outputW, outputH);
			goto init_failed;
		}
		if((pipe = dpipe_lookup(pipename)) == NULL) {
			ga_error("video encoder: pipe %s is not found\n", pipename);
			goto init_failed;
		}
		ga_error("video encoder: video source #%d from '%s' (%dx%d).\n",
			iid, pipe->name, outputW, outputH, iid);
		//
		bzero(&params, sizeof(params));
		x264_param_default(&params);
		// fill params
		preset[0] = tune[0] = '\0';
		ga_conf_mapreadv("video-specific", "preset", preset, sizeof(preset));
		ga_conf_mapreadv("video-specific", "tune", tune, sizeof(tune));
		if(preset[0] != '\0' || tune[0] != '\0') {
			if(x264_param_default_preset(&params, preset, tune) < 0) {
				ga_error("video encoder: bad x264 preset=%s; tune=%s\n", preset, tune);
				goto init_failed;
			} else {
				ga_error("video encoder: x264 preset=%s; tune=%s\n", preset, tune); 
			}
		}
		//
		if(ga_conf_mapreadv("video-specific", "b", tmpbuf, sizeof(tmpbuf)) != NULL)
			ga_x264_param_parse_bit(&params, "bitrate", tmpbuf);
		if(ga_conf_mapreadv("video-specific", "crf", tmpbuf, sizeof(tmpbuf)) != NULL)
			x264_param_parse(&params, "crf", tmpbuf);
		if(ga_conf_mapreadv("video-specific", "vbv-init", tmpbuf, sizeof(tmpbuf)) != NULL)
			x264_param_parse(&params, "vbv-init", tmpbuf);
		if(ga_conf_mapreadv("video-specific", "maxrate", tmpbuf, sizeof(tmpbuf)) != NULL)
			ga_x264_param_parse_bit(&params, "vbv-maxrate", tmpbuf);
		if(ga_conf_mapreadv("video-specific", "bufsize", tmpbuf, sizeof(tmpbuf)) != NULL)
			ga_x264_param_parse_bit(&params, "vbv-bufsize", tmpbuf);
		if(ga_conf_mapreadv("video-specific", "refs", tmpbuf, sizeof(tmpbuf)) != NULL)
			x264_param_parse(&params, "ref", tmpbuf);
		if(ga_conf_mapreadv("video-specific", "me_method", tmpbuf, sizeof(tmpbuf)) != NULL)
			x264_param_parse(&params, "me", tmpbuf);
		if(ga_conf_mapreadv("video-specific", "me_range", tmpbuf, sizeof(tmpbuf)) != NULL)
			x264_param_parse(&params, "merange", tmpbuf);
		if(ga_conf_mapreadv("video-specific", "g", tmpbuf, sizeof(tmpbuf)) != NULL)
			x264_param_parse(&params, "keyint", tmpbuf);
		if(ga_conf_mapreadv("video-specific", "intra-refresh", tmpbuf, sizeof(tmpbuf)) != NULL)
			x264_param_parse(&params, "intra-refresh", tmpbuf);
		//
		x264_param_parse(&params, "bframes", "0");
		x264_param_apply_fastfirstpass(&params);
		if(ga_conf_mapreadv("video-specific", "profile", profile, sizeof(profile)) != NULL) {
			if(x264_param_apply_profile(&params, profile) < 0) {
				ga_error("video encoder: x264 - bad profile %s\n", profile);
				goto init_failed;
			}
		}
		//
		if(ga_conf_readv("video-fps", tmpbuf, sizeof(tmpbuf)) != NULL)
			x264_param_parse(&params, "fps", tmpbuf);
		if(ga_conf_mapreadv("video-specific", "threads", tmpbuf, sizeof(tmpbuf)) != NULL)
			x264_param_parse(&params, "threads", tmpbuf);
		if(ga_conf_mapreadv("video-specific", "slices", tmpbuf, sizeof(tmpbuf)) != NULL)
			x264_param_parse(&params, "slices", tmpbuf);
		//
		params.i_log_level = X264_LOG_INFO;
		params.i_csp = X264_CSP_I420;
		params.i_width  = outputW;
		params.i_height = outputH;
		//params.vui.b_fullrange = 1;
		params.b_repeat_headers = 1;
		params.b_annexb = 1;
		// handle x264-params
		if(ga_conf_mapreadv("video-specific", "x264-params", x264params, sizeof(x264params)) != NULL) {
			char *saveptr, *value;
			char *name = strtok_r(x264params, ":", &saveptr);
			while(name != NULL) {
				if((value = strchr(name, '=')) != NULL) {
					*value++ = '\0';
				}
				if(x264_param_parse(&params, name, value) < 0) {
					ga_error("video encoder: warning - bad x264 param [%s=%s]\n", name, value);
				}
				name = strtok_r(NULL, ":", &saveptr);
			}
		}
		//
		vencoder[iid] = x264_encoder_open(&params);
		if(vencoder[iid] == NULL)
			goto init_failed;
		ga_error("video encoder: opened! bitrate=%dKbps; me_method=%d; me_range=%d; refs=%d; g=%d; intra-refresh=%d; width=%d; height=%d; crop=%d,%d,%d,%d; threads=%d; slices=%d; repeat-hdr=%d; annexb=%d\n",
			params.rc.i_bitrate,
			params.analyse.i_me_method, params.analyse.i_me_range,
			params.i_frame_reference,
			params.i_keyint_max,
			params.b_intra_refresh,
			params.i_width, params.i_height,
			params.crop_rect.i_left, params.crop_rect.i_top,
			params.crop_rect.i_right, params.crop_rect.i_bottom,
			params.i_threads, params.i_slice_count,
			params.b_repeat_headers, params.b_annexb);
	}
#ifdef SAVEENC
	fsaveenc = fopen(SAVEENC, "wb");
#endif
	vencoder_initialized = 1;
	ga_error("video encoder: initialized.\n");
	return 0;
init_failed:
	vencoder_deinit(NULL);
	return -1;
}

static int
vencoder_reconfigure(int iid) {
	int ret = 0;
	x264_param_t params;
	x264_t *encoder = vencoder[iid];
	ga_ioctl_reconfigure_t *reconf = &vencoder_reconf[iid];
	//
	pthread_mutex_lock(&vencoder_reconf_mutex[iid]);
	if(vencoder_reconf[iid].id >= 0) {
		int doit = 0;
		x264_encoder_parameters(encoder, &params);
		//
		if(reconf->crf > 0) {
			params.rc.f_rf_constant = 1.0 * reconf->crf;
			doit++;
		}
		if(reconf->framerate_n > 0) {
			params.i_fps_num = reconf->framerate_n;
			params.i_fps_den = reconf->framerate_d > 0 ? reconf->framerate_d : 1;
			doit++;
		}
		if(reconf->bitrateKbps > 0) {
			// XXX: do not use x264_param_parse("bitrate"), it switches mode to ABR
			// - although mode switching may be not allowed
			params.rc.i_bitrate = reconf->bitrateKbps;
			params.rc.i_vbv_max_bitrate = reconf->bitrateKbps;
			doit++;
		}
		if(reconf->bufsize > 0) {
			params.rc.i_vbv_buffer_size = reconf->bufsize;
			doit++;
		}
		//
		if(doit > 0) {
			if(x264_encoder_reconfig(encoder, &params) < 0) {
				ga_error("video encoder: reconfigure failed. crf=%d; framerate=%d/%d; bitrate=%d; bufsize=%d.\n",
						reconf->crf,
						reconf->framerate_n, reconf->framerate_d,
						reconf->bitrateKbps,
						reconf->bufsize);
				ret = -1;
			} else {
				ga_error("video encoder: reconfigured. crf=%.2f; framerate=%d/%d; bitrate=%d/%dKbps; bufsize=%dKbit.\n",
						params.rc.f_rf_constant,
						params.i_fps_num, params.i_fps_den,
						params.rc.i_bitrate, params.rc.i_vbv_max_bitrate,
						params.rc.i_vbv_buffer_size);
			}
		}
		reconf->id = -1;
	}
	pthread_mutex_unlock(&vencoder_reconf_mutex[iid]);
	return ret;
}

static void *
vencoder_threadproc(void *arg) {
	// arg is pointer to source pipename
	int iid, outputW, outputH;
	vsource_frame_t *frame = NULL;
	char *pipename = (char*) arg;
	dpipe_t *pipe = dpipe_lookup(pipename);
	dpipe_buffer_t *data = NULL;
	x264_t *encoder = NULL;
	//
	long long basePts = -1LL, newpts = 0LL, pts = -1LL, ptsSync = 0LL;
	pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	//
	unsigned char *pktbuf = NULL;
	int pktbufsize = 0, pktbufmax = 0;
	int video_written = 0;
	int64_t x264_pts = 0;
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
	pktbufmax = outputW * outputH * 2;
	if((pktbuf = (unsigned char*) malloc(pktbufmax)) == NULL) {
		ga_error("video encoder: allocate memory failed.\n");
		goto video_quit;
	}
	// start encoding
	ga_error("video encoding started: tid=%ld %dx%d@%dfps.\n",
		ga_gettid(),
		outputW, outputH, rtspconf->video_fps);
	//
	while(vencoder_started != 0 && encoder_running() > 0) {
		x264_picture_t pic_in, pic_out = {0};
		x264_nal_t *nal;
		int i, size, nnal;
		struct timeval tv;
		struct timespec to;
		gettimeofday(&tv, NULL);
		// need reconfigure?
		vencoder_reconfigure(iid);
		// wait for notification
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
		//
		x264_picture_init(&pic_in);
		//
		pic_in.img.i_csp = X264_CSP_I420;
		pic_in.img.i_plane = 3;
		pic_in.img.i_stride[0] = frame->linesize[0];
		pic_in.img.i_stride[1] = frame->linesize[1];
		pic_in.img.i_stride[2] = frame->linesize[2];
		pic_in.img.plane[0] = frame->imgbuf;
		pic_in.img.plane[1] = pic_in.img.plane[0] + outputW*outputH;
		pic_in.img.plane[2] = pic_in.img.plane[1] + ((outputW * outputH) >> 2);
		// pts must be monotonically increasing
		if(newpts > pts) {
			pts = newpts;
		} else {
			pts++;
		}
		//pic_in.i_pts = pts;
		pic_in.i_pts = x264_pts++;
		// encode
		if((size = x264_encoder_encode(encoder, &nal, &nnal, &pic_in, &pic_out)) < 0) {
			ga_error("video encoder: encode failed, err = %d\n", size);
			dpipe_put(pipe, data);
			break;
		}
		dpipe_put(pipe, data);
		// encode
		if(size > 0) {
			AVPacket pkt;
#if 1
			av_init_packet(&pkt);
			pkt.pts = pic_in.i_pts;
			pkt.stream_index = 0;
			// concatenate nals
			pktbufsize = 0;
			for(i = 0; i < nnal; i++) {
				if(pktbufsize + nal[i].i_payload > pktbufmax) {
					ga_error("video encoder: nal dropped (%d < %d).\n", i+1, nnal);
					break;
				}
				bcopy(nal[i].p_payload, pktbuf + pktbufsize, nal[i].i_payload);
				pktbufsize += nal[i].i_payload;
			}
			pkt.size = pktbufsize;
			pkt.data = pktbuf;
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
			// send the packet
			if(encoder_send_packet("video-encoder",
					iid/*rtspconf->video_id*/, &pkt,
					pkt.pts, NULL) < 0) {
				goto video_quit;
			}
#ifdef SAVEENC
			if(fsaveenc != NULL)
				fwrite(pkt.data, sizeof(char), pkt.size, fsaveenc);
#endif
#else
			// handling special nals (type > 5)
			for(i = 0; i < nnal; i++) {
				unsigned char *ptr;
				int offset;
				if((ptr = ga_find_startcode(nal[i].p_payload, nal[i].p_payload + nal[i].i_payload, &offset))
				!= nal[i].p_payload) {
					ga_error("video encoder: no startcode found for nals\n");
					goto video_quit;
				}
				if((*(ptr+offset) & 0x1f) <= 5)
					break;
				av_init_packet(&pkt);
				pkt.pts = pic_in.i_pts;
				pkt.stream_index = 0;
				pkt.size = nal[i].i_payload;
				pkt.data = ptr;
				if(encoder_send_packet("video-encoder",
					iid/*rtspconf->video_id*/, &pkt, pkt.pts, NULL) < 0) {
					goto video_quit;
				}
#ifdef SAVEENC
				if(fsaveenc != NULL)
					fwrite(pkt.data, sizeof(char), pkt.size, fsaveenc);
#endif
			}
			// handling video frame data
			pktbufsize = 0;
			for(; i < nnal; i++) {
				if(pktbufsize + nal[i].i_payload > pktbufmax) {
					ga_error("video encoder: nal dropped (%d < %d).\n", i+1, nnal);
					break;
				}
				bcopy(nal[i].p_payload, pktbuf + pktbufsize, nal[i].i_payload);
				pktbufsize += nal[i].i_payload;
			}
			if(pktbufsize > 0) {
				av_init_packet(&pkt);
				pkt.pts = pic_in.i_pts;
				pkt.stream_index = 0;
				pkt.size = pktbufsize;
				pkt.data = pktbuf;
				if(encoder_send_packet("video-encoder",
					iid/*rtspconf->video_id*/, &pkt, pkt.pts, NULL) < 0) {
					goto video_quit;
				}
#ifdef SAVEENC
				if(fsaveenc != NULL)
					fwrite(pkt.data, sizeof(char), pkt.size, fsaveenc);
#endif
			}
#endif
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
				ga_error("first video frame written (pts=%lld)\n", pic_in.i_pts);
			}
		}
	}
	//
video_quit:
	if(pipe) {
		pipe = NULL;
	}
	if(pktbuf != NULL) {
		free(pktbuf);
	}
	pktbuf = NULL;
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

static int
x264_reconfigure(ga_ioctl_reconfigure_t *reconf) {
	if(vencoder_started == 0 || encoder_running() == 0) {
		ga_error("video encoder: reconfigure - not running.\n");
		return 0;
	}
	pthread_mutex_lock(&vencoder_reconf_mutex[reconf->id]);
	bcopy(reconf, &vencoder_reconf[reconf->id], sizeof(ga_ioctl_reconfigure_t));
	pthread_mutex_unlock(&vencoder_reconf_mutex[reconf->id]);
	return 0;
}

static int
x264_get_sps_pps(int iid) {
	x264_nal_t *p_nal;
	int ret = 0;
	int i, i_nal;
	// alread obtained?
	if(_sps[iid] != NULL)
		return 0;
	//
	if(vencoder_initialized == 0)
		return GA_IOCTL_ERR_NOTINITIALIZED;
	if(x264_encoder_headers(vencoder[iid], &p_nal, &i_nal) < 0)
		return GA_IOCTL_ERR_NOTFOUND;
	for(i = 0; i < i_nal; i++) {
		if(p_nal[i].i_type == NAL_SPS) {
			if((_sps[iid] = (char*) malloc(p_nal[i].i_payload)) == NULL) {
				ret = GA_IOCTL_ERR_NOMEM;
				break;
			}
			bcopy(p_nal[i].p_payload, _sps[iid], p_nal[i].i_payload);
			_spslen[iid] = p_nal[i].i_payload;
		} else if(p_nal[i].i_type == NAL_PPS) {
			if((_pps[iid] = (char*) malloc(p_nal[i].i_payload)) == NULL) {
				ret = GA_IOCTL_ERR_NOMEM;
				break;
			}
			bcopy(p_nal[i].p_payload, _pps[iid], p_nal[i].i_payload);
			_ppslen[iid] = p_nal[i].i_payload;
		}
	}
	//
	if(_sps[iid] == NULL || _pps[iid] == NULL) {
		if(_sps[iid])	free(_sps[iid]);
		if(_pps[iid])	free(_pps[iid]);
		_sps[iid] = _pps[iid] = NULL;
		_spslen[iid] = _ppslen[iid] = 0;
	} else {
		ga_error("video encoder: found sps (%d bytes); pps (%d bytes)\n",
			_spslen[iid], _ppslen[iid]);
	}
	return ret;
}

static int
vencoder_ioctl(int command, int argsize, void *arg) {
	int ret = 0;
	ga_ioctl_buffer_t *buf = (ga_ioctl_buffer_t*) arg;
	//
	if(vencoder_initialized == 0)
		return GA_IOCTL_ERR_NOTINITIALIZED;
	//
	switch(command) {
	case GA_IOCTL_RECONFIGURE:
		if(argsize != sizeof(ga_ioctl_reconfigure_t))
			return GA_IOCTL_ERR_INVALID_ARGUMENT;
		x264_reconfigure((ga_ioctl_reconfigure_t*) arg);
		break;
	case GA_IOCTL_GETSPS:
		if(argsize != sizeof(ga_ioctl_buffer_t))
			return GA_IOCTL_ERR_INVALID_ARGUMENT;
		if(x264_get_sps_pps(buf->id) < 0)
			return GA_IOCTL_ERR_NOTFOUND;
		if(buf->size < _spslen[buf->id])
			return GA_IOCTL_ERR_BUFFERSIZE;
		buf->size = _spslen[buf->id];
		bcopy(_sps[buf->id], buf->ptr, buf->size);
		break;
	case GA_IOCTL_GETPPS:
		if(argsize != sizeof(ga_ioctl_buffer_t))
			return GA_IOCTL_ERR_INVALID_ARGUMENT;
		if(x264_get_sps_pps(buf->id) < 0)
			return GA_IOCTL_ERR_NOTFOUND;
		if(buf->size < _ppslen[buf->id])
			return GA_IOCTL_ERR_BUFFERSIZE;
		buf->size = _ppslen[buf->id];
		bcopy(_pps[buf->id], buf->ptr, buf->size);
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
	//
	bzero(&m, sizeof(m));
	m.type = GA_MODULE_TYPE_VENCODER;
	m.name = strdup("x264-video-encoder");
	m.mimetype = strdup("video/H264");
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

