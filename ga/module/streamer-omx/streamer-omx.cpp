/*
 * Copyright (c) 2013-2015 Chun-Ying Huang
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
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "vsource.h"
#include "encoder-common.h"
#include "rtspconf.h"

#include "ga-common.h"
#include "ga-avcodec.h"
#include "ga-conf.h"
#include "ga-module.h"
#include "controller.h"
#include "ctrl-sdl.h"

#include "omx-streamer.h"

static int vsource_initialized = 0;
static int vencoder_initialized = 0;
static int vencoder_started = 0;
static pthread_t vencoder_tid[VIDEO_SOURCE_CHANNEL_MAX];
static omx_streamer_t omxe[VIDEO_SOURCE_CHANNEL_MAX];

// specific data for h.264/h.265
static char *_sps[VIDEO_SOURCE_CHANNEL_MAX];
static int _spslen[VIDEO_SOURCE_CHANNEL_MAX];
static char *_pps[VIDEO_SOURCE_CHANNEL_MAX];
static int _ppslen[VIDEO_SOURCE_CHANNEL_MAX];

//#define	SAVEFILE	"save.raw"

#ifdef SAVEFILE
static FILE *fout = NULL;
#endif

#define	PHOTO_PREFIX	"IMG_"
#define	PHOTO_EXT	"JPG"

static char *photo_dir = "/tmp";
static int photo_seq = 1;

static int
init_image_storage() {
	DIR *dir;
	struct dirent d, *pd;
	int pfxlen = strlen(PHOTO_PREFIX);
	int maxseq = 0;
	char path[128];
	//
	if(ga_conf_readv("photo_dir", path, sizeof(path)) != NULL) {
		photo_dir = strdup(path);
	}
	//
	if((dir = opendir(photo_dir)) == NULL) {
		ga_error("video encoder: cannot open image storage (%s).\n", photo_dir);
		return -1;
	}
	while(readdir_r(dir, &d, &pd) == 0) {
		char *ptr;
		int seq = 0;
		if(pd == NULL)
			break;
		if(strncmp(d.d_name, PHOTO_PREFIX, pfxlen) != 0)
			continue;
		ptr = d.d_name + pfxlen;
		while(*ptr && isdigit(*ptr)) {
			seq *= 10;
			seq += *ptr - '0';
			ptr++;
		}
		if(*ptr++ != '.')
			continue;
		if(strcmp(ptr, PHOTO_EXT) != 0)
			continue;
		if(seq > maxseq)
			maxseq = seq;
	}
	closedir(dir);
	//
	photo_seq = maxseq + 1;
	ga_error("video encoder: photo image stored from sequenuce #%d\n", photo_seq);
	return 0;
}

static void
camera_control(void *msg, int msglen) {
	sdlmsg_mouse_t *m = (sdlmsg_mouse_t*) msg;
	if(vencoder_initialized == 0)
		return;
	if(msglen != ntohs(m->msgsize)) {
		ga_error("video source: ctrl message length mismatched. (%d != %d)\n",
			msglen, ntohs(m->msgsize));
		return;
	}
	//
	// XXX: all are in char-type, no endian conversion is required
	if(m->msgtype != SDL_EVENT_MSGTYPE_MOUSEKEY)
		return;
	if(m->mousebutton != 1)
		return;
	if(m->is_pressed == 0) {
		// do camera shot: call /usr/bin/raspistill -n -w 2592 -h 1944 -q 95 -e jpg -o $file
		char filename[256];
		char cmd[512];
		struct stat s;
		snprintf(filename, sizeof(filename), "%s/%s%04d.%s",
			photo_dir, PHOTO_PREFIX, photo_seq++, PHOTO_EXT);
		snprintf(cmd, sizeof(cmd),
			"/usr/bin/raspistill -n -w 2592 -h 1944 -q 95 -e jpg -o %s", filename);
		if(omx_streamer_suspend(&omxe[0]) < 0) {
			ga_error("video encoder: suspend failed.\n");
			return;
		}
		//
		ga_error("video encoder: taking camera shot ... (%s)\n", cmd);
		system(cmd);
		if(stat(filename, &s) == 0 && s.st_size > 0) {
			ga_error("video encoder: done (%s: %d bytes saved).\n", filename, s.st_size);
		} else {
			ga_error("video encoder: camera shot failed.\n");
		}
		//
		if(omx_streamer_resume(&omxe[0]) < 0) {
			ga_error("video encoder: resume failed.\n");
			return;
		}
	}
	return;
}

static int
vencoder_deinit(void *arg) {
	int iid;
	for(iid = 0; iid < 1/*video_source_channels()*/; iid++) {
		if(_sps[iid] != NULL)	free(_sps[iid]);
		if(_pps[iid] != NULL)	free(_pps[iid]);
		omx_streamer_deinit(&omxe[iid]);
	}
	//
	bzero(_sps, sizeof(_sps));
	bzero(_pps, sizeof(_pps));
	bzero(_spslen, sizeof(_spslen));
	bzero(_ppslen, sizeof(_ppslen));
	bzero(vencoder_tid, sizeof(vencoder_tid));
	//
	vencoder_initialized = 0;
	ga_error("video encoder: OMX deinitialized.\n");
	return 0;
}

