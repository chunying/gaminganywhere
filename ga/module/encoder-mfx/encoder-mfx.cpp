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
#include <mfxvideo.h>

#include "vsource.h"
#include "rtspconf.h"
#include "encoder-common.h"

#include "ga-common.h"
#include "ga-avcodec.h"
#include "ga-conf.h"
#include "ga-module.h"

#include "dpipe.h"
#include "allocator.h"
#include "mfx-common.h"

static int mfx_initialized = 0;
static int mfx_started = 0;
static pthread_t mfx_tid[VIDEO_SOURCE_CHANNEL_MAX];

// specific data for h.264/h.265
static char *_sps[VIDEO_SOURCE_CHANNEL_MAX];
static int _spslen[VIDEO_SOURCE_CHANNEL_MAX];
static char *_pps[VIDEO_SOURCE_CHANNEL_MAX];
static int _ppslen[VIDEO_SOURCE_CHANNEL_MAX];

//#define	SAVEENC	"save.264"
#ifdef SAVEENC
static FILE *fsaveenc = NULL;
#endif

static int
mfx_deinit(void *arg) {
	int iid;
	for(iid = 0; iid < video_source_channels(); iid++) {
		if(_sps[iid] != NULL)	free(_sps[iid]);
		if(_pps[iid] != NULL)	free(_pps[iid]);
		mfx_deinit_internal(iid);
	}
	//
	bzero(_sps, sizeof(_sps));
	bzero(_pps, sizeof(_pps));
	bzero(_spslen, sizeof(_spslen));
	bzero(_ppslen, sizeof(_ppslen));
	bzero(mfx_tid, sizeof(mfx_tid));
	//
	mfx_initialized = 0;
	ga_error("video encoder: MediaSDK deinitialized.\n");
	return 0;
}

static int
mfx_init(void *arg) {
	int iid;
	int RGBmode;
	char *pipefmt = (char*) arg;
	struct RTSPConf *rtspconf = rtspconf_global();
	//
	if(rtspconf == NULL) {
		ga_error("video encoder: no configuration found\n");
		return -1;
	}
	if(mfx_initialized != 0)
		return 0;
	// RGB mode?
	RGBmode = ga_conf_readint("encoder-rgb-mode");
	if(RGBmode <= 0)
		RGBmode = 0;
	//
	for(iid = 0; iid < video_source_channels(); iid++) {
		char pipename[64];
		int outputW, outputH;
		int bitrate;
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
		bitrate = ga_conf_mapreadint("video-specific", "b");
		if(bitrate <= 0)
			bitrate = 3000000;
		bitrate /= 1000;
		if(mfx_init_internal(iid, outputW, outputH, rtspconf->video_fps, bitrate, RGBmode) < 0)
			goto init_failed;
	}
	mfx_initialized = 1;
	ga_error("video encoder: initialized (%d channels).\n", iid);
	return 0;
init_failed:
	mfx_deinit(NULL);
	return -1;
}

