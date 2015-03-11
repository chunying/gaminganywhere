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
#include <stdlib.h>
#include <pthread.h>
#ifndef WIN32
#include <dlfcn.h>
#include <unistd.h>
#endif

#include "ga-common.h"
#include "ga-conf.h"
#include "ga-module.h"
#include "rtspconf.h"
#include "controller.h"
#include "encoder-common.h"

#include "ga-hook-common.h"
#ifdef WIN32
#include "easyhook.h"
#endif

#include <map>
#include <string>
using namespace std;

#define	BITSPERPIXEL		32
#define	ENCODING_MOD_BASE	2

int vsource_initialized = 0;
int resolution_retrieved = 0;
int game_width = 0;
int game_height = 0;
int encoder_width, encoder_height;
int hook_boost = 0;
int no_default_controller = 0;

int enable_server_rate_control = 1;
int server_token_fill_interval = 41667;
int server_num_token_to_fill = 1;
int server_max_tokens = 2;
int video_fps = 24;

dpipe_t *g_pipe[SOURCES];

static char *ga_root = NULL;

static char *imagepipefmt = "video-%d";
static char *filterpipefmt = "filter-%d";
static char *imagepipe0 = "video-0";
static char *filterpipe0 = "filter-0";
static char *filter_param[] = { imagepipefmt, filterpipefmt };
static char *video_encoder_param = filterpipefmt;
static void *audio_encoder_param = NULL;

static struct gaImage realimage, *image = &realimage;
static struct gaRect *prect = NULL;
static struct gaRect rect;

static ga_module_t *m_filter, *m_vencoder, *m_asource, *m_aencoder, *m_ctrl, *m_server;

int	// should be called only once
vsource_init(int width, int height) {
	int i;
	//
	if(vsource_initialized > 0) {
		return 0;
	}
	//
	if(resolution_retrieved == 0) {
		ga_error("resolution not available.\n");
		return -1;
	}
	//
	image->width = width;
	image->height = height;
	// assume always 4-bytes?
	image->bytes_per_line = (BITSPERPIXEL>>3) * image->width;
#ifdef SOURCES
	do {
		vsource_config_t config[SOURCES];
		bzero(config, sizeof(config));
		for(i = 0; i < SOURCES; i++) {
			//config[i].rtp_id = i;
			config[i].curr_width = image->width;
			config[i].curr_height = image->height;
			config[i].curr_stride = image->bytes_per_line;
		}
		if(video_source_setup_ex(config, SOURCES) < 0) {
			return -1;
		}
	} while(0);
#else
	if(video_source_setup(image->width, image->height, image->bytes_per_line) < 0) {
		return -1;
	}
#endif
	// setup pipelines
	for(i = 0; i < SOURCES; i++) {
		char pipename[64];
		snprintf(pipename, sizeof(pipename), imagepipefmt, i);
		if ((g_pipe[i] = dpipe_lookup(pipename)) == NULL) {
			ga_error("image source hook: cannot find pipeline '%s'\n", pipename);
			return -1;
		}
	}
	//
	vsource_initialized = 1;
	//
	return 0;
}

int
ga_hook_capture_prepared(int width, int height, int check_resolution) {
	if(resolution_retrieved == 0) {
		if(ga_hook_get_resolution(width, height) >= 0) {
			resolution_retrieved = 1;
		}
		return -1;
	}
	//
	if(vsource_initialized == 0)
		return -1;

	if(check_resolution == 0)
		return 0;

	if(game_width != width
	|| game_height != height) {
		ga_error("game width/height mismatched (%dx%d) != (%dx%d)\n",
			width, height, game_width, game_height);
		return -1;
	}

	return 0;
}

void
ga_hook_capture_dupframe(vsource_frame_t *frame) {
	int i;
	for(i = 1; i < SOURCES; i++) {
		dpipe_buffer_t *dupdata;
		vsource_frame_t *dupframe;
		dupdata = dpipe_get(g_pipe[i]);
		dupframe = (vsource_frame_t*) dupdata->pointer;
		//
		vsource_dup_frame(frame, dupframe);
		//
		dpipe_store(g_pipe[i], dupdata);
	}
	return;
}

