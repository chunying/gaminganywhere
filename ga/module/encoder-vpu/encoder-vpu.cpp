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
#include "encoder-common.h"
#include "rtspconf.h"

#include "ga-common.h"
#include "ga-avcodec.h"
#include "ga-conf.h"
#include "ga-module.h"

#include "dpipe.h"
#include "vpu-common.h"

static int vencoder_initialized = 0;
static int vencoder_started = 0;
static pthread_t vencoder_tid[VIDEO_SOURCE_CHANNEL_MAX];
static vpu_context_t vpu[VIDEO_SOURCE_CHANNEL_MAX];

// specific data for h.264/h.265
static char *_sps[VIDEO_SOURCE_CHANNEL_MAX];
static int _spslen[VIDEO_SOURCE_CHANNEL_MAX];
static char *_pps[VIDEO_SOURCE_CHANNEL_MAX];
static int _ppslen[VIDEO_SOURCE_CHANNEL_MAX];

//#define	PRINT_LATENCY	1
//#define	SAVEFILE	"save.raw"

#ifdef SAVEFILE
static FILE *fout = NULL;
#endif

static int
vencoder_deinit(void *arg) {
	int iid;
	for(iid = 0; iid < video_source_channels(); iid++) {
		if(_sps[iid] != NULL)	free(_sps[iid]);
		if(_pps[iid] != NULL)	free(_pps[iid]);
		vpu_encoder_deinit(&vpu[iid]);
	}
	//
	bzero(_sps, sizeof(_sps));
	bzero(_pps, sizeof(_pps));
	bzero(_spslen, sizeof(_spslen));
	bzero(_ppslen, sizeof(_ppslen));
	bzero(vencoder_tid, sizeof(vencoder_tid));
	//
	vencoder_initialized = 0;
	ga_error("video encoder: VPU deinitialized.\n");
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
		//
		if(vpu_encoder_init(&vpu[iid], outputW, outputH, rtspconf->video_fps, 1,
				ga_conf_mapreadint("video-specific", "b") / 1000,
				ga_conf_mapreadint("video-specific", "g")) < 0)
			goto init_failed;
	}
	vencoder_initialized = 1;
	ga_error("video encoder: initialized (%d channels).\n", iid);
	return 0;
init_failed:
	vencoder_deinit(NULL);
	return -1;
}

/// TODO
static void *
vencoder_threadproc(void *arg) {
	// arg is pointer to source pipename
	int cid;
	vsource_frame_t *frame = NULL;
	char *pipename = (char*) arg;
	dpipe_t *pipe = dpipe_lookup(pipename);
	dpipe_buffer_t *data = NULL;
	struct RTSPConf *rtspconf = NULL;
	//
	long long basePts = -1LL, newpts = 0LL, pts = -1LL, ptsSync = 0LL;
	pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	//
	int outputW, outputH;
	//
	struct timeval pkttv;
#ifdef PRINT_LATENCY
	struct timeval ptv;
#endif
	//
	int video_written = 0;
	//
	if(pipe == NULL) {
		ga_error("video encoder: invalid pipeline specified (%s).\n", pipename);
		goto video_quit;
	}
	//
	rtspconf = rtspconf_global();
	cid = pipe->channel_id;
	outputW = video_source_out_width(cid);
	outputH = video_source_out_height(cid);
	//
	// start encoding
	ga_error("video encoding started: tid=%ld.\n", ga_gettid());
	//
	while(vencoder_started != 0 && encoder_running() > 0) {
		//
		AVPacket pkt;
		unsigned char *enc;
		int encsize;
		struct timeval tv;
		struct timespec to;
		// wait for notification
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
		// encode!
		gettimeofday(&pkttv, NULL);
		enc = vpu_encoder_encode(&vpu[cid], frame->imgbuf, vpu[cid].vpu_framesize, &encsize);
		//
		dpipe_put(pipe, data);
		//
		if(enc == NULL) {
			ga_error("encoder-vpu: encode failed.\n");
			goto video_quit;
		}
		// pts must be monotonically increasing
		if(newpts > pts) {
			pts = newpts;
		} else {
			pts++;
		}
		// send packet
#ifdef SAVEFILE
		if(fout != NULL)
			fwrite(enc, sizeof(char), encsize, fout);
#endif
		pkt.data = enc;
		pkt.size = encsize;
		if(encoder_send_packet("video-encoder", cid, &pkt, pkt.pts, &pkttv) < 0) {
			goto video_quit;
		}
		if(video_written == 0) {
			video_written = 1;
			ga_error("first video frame written (pts=%lld)\n", pts);
		}
#ifdef PRINT_LATENCY		/* print out latency */
		gettimeofday(&ptv, NULL);
		ga_aggregated_print(0x0001, 601, tvdiff_us(&ptv, &frame->timestamp));
#endif
	}
	//
video_quit:
	if(pipe) {
		pipe = NULL;
	}
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
#ifdef SAVEFILE
	if(fout == NULL) {
		fout = fopen(SAVEFILE, "wb");
	}
#endif
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
#ifdef SAVEFILE
	if(fout != NULL) {
		fout = NULL;
	}
#endif
	if(vencoder_started == 0)
		return 0;
	vencoder_started = 0;
	for(iid = 0; iid < video_source_channels(); iid++) {
		pthread_join(vencoder_tid[iid], &ignored);
	}
	ga_error("video encdoer: all stopped (%d)\n", iid);
	return 0;
}