static void *
mfx_threadproc(void *arg) {
	// arg is pointer to source pipename
	int cid;
	int RGBmode;
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
	mfxStatus sts = MFX_ERR_NONE;
	mfxSyncPoint vppsync = NULL;
	mfxSyncPoint encsync = NULL;
	mfxFrameInfo vppdefinfo;
	unsigned char startcode[] = { 0, 0, 0, 1 };
	unsigned long long timeunit;
	unsigned long long lastTimeStamp = (unsigned long long) -1LL;
	struct timeval pkttv;
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
	timeunit = 90000 / rtspconf->video_fps;	/* in 90KHz */
	// RGB mode?
	RGBmode = ga_conf_readint("encoder-rgb-mode");
	if(RGBmode <= 0)
		RGBmode = 0;
	//
	memset(&vppdefinfo, 0, sizeof(vppdefinfo));
	vppdefinfo.PicStruct		= MFX_PICSTRUCT_PROGRESSIVE;
	if(RGBmode == 0) {
		vppdefinfo.FourCC	= MFX_FOURCC_YV12;	// the only difference
		vppdefinfo.ChromaFormat	= MFX_CHROMAFORMAT_YUV420;
	} else {
		vppdefinfo.FourCC	= MFX_FOURCC_RGB4;	// the only difference
		vppdefinfo.ChromaFormat	= MFX_CHROMAFORMAT_YUV444;
	}
	vppdefinfo.Width		= MFX_ALIGN16(outputW);
	vppdefinfo.Height		= MFX_ALIGN16(outputH);
	vppdefinfo.CropW		= outputW;
	vppdefinfo.CropH		= outputH;
	vppdefinfo.FrameRateExtN	= rtspconf->video_fps;
	vppdefinfo.FrameRateExtD	= 1;
	// start encoding
	ga_error("video encoding started: tid=%ld %dx%d (%dx%d) at %dfps, async=%d, buffer=%dKB.\n",
		ga_gettid(),
		_encparam[cid].mfx.FrameInfo.CropW,
		_encparam[cid].mfx.FrameInfo.CropH,
		_encparam[cid].mfx.FrameInfo.Width,
		_encparam[cid].mfx.FrameInfo.Height,
		rtspconf->video_fps,
		_encparam[cid].AsyncDepth,
		_encparam[cid].mfx.BufferSizeInKB);
	//
	while(mfx_started != 0 && encoder_running() > 0
		&& (sts >= MFX_ERR_NONE || sts == MFX_ERR_MORE_DATA)) {
		//
		mfxFrameSurface1 *svppin, *svppout;
		AVPacket pkt;
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
		// get a surface
		svppin  = frame_pool_get(_vpppool[cid][0], &_vppresponse[cid][0]);
		svppout = frame_pool_get(_vpppool[cid][1], &_vppresponse[cid][1]);
		if(svppin == NULL || svppout == NULL) {
			dpipe_put(pipe, data);
			ga_error("video encoder: frame dropped - no surface available (%p, %p)\n", svppin, svppout);
			continue;
			//break;
		}
		// handle pts
		if(basePts == -1LL) {
			basePts = frame->imgpts;
			ptsSync = encoder_pts_sync(rtspconf->video_fps);
			newpts = ptsSync;
		} else {
			newpts = ptsSync + frame->imgpts - basePts;
		}
		//
		if(fa_lock(NULL, svppin->Data.MemId, &svppin->Data) != MFX_ERR_NONE) {
			ga_error("video encoder: Unable to lock VPP frame\n");
			break;
		}
		// fill frame info
		memcpy(&svppin->Info,  &vppdefinfo, sizeof(mfxFrameInfo));
		memcpy(&svppout->Info, &vppdefinfo, sizeof(mfxFrameInfo));
		//
		svppin->Data.TimeStamp = timeunit * frame->imgpts;
		// vpp-out is always NV12
		svppout->Info.FourCC = MFX_FOURCC_NV12;
		svppout->Info.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
		//
		if(RGBmode == 0) {
			// YUV420P
			mfxU8 *dst;
			unsigned char *src = frame->imgbuf;
			int i, w2 = outputW>>1, h2 = outputH>>1, p2 = svppin->Data.Pitch/2;
			// copy the frame
			if(svppin->Data.Pitch < frame->linesize[0]
			|| svppin->Data.Pitch / 2 < frame->linesize[1]
			|| svppin->Data.Pitch / 2 < frame->linesize[2]) {
				ga_error("video encoder: YUV/ERROR - surface->Pitch (%d,%d,%d) < frame->linesize (%d,%d,%d)\n",
					svppin->Data.Pitch, svppin->Data.Pitch/2, svppin->Data.Pitch/2,
					frame->linesize[0], frame->linesize[1], frame->linesize[2]);
				exit(-1);
				break;
			}
			// Copy Y
			for(dst = svppin->Data.Y, i = 0; i < outputH; i++) {
				memcpy(dst, src, outputW);
				dst += svppin->Data.Pitch;
				src += frame->linesize[0]; 
			}
			// Copy U
			for(dst = svppin->Data.U, i = 0; i < h2; i++) {
				memcpy(dst, src, w2);
				dst += p2;
				src += frame->linesize[1];
			}
			// Copy V
			for(dst = svppin->Data.V, i = 0; i < h2; i++) {
				memcpy(dst, src, w2);
				dst += p2;
				src += frame->linesize[2];
			}
		} else {
			// RGB
			mfxU8 *dst;
			unsigned char *src = frame->imgbuf;
			int i;
			// copy the frame
			if(svppin->Data.Pitch < frame->linesize[0]) {
				ga_error("video encoder: RGB/ERROR - surface->Pitch (%d) < frame->linesize (%d)\n",
					svppin->Data.Pitch, frame->linesize[0]);
				exit(-1);
				break;
			}
			// Copy
#define MSDK_MIN(a, b)	((a) < (b) ? (a) : (b))
			for(dst = MSDK_MIN(MSDK_MIN(svppin->Data.R, svppin->Data.G), svppin->Data.B), i = 0;
					i < outputH; i++) {
				memcpy(dst, src, MSDK_MIN(svppin->Data.Pitch, frame->linesize[0]));
				dst += svppin->Data.Pitch;
				src += frame->linesize[0]; 
			}
#undef MSDK_MIN
		}
		//
		if(fa_unlock(NULL, svppin->Data.MemId, &svppin->Data) != MFX_ERR_NONE) {
			ga_error("video encoder: Unable to unlock VPP frame\n");
			break;
		}
		dpipe_put(pipe, data);
		// pts must be monotonically increasing
		if(newpts > pts) {
			pts = newpts;
		} else {
			pts++;
		}
		// do VPP
		sts = mfx_encode_vpp(_session[cid], svppin, svppout, &vppsync);
		// VPP errors?
		if(sts == MFX_ERR_MORE_DATA)	continue;
		if(sts == MFX_ERR_MORE_SURFACE)	continue;
		if(sts != MFX_ERR_NONE) {
			mfx_invalid_status(sts);
			ga_error("video encoder: VPP failed.\n");
			break;
		}
		//// wait for VPP finish - seems not necessary
		//MFXVideoCORE_SyncOperation(_session[cid], vppsync, MFX_INFINITE);
		// do ENCODE
		sts = mfx_encode_encode(_session[cid], svppout, &_mfxbs[cid], &encsync);
		//
		if(sts == MFX_ERR_MORE_DATA)	continue;
		if(sts != MFX_ERR_NONE) {
			mfx_invalid_status(sts);
			ga_error("video encoder: encode failed.\n");
			break;
		}
		// send packet
		MFXVideoCORE_SyncOperation(_session[cid], encsync, MFX_INFINITE);
		if(_mfxbs[cid].TimeStamp != lastTimeStamp) {
			lastTimeStamp = _mfxbs[cid].TimeStamp;
			gettimeofday(&pkttv, NULL);
		}
#ifdef SAVEENC
		if(fsaveenc != NULL)
			fwrite(_mfxbs[cid].Data, sizeof(char), _mfxbs[cid].DataLength, fsaveenc);
#endif
#if 1
		unsigned char *ptr, *nextptr;
		int offset, nextoffset;
		if(_mfxbs[cid].Data == NULL) {
			//_mfxbs[cid].DataLength = _mfxbs[cid].DataOffset = 0;
			continue;
		}
		if((ptr = ga_find_startcode(	_mfxbs[cid].Data+_mfxbs[cid].DataOffset,
						_mfxbs[cid].Data+_mfxbs[cid].DataOffset+_mfxbs[cid].DataLength,
						&offset))
		!= _mfxbs[cid].Data+_mfxbs[cid].DataOffset) {
			ga_error("video encoder: cannot find a startcode.\n");
			goto video_quit;
		}
		while(ptr != NULL) {
			nextptr = ga_find_startcode(ptr+4, _mfxbs[cid].Data+_mfxbs[cid].DataOffset+_mfxbs[cid].DataLength, &nextoffset);
			// is a non-frame data nal?
			if((*(ptr + offset) & 0x1f) > 5) {
				av_init_packet(&pkt);
				pkt.data = ptr;
				pkt.size = nextptr != NULL ?
						(nextptr - ptr) :
						(_mfxbs[cid].Data+_mfxbs[cid].DataOffset+_mfxbs[cid].DataLength-ptr);
				if(encoder_send_packet("video-encoder", cid, &pkt, pkt.pts, &pkttv) < 0) {
					goto video_quit;
				}
				ptr = nextptr;
				offset = nextoffset;
				continue;
			}
			// handling frame-data: send all in one shot
			av_init_packet(&pkt);
			pkt.data = ptr;
			pkt.size = _mfxbs[cid].Data+_mfxbs[cid].DataOffset+_mfxbs[cid].DataLength-ptr;
			if(encoder_send_packet("video-encoder", cid, &pkt, pkt.pts, &pkttv) < 0) {
				goto video_quit;
			}
			video_written = 1;
			break;
		}
#else
		if(_mfxbs[cid].Data) {
			pkt.data = _mfxbs[cid].Data + _mfxbs[cid].DataOffset;
			pkt.size = _mfxbs[cid].DataLength;
			if(encoder_send_packet("video-encoder", cid, &pkt, pkt.pts, &pkttv) < 0) {
				goto video_quit;
			}
		}
#endif
		_mfxbs[cid].DataLength = _mfxbs[cid].DataOffset = 0;
		if(video_written == 0) {
			video_written = 1;
			ga_error("first video frame written (pts=%lld)\n", pts);
		}
	}
	//
	while(sts >= MFX_ERR_NONE) {
		sts = mfx_encode_encode(_session[cid], NULL, &_mfxbs[cid], &encsync);
	}
	//
video_quit:
	if(pipe) {
		pipe = NULL;
	}
	//
	//MFXVideoENCODE_Close(session[cid]);
	//
	ga_error("video encoder: thread terminated (tid=%ld).\n", ga_gettid());
	//
	return NULL;
}