#ifdef	WIN32
#define	BACKSLASHDIR(fwd, back)	back
#else
#define	BACKSLASHDIR(fwd, back)	fwd
#endif
int
load_modules() {
	char module_path[2048] = "";
	char hook_audio[64] = "";
	//
	snprintf(module_path, sizeof(module_path),
		BACKSLASHDIR("%s/mod/filter-rgb2yuv", "%smod\\filter-rgb2yuv"),
		ga_root);
	if((m_filter = ga_load_module(module_path, "filter_RGB2YUV_")) == NULL)
		return -1;
	//
	snprintf(module_path, sizeof(module_path),
		BACKSLASHDIR("%s/mod/encoder-video", "%smod\\encoder-video"),
		ga_root);
	if((m_vencoder = ga_load_module(module_path, "vencoder_")) == NULL)
		return -1;
	if(ga_conf_readbool("enable-audio", 1) != 0) {
	//////////////////////////
#ifndef __APPLE__
	if(ga_conf_readv("hook-audio", hook_audio, sizeof(hook_audio)) == NULL
	|| hook_audio[0] == '\0') {
	//////////////////////////
	snprintf(module_path, sizeof(module_path),
		BACKSLASHDIR("%s/mod/asource-system", "%smod\\asource-system"),
		ga_root);
	if((m_asource = ga_load_module(module_path, "asource_")) == NULL)
		return -1;
	//////////////////////////
	}
#endif
	snprintf(module_path, sizeof(module_path),
		BACKSLASHDIR("%s/mod/encoder-audio", "%smod\\encoder-audio"),
		ga_root);
	if((m_aencoder = ga_load_module(module_path, "aencoder_")) == NULL)
		return -1;
	//////////////////////////
	}
	if(no_default_controller == 0) {
	snprintf(module_path, sizeof(module_path),
		BACKSLASHDIR("%s/mod/ctrl-sdl", "%smod\\ctrl-sdl"),
		ga_root);
	if((m_ctrl = ga_load_module(module_path, "sdlmsg_replay_")) == NULL)
		return -1;
	}
	//////////////////////////
	snprintf(module_path, sizeof(module_path),
		BACKSLASHDIR("%s/mod/server-live555", "%smod\\server-live555"),
		ga_root);
	if((m_server = ga_load_module(module_path, "live555_")) == NULL)
		return -1;
	//////////////////////////
	return 0;
}

int
init_modules() {
	struct RTSPConf *conf = rtspconf_global();
	static const char *filterpipe[] =  { imagepipe0, filterpipe0 };
	char hook_audio[64] = "";
	//
	if(conf->ctrlenable && no_default_controller==0) {
		if(ga_init_single_module("controller", m_ctrl, (void*) prect) < 0) {
			ga_error("******** Init controller module failed, controller disabled.\n");
			conf->ctrlenable = 0;
			no_default_controller = 1;
		}
	}
	// controller server is built-in - no need to init
	ga_init_single_module_or_quit("filter", m_filter, (void*) filter_param);
	//
	ga_init_single_module_or_quit("video-encoder", m_vencoder, filterpipefmt);
	if(ga_conf_readbool("enable-audio", 1) != 0) {
	//////////////////////////
#ifndef __APPLE__
	if(ga_conf_readv("hook-audio", hook_audio, sizeof(hook_audio)) == NULL
	|| hook_audio[0] == '\0') {
		ga_init_single_module_or_quit("audio-source", m_asource, NULL);
	}
#endif
	ga_init_single_module_or_quit("audio-encoder", m_aencoder, NULL);
	//////////////////////////
	}
	ga_init_single_module_or_quit("rtsp-server", m_server, NULL);
	return 0;
}

int
run_modules() {
	struct RTSPConf *conf = rtspconf_global();
	static const char *filterpipe[] =  { imagepipe0, filterpipe0 };
	char hook_audio[64] = "";
	// controller server is built-in, but replay is a module
	if(conf->ctrlenable) {
		ga_run_single_module_or_quit("control server", ctrl_server_thread, conf);
		if(no_default_controller == 0) {
			// XXX: safe to comment out?
			//ga_run_single_module_or_quit("control replayer", m_ctrl->threadproc, conf);
		}
	}
	// video
	//ga_run_single_module_or_quit("filter 0", m_filter->threadproc, (void*) filterpipe);
	if(m_filter->start(filter_param) < 0)	exit(-1);
	encoder_register_vencoder(m_vencoder, video_encoder_param);
	// audio
	if(ga_conf_readbool("enable-audio", 1) != 0) {
	//////////////////////////
#ifndef __APPLE__
	if(ga_conf_readv("hook-audio", hook_audio, sizeof(hook_audio)) == NULL
	|| hook_audio[0] == '\0') {
		//ga_run_single_module_or_quit("audio source", m_asource->threadproc, NULL);
		if(m_asource->start(NULL) < 0)	exit(-1);
	}
#endif
	encoder_register_aencoder(m_aencoder, audio_encoder_param);
	//////////////////////////
	}
	// server
	if(m_server->start(NULL) < 0)	exit(-1);
	//
	return 0;
}