static int
vencoder_get_sps_pps(int iid) {
	const unsigned char *s, *p;
	int ssize, psize;
	//
	if(vencoder_initialized == 0) {
		ga_error("video encoder: get SPS/PPS failed - not initialized?\n");
		return GA_IOCTL_ERR_NOTINITIALIZED;
	}
	if(_sps[iid] != NULL)
		return 0;
	if((s = vpu_encoder_get_h264_sps(&vpu[iid], &ssize)) == NULL) {
		ga_error("video encoder: get SPS failed.\n");
		return GA_IOCTL_ERR_NOTFOUND;
	}
	if((p = vpu_encoder_get_h264_pps(&vpu[iid], &psize)) == NULL) {
		ga_error("video encoder: get PPS failed.\n");
		return GA_IOCTL_ERR_NOTFOUND;
	}
	if((_sps[iid] = (char*) malloc(ssize)) == NULL) {
		ga_error("video encoder: get SPS/PPS failed - alloc sps failed.\n");
		return GA_IOCTL_ERR_NOMEM;
	}
	if((_pps[iid] = (char*) malloc(psize)) == NULL) {
		free(_sps[iid]);
		_sps[iid] = NULL;
		ga_error("video encoder: get SPS/PPS failed - alloc pps failed.\n");
		return GA_IOCTL_ERR_NOMEM;
	}
	bcopy(s, _sps[iid], ssize);
	bcopy(p, _pps[iid], psize);
	_spslen[iid] = ssize;
	_ppslen[iid] = psize;
	//
	ga_error("video encoder: sps=%d; pps=%d\n", _spslen[iid], _ppslen[iid]);
	return 0;
}

static int
vencoder_reconfigure(int iid, ga_ioctl_reconfigure_t *reconf) {
	if(vencoder_initialized == 0) {
		ga_error("video encoder: reconfigure failed - not initialized?\n");
		return GA_IOCTL_ERR_NOTINITIALIZED;
	}
	if(reconf->bitrateKbps > 0
	|| reconf->framerate_n > 0) {
		unsigned int framerate = 0;
		if(reconf->framerate_n > 0 && reconf->framerate_d > 0) {
			framerate = (((reconf->framerate_d-1) & 0x0ffff)<<16) | (reconf->framerate_n & 0x0ffff);
		}
		if(vpu_encoder_reconfigure(&vpu[reconf->id], reconf->bitrateKbps, framerate) < 0) {
			ga_error("video encoder: reconfigure failed.\n");
			return -1;
		}
	}
	return 0;
}

static int
vencoder_ioctl(int command, int argsize, void *arg) {
	int ret = 0;
	ga_ioctl_buffer_t *buf = (ga_ioctl_buffer_t*) arg;
	ga_ioctl_reconfigure_t *reconf = NULL;
	//
	switch(command) {
	case GA_IOCTL_GETSPS:
		if(argsize != sizeof(ga_ioctl_buffer_t))
			return GA_IOCTL_ERR_INVALID_ARGUMENT;
		if((ret = vencoder_get_sps_pps(buf->id)) < 0)
			break;
		if(buf->size < _spslen[buf->id])
			return GA_IOCTL_ERR_BUFFERSIZE;
		buf->size = _spslen[buf->id];
		bcopy(_sps[buf->id], buf->ptr, buf->size);
		break;
	case GA_IOCTL_GETPPS:
		if(argsize != sizeof(ga_ioctl_buffer_t))
			return GA_IOCTL_ERR_INVALID_ARGUMENT;
		if((ret = vencoder_get_sps_pps(buf->id)) < 0)
			break;
		if(buf->size < _spslen[buf->id])
			return GA_IOCTL_ERR_BUFFERSIZE;
		buf->size = _ppslen[buf->id];
		bcopy(_pps[buf->id], buf->ptr, buf->size);
		break;
	case GA_IOCTL_RECONFIGURE:
		reconf = (ga_ioctl_reconfigure_t*) arg;
		if(argsize != sizeof(ga_ioctl_reconfigure_t))
			return GA_IOCTL_ERR_INVALID_ARGUMENT;
		if(vencoder_reconfigure(buf->id, reconf) < 0) {
			ga_error("video encoder: reconfigure to bitrate to %dKbps failed\n", reconf->bitrateKbps);
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
	struct RTSPConf *rtspconf = rtspconf_global();
	//
	bzero(&m, sizeof(m));
	m.type = GA_MODULE_TYPE_VENCODER;
	m.name = strdup("i.MX6-vpu-H.264-encoder");
	m.mimetype = strdup("video/H264");
	m.init = vencoder_init;
	m.start = vencoder_start;
	//m.threadproc = vencoder_threadproc;
	m.stop = vencoder_stop;
	m.deinit = vencoder_deinit;
	//
	m.ioctl = vencoder_ioctl;
	return &m;
}