static int
mfx_start(void *arg) {
	int iid;
	char *pipefmt = (char*) arg;
#define	MAXPARAMLEN	64
	static char pipename[VIDEO_SOURCE_CHANNEL_MAX][MAXPARAMLEN];
#ifdef SAVEENC
	if(fsaveenc == NULL) {
		fsaveenc = fopen(SAVEENC, "wb");
	}
#endif
	if(mfx_started != 0)
		return 0;
	mfx_started = 1;
	for(iid = 0; iid < video_source_channels(); iid++) {
		snprintf(pipename[iid], MAXPARAMLEN, pipefmt, iid);
		if(pthread_create(&mfx_tid[iid], NULL, mfx_threadproc, pipename[iid]) != 0) {
			mfx_started = 0;
			ga_error("video encoder: create thread failed.\n");
			return -1;
		}
	}
	ga_error("video encdoer: all started (%d)\n", iid);
	return 0;
}

static int
mfx_stop(void *arg) {
	int iid;
	void *ignored;
#ifdef SAVEENC
	if(fsaveenc != NULL) {
		fsaveenc = NULL;
	}
#endif
	if(mfx_started == 0)
		return 0;
	mfx_started = 0;
	for(iid = 0; iid < video_source_channels(); iid++) {
		pthread_join(mfx_tid[iid], &ignored);
	}
	ga_error("video encdoer: all stopped (%d)\n", iid);
	return 0;
}