void *
ga_server(void *arg) {
	//
	do {
		usleep(100000);
	} while(resolution_retrieved == 0);
	//
	ga_error("[ga_server] load modules and run the server\n");
	//
	if(vsource_init(encoder_width, encoder_height) < 0) {
		ga_error("[ga_server] video source init failed.\n");
		return NULL;
	}
	//
	prect = NULL;
	//
	if(ga_crop_window(&rect, &prect) < 0) {
		return NULL;
	} else if(prect == NULL) {
		ga_error("*** Crop disabled.\n");
	} else if(prect != NULL) {
		ga_error("*** Crop enabled: (%d,%d)-(%d,%d)\n", 
			prect->left, prect->top,
			prect->right, prect->bottom);
	}
	// load server modules
	if(load_modules() < 0)	 { return NULL; }
	if(init_modules() < 0)	 { return NULL; }
	if(run_modules() < 0)	 { return NULL; }
	//
	//rtspserver_main(NULL);
	//liveserver_main(NULL);
	while(1) {
		usleep(5000000);
	}
	//
	ga_deinit();
	//
	return NULL;
}

int
ga_hook_get_resolution(int width, int height) {
	//
	int resolution[2];
	//
	if(game_width <= 0 || game_height <= 0) {
		game_width = width;
		game_height = height;
		// tune resolution?
		if(ga_conf_readints("max-resolution", resolution, 2) == 2) {
			encoder_width = resolution[0];
			encoder_height = resolution[1];
		} else {
			encoder_width = (game_width / ENCODING_MOD_BASE) * ENCODING_MOD_BASE;
			encoder_height = (game_height / ENCODING_MOD_BASE) * ENCODING_MOD_BASE;
		}
		//
		ga_error("detected resolution: game %dx%d; encoder %dx%d\n",
			game_width, game_height, encoder_width, encoder_height);
		return 0;
	}
	//
	if(width == game_width && height == game_height) {
		if(ga_conf_readints("max-resolution", resolution, 2) == 2) {
			encoder_width = resolution[0];
			encoder_height = resolution[1];
		} else {
			encoder_width = (game_width / ENCODING_MOD_BASE) * ENCODING_MOD_BASE;
			encoder_height = (game_height / ENCODING_MOD_BASE) * ENCODING_MOD_BASE;
		}
		ga_error("resolution fitted: game %dx%d; encoder %dx%d\n",
			game_width, game_height, encoder_width, encoder_height);
		return 0;
	} else {
		ga_error("resolution not fitted (%dx%d)\n", width, height);
	}
	//
	return -1;
}

// token bucket rate controller
int
ga_hook_video_rate_control() {
	static int initialized = 0;
	static int max_tokens;
	static long long tokens = 0LL;
#ifdef WIN32
	static LARGE_INTEGER lastCounter, freq;
	LARGE_INTEGER currCounter;
#else
	static struct timeval lastCounter;
	struct timeval currCounter;
#endif
	long long delta;
	// init
	if(initialized == 0) {
		tokens = 0LL;
		max_tokens = server_max_tokens * server_token_fill_interval;
#ifdef WIN32
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&lastCounter);
#else
		gettimeofday(&lastCounter, NULL);
#endif
		ga_error("[token_bucket] interval=%d, fill=%d, max=%d (%d)\n",
			(int) server_token_fill_interval,
			(int) server_num_token_to_fill,
			(int) server_max_tokens,
			(int) max_tokens);
		initialized = 1;
		return -1;
	}
	//
#ifdef WIN32
	QueryPerformanceCounter(&currCounter);
	delta = pcdiff_us(currCounter, lastCounter, freq);
#else
	gettimeofday(&currCounter, NULL);
	delta = tvdiff_us(&currCounter, &lastCounter);