static int
gaomx_load_int(const char *name, int min, int max, int def) {
	char *ptr, value[64];
	int rv;
	if((ptr = ga_conf_readv(name, value, sizeof(value))) == NULL)
		return def;
	rv = strtol(ptr, NULL, 0);
	if(rv < min || rv > max)
		return def;
	return rv;
}

static OMX_BOOL
gaomx_load_bool(const char *name, OMX_BOOL def) {
	char *ptr, value[64];
	if(ga_conf_readbool(name, def == OMX_TRUE ? 1 : 0) == 0)
		return OMX_FALSE;
	return OMX_TRUE;
}

static int
vencoder_init(void *arg) {
	int iid, width, height, fps, bitrate, gopsize;
	int vsmode[3];
	char *pipefmt = (char*) arg;
	struct RTSPConf *rtspconf = rtspconf_global();
	omx_streamer_config_t sc;
	//
	if(rtspconf == NULL) {
		ga_error("video encoder: no configuration found\n");
		return -1;
	}
	if(vencoder_initialized != 0)
		return 0;
	//
	iid = 0;
	//
	if(ga_conf_readints("video-source-v4l-mode", vsmode, 3) == 3) {
		ga_error("video encoder: use user config: %d %d %d\n",
				vsmode[0], vsmode[1], vsmode[2]);
		width = vsmode[1];
		height = vsmode[2];
	} else {
		width = 640;
		height = 480;
	}
	if((fps = ga_conf_readint("video-fps")) <= 1) {
		fps = 24;
	}
	if((bitrate = ga_conf_mapreadint("video-specific", "b")) < 1000000) {
		bitrate = 3000000;
	}
	if((gopsize = ga_conf_mapreadint("video-specific", "g")) < 5) {
		gopsize = fps;
	}
	// search for photo seq
	if(init_image_storage() < 0) {
		return -1;
	}
	// register a dummy video source: only once
	if(vsource_initialized == 0) {
		vsource_config_t config;
		bzero(&config, sizeof(config));
		config.curr_width = width;
		config.curr_height = height;
		config.curr_stride = width;
		if(video_source_setup_ex(&config, 1) < 0) {
			ga_error("video encoder: setup dummy source failed (%dx%d)\n", width, height);
			return -1;
		}
		ga_error("video encoder: dummy source configured (%dx%d)\n", width, height);
		vsource_initialized = 1;
	}
	//
	ga_error("video encoder: mode=(ignored) (%dx%d), fps=%d\n",
		width, height, fps);
	// load configs
	bzero(&sc, sizeof(sc));
	sc.camera_sharpness = gaomx_load_int("omx-camera-sharpness", -100, 100, OSCAM_DEF_SHARPNESS);
	sc.camera_contrast = gaomx_load_int("omx-camera-contrast", -100, 100, OSCAM_DEF_CONTRAST);
	sc.camera_brightness = gaomx_load_int("omx-camera-brightness", 0, 100, OSCAM_DEF_BRIGHTNESS);
	sc.camera_saturation = gaomx_load_int("omx-camera-saturation", -100, 100, OSCAM_DEF_SATURATION);
	sc.camera_ev = gaomx_load_int("omx-camera-ev", -10, 10, OSCAM_DEF_EXPOSURE_VALUE_COMPENSATION);
	sc.camera_iso = gaomx_load_int("omx-camera-iso", 100, 800, OSCAM_DEF_EXPOSURE_ISO_SENSITIVITY);
	sc.camera_iso_auto = gaomx_load_bool("omx-camera-iso-auto", OSCAM_DEF_EXPOSURE_AUTO_SENSITIVITY);
	sc.camera_frame_stabilisation = gaomx_load_bool("omx-camera-frame-stabilisation", OSCAM_DEF_FRAME_STABILISATION);
	sc.camera_flip_horizon = gaomx_load_bool("omx-camera-flip-horizon", OSCAM_DEF_FLIP_HORIZONTAL);
	sc.camera_flip_vertical = gaomx_load_bool("omx-camera-flip-vertical", OSCAM_DEF_FLIP_VERTICAL);
	sc.camera_whitebalance =
		(enum OMX_WHITEBALCONTROLTYPE) gaomx_load_int("camera-omx-whitebalance", 0, (int) OMX_WhiteBalControlMax, (int) OSCAM_DEF_WHITE_BALANCE_CONTROL);
	sc.camera_filter =
		(enum OMX_IMAGEFILTERTYPE) gaomx_load_int("camera-omx-filter", (int) OMX_ImageFilterNone, (int) OMX_ImageFilterMax, (int) OSCAM_DEF_IMAGE_FILTER);
	//
	if(omx_streamer_init(&omxe[iid], NULL, width, height, fps, 1, bitrate, gopsize) < 0) {
		ga_error("video encoder: init failed.\n");
		return -1;
	}
	// register a dummy control
	ctrl_server_setreplay(camera_control);
	ga_error("video encoder: dummy control enabled.\n");
	//
	vencoder_initialized = 1;
	ga_error("video encoder: initialized (%d channels).\n", iid+1);
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
	struct RTSPConf *rtspconf = NULL;
	//
	long long basePts = -1LL, newpts = 0LL, pts = -1LL, ptsSync = 0LL;
	pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	//
	struct timeval pkttv;
	//
	int video_written = 0;
	//
	rtspconf = rtspconf_global();
	cid = 0;
	// start encoding
	if(omx_streamer_start(&omxe[cid]) < 0) {
		ga_error("video encoder: start streamer failed.\n");
		goto video_quit;
	}
	//
	ga_error("video encoding started: tid=%ld.\n", ga_gettid());
	//
	while(vencoder_started != 0 && encoder_running() > 0) {
		//
		AVPacket pkt;
		unsigned char *enc;
		int encsize;
		// handle pts
		if(basePts == -1LL) {
			basePts = omxe[cid].frame_out;
			ptsSync = encoder_pts_sync(rtspconf->video_fps);
			newpts = ptsSync;
		} else {
			newpts = ptsSync + omxe[cid].frame_out - basePts;
		}
		//
		if((enc = omx_streamer_get(&omxe[cid], &encsize)) == NULL) {
			ga_error("video encoder: encode failed.\n");
			break;
		}
		if(encsize == 0)
			continue;
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
#if 0		// XXX: data check
		do {
			unsigned char *ptr, *nextptr;
			int offset, nextoffset, left = encsize;
			ga_error("XXX: %d bytes encoded ---------------\n", encsize);
			if((ptr = ga_find_startcode(enc, enc+encsize, &offset)) != enc) {
				ga_error("XXX: cannot find a start code\n");
				break;
			}
			while(ptr != NULL) {
				int nalsize;
				nextptr = ga_find_startcode(ptr+4, enc+encsize, &nextoffset);
				nalsize = nextptr != NULL ? nextptr-ptr : enc+encsize-ptr;
				ga_error("XXX: nal_t=%d, size=%d\n", ptr[offset] & 0x1f, nalsize);
				ptr = nextptr;
				offset = nextoffset;
			}
		} while(0);
#endif
		pkt.data = enc;
		pkt.size = encsize;
		if(encoder_send_packet("video-encoder", cid, &pkt, pkt.pts, NULL/*&pkttv*/) < 0) {
			goto video_quit;
		}
		if(video_written == 0) {
			video_written = 1;
			ga_error("first video frame written (pts=%lld)\n", pts);
		}
	}
	//
video_quit:
	//
	if(omx_streamer_prepare_stop(&omxe[cid]) < 0) {
		ga_error("streamer: prepare stop failed.\n");
	}
	//
	if(omx_streamer_stop(&omxe[cid]) < 0) {
		ga_error("streamer: start streamer failed.\n");
	}
	//
	ga_error("video encoder: thread terminated (tid=%ld).\n", ga_gettid());
	//
	return NULL;
}

static int
vencoder_start(void *arg) {
	int iid;
#ifdef SAVEFILE
	if(fout == NULL) {
		fout = fopen(SAVEFILE, "wb");
	}
#endif
	if(vencoder_started != 0)
		return 0;
	vencoder_started = 1;
	for(iid = 0; iid < 1/*video_source_channels()*/; iid++) {
		if(pthread_create(&vencoder_tid[iid], NULL, vencoder_threadproc, NULL) != 0) {
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
	for(iid = 0; iid < 1/*video_source_channels()*/; iid++) {
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
	if((s = omx_streamer_get_h264_sps(&omxe[iid], &ssize)) == NULL) {
		ga_error("video encoder: get SPS failed.\n");
		return GA_IOCTL_ERR_NOTFOUND;
	}
	if((p = omx_streamer_get_h264_pps(&omxe[iid], &psize)) == NULL) {
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
		} else if(reconf->framerate_n > 0) {
			framerate = reconf->framerate_n & 0x0ffff;
		}
		if(omx_streamer_reconfigure(&omxe[reconf->id], reconf->bitrateKbps, framerate, reconf->width, reconf->height) < 0) {
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
			ret = -1;
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
	m.name = strdup("Broadcom-VideoCore-H.264-encoder");
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