static void
mfx_dump_buffer(const char *prefix, unsigned char *buf, int buflen) {
	char output[2048];
	int size = 0, wlen;
	// head
	wlen = snprintf(output+size, sizeof(output)-size, "%s [", prefix);
	size += wlen;
	// numbers
	while(buflen > 0) {
		wlen = snprintf(output+size, sizeof(output)-size, " %02x", *buf++);
		size += wlen;
		buflen--;
	}
	// tail
	wlen = snprintf(output+size, sizeof(output)-size, " ]\n");
	ga_error(output);
	return;
}

static int
mfx_get_sps_pps(int iid) {
	void *ret = NULL;
	char buf1[1024], buf2[1024];
	mfxVideoParam param;
	mfxExtCodingOptionSPSPPS spspps;
	mfxExtBuffer *buffer[] = { (mfxExtBuffer*) &spspps };
	mfxStatus sts;
	//
	if(mfx_initialized == 0) {
		ga_error("video encoder: get sps/pps failed - not initialized?\n");
		return GA_IOCTL_ERR_NOTINITIALIZED;
	}
	if(_sps[iid] != NULL)
		return 0;
	bzero(&param, sizeof(param));
	bzero(&spspps, sizeof(spspps));
	bzero(buf1, sizeof(buf1));
	bzero(buf2, sizeof(buf2));
	param.NumExtParam = 1;
	param.ExtParam = buffer;
	spspps.Header.BufferId = MFX_EXTBUFF_CODING_OPTION_SPSPPS;
	spspps.Header.BufferSz = sizeof(spspps);
	spspps.SPSBuffer = (mfxU8*) buf1;
	spspps.PPSBuffer = (mfxU8*) buf2;
	spspps.SPSBufSize = sizeof(buf1);
	spspps.PPSBufSize = sizeof(buf2);
	if((sts = MFXVideoENCODE_GetVideoParam(_session[iid], &param)) != MFX_ERR_NONE) {
		mfx_invalid_status(sts);
		ga_error("video encoder: get sps/pps failed\n");
		return GA_IOCTL_ERR_NOTFOUND;
	}
	if((_sps[iid] = (char*) malloc(spspps.SPSBufSize)) == NULL) {
		ga_error("video encoder: get sps/pps failed - alloc sps failed.\n");
		return GA_IOCTL_ERR_NOMEM;
	}
	if((_pps[iid] = (char*) malloc(spspps.PPSBufSize)) == NULL) {
		free(_sps[iid]);
		_sps[iid] = NULL;
		ga_error("video encoder: get sps/pps failed - alloc pps failed.\n");
		return GA_IOCTL_ERR_NOMEM;
	}
	bcopy(spspps.SPSBuffer, _sps[iid], spspps.SPSBufSize);
	bcopy(spspps.PPSBuffer, _pps[iid], spspps.PPSBufSize);
	_spslen[iid] = spspps.SPSBufSize;
	_ppslen[iid] = spspps.PPSBufSize;
	//
	ga_error("video encoder: sps=%d; pps=%d\n", _spslen[iid], _ppslen[iid]);
	mfx_dump_buffer("video encoder: sps = ", (unsigned char*) _sps[iid], _spslen[iid]);
	mfx_dump_buffer("video encoder: pps = ", (unsigned char*) _pps[iid], _ppslen[iid]);
	return 0;
}