#endif
	if(delta >= server_token_fill_interval) {
		tokens += delta;
		lastCounter = currCounter;
	}
	if(tokens > max_tokens) {
		tokens = max_tokens;
	}
	if(tokens >= server_token_fill_interval) {
		tokens -= server_token_fill_interval;
		return 1;
	}
	return -1;
}

int
ga_hook_init() {
	char *ptr, hook_method[64];
	int resolution[2], out_resolution[2];
	char *confpath;
/*#ifdef WIN32
	extern char g_root[1024];
	extern char g_confpath[1024];
	extern char g_appexe[1024];
	//
	confpath = g_confpath;
	ga_root = strdup(g_root);
#else*/
	if((confpath = getenv("GA_ROOT")) == NULL) {
		ga_error("GA_ROOT not set.\n");
		return -1;
	}
	ga_root = strdup(confpath);
	//
	if((confpath = getenv("GA_CONFIG")) == NULL) {
		ga_error("GA_CONFIG not set.\n");
		return -1;
	}
//#endif
	//
	if(ga_init(confpath, NULL) < 0)
		return -1;
	//
	ga_openlog();
	//
	if(rtspconf_parse(rtspconf_global()) < 0)
		return -1;
	// handle ga-hook specific configurations
	if((ptr = ga_conf_readv("hook-method", hook_method, sizeof(hook_method))) != NULL) {
		ga_error("*** hook method specified: %s\n", hook_method);
	} else {
		hook_method[0] = '\0';
	}
	if(ga_conf_readbool("hook-experimental", 0) != 0) {
		hook_boost = 1;
		ga_error("*** hook experimental codes enabled.\n");
	}
	if(ga_conf_readints("game-resolution", resolution, 2) == 2) {
		game_width = resolution[0];
		game_height = resolution[1];
		ga_error("*** demanded resolution = %dx%d\n", game_width, game_height);
		ctrl_server_set_resolution(game_width, game_height);
		ctrl_server_set_output_resolution(game_width, game_height);
	}
	//
	if(ga_conf_readints("output-resolution", out_resolution, 2) == 2) {
		ga_error("*** output resolution = %dx%d\n",
				out_resolution[0], out_resolution[1]);
		ctrl_server_set_output_resolution(out_resolution[0], out_resolution[1]);
	}
	//
	enable_server_rate_control = ga_conf_readbool("enable-server-rate-control", 0);
	server_token_fill_interval = ga_conf_readint("server-token-fill-interval");
	server_num_token_to_fill = ga_conf_readint("server-num-token-to-fill");
	server_max_tokens = ga_conf_readint("server-max-tokens");
	video_fps = ga_conf_readint("video-fps");
	//
	// XXX: check for valid configurations
	//
	return 0;
}

#ifndef WIN32
void *
ga_hook_lookup(void *handle, const char *name) {
	void *sym = dlsym(handle, name);
	return sym;
}

void *
ga_hook_lookup_or_quit(void *handle, const char *name) {
	void *sym = ga_hook_lookup(handle, name);
	if(sym == NULL) {
		ga_error("cannot find %s\n", name);
		exit(-1);
	}
	return sym;
}
#endif

#ifdef WIN32
static map<string, TRACED_HOOK_HANDLE> hookdb;

void
ga_hook_function(const char *id, void *oldfunc, void *newfunc) {
	unsigned long thread_ids[] = { GA_HOOK_INVALID_THREADID };
	TRACED_HOOK_HANDLE h = NULL;
	//
	if(hookdb.find(id) != hookdb.end()) {
		ga_error("[ga-hook] %s already hooked? please check.\n", id);
		return;
	}
	//
	if((h = (TRACED_HOOK_HANDLE) malloc(sizeof(HOOK_TRACE_INFO))) == NULL) {
		ga_error("[ga-hook] alloc HOOK_TRACE_INFO failed.\n");
		exit(-1);
	}
	memset(h, 0, sizeof(HOOK_TRACE_INFO));
	//
	if(LhInstallHook(oldfunc, newfunc, NULL, h) != 0) {
		ga_error("[ga-hook] hook for function %s failed.\n", id);
		exit(-1);
	}
	//
	if(LhSetExclusiveACL(thread_ids, 1, h) != 0) {
		ga_error("[ga-hook] cannot activate hook (%s)\n", id);
		exit(-1);
	}
	//
	hookdb[id] = h;
	//
	return;
}
#endif	/* WIN32 */