static int
mfx_ioctl(int command, int argsize, void *arg) {
	int ret = 0;
	ga_ioctl_buffer_t *buf = (ga_ioctl_buffer_t*) arg;
	ga_ioctl_reconfigure_t *reconf = (ga_ioctl_reconfigure_t*) arg;
	//
	switch(command) {
	case GA_IOCTL_GETSPS:
		if(argsize != sizeof(ga_ioctl_buffer_t))
			return GA_IOCTL_ERR_INVALID_ARGUMENT;
		if((ret = mfx_get_sps_pps(buf->id)) < 0)
			break;
		if(buf->size < _spslen[buf->id])
			return GA_IOCTL_ERR_BUFFERSIZE;
		buf->size = _spslen[buf->id];
		bcopy(_sps[buf->id], buf->ptr, buf->size);
		break;
	case GA_IOCTL_GETPPS:
		if(argsize != sizeof(ga_ioctl_buffer_t))
			return GA_IOCTL_ERR_INVALID_ARGUMENT;
		if((ret = mfx_get_sps_pps(buf->id)) < 0)
			break;
		if(buf->size < _spslen[buf->id])
			return GA_IOCTL_ERR_BUFFERSIZE;
		buf->size = _ppslen[buf->id];
		bcopy(_pps[buf->id], buf->ptr, buf->size);
		break;
	case GA_IOCTL_RECONFIGURE:
		if(argsize != sizeof(ga_ioctl_reconfigure_t))
			return GA_IOCTL_ERR_INVALID_ARGUMENT;
		if(reconf->bitrateKbps <= 0)
			break;
		if(mfx_reconfigure(_session[reconf->id], reconf->bitrateKbps) == MFX_ERR_NONE) {
			ga_error("video encoder: reconfigure to bitrate to %dKbps OK\n", reconf->bitrateKbps);
		} else {
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
	m.name = strdup("intel-mediasdk-video-encoder");
	m.mimetype = strdup("video/H264");
	m.init = mfx_init;
	m.start = mfx_start;
	//m.threadproc = mfx_threadproc;
	m.stop = mfx_stop;
	m.deinit = mfx_deinit;
	//
	m.ioctl = mfx_ioctl;
	return &m;
}

