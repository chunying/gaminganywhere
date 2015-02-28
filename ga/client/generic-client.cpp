/*
 * Copyright (c) 2012-2015 Chun-Ying Huang
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

#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>

#include "ga-common.h"
#include "ga-conf.h"
#include "vconverter.h"
#include "rtspconf.h"
#include "rtspclient.h"
#include "controller.h"
#include "ctrl-sdl.h"

#include "generic-client.h"

// exported configurations
static struct RTSPConf *g_conf = NULL;
static struct RTSPThreadParam rtspThreadParam;
static pthread_t ctrlthread;
static pthread_t rtspthread;

// watchdog variables
pthread_mutex_t watchdogMutex;
struct timeval watchdogTimer = {0LL, 0LL};

static void
ga_client_reset_configuration() {
	int i;
	if(g_conf == NULL) {
		g_conf = rtspconf_global();
		bzero(g_conf, sizeof(struct RTSPConf));
	}
	if(g_conf->servername != NULL)
		free(g_conf->servername);
	for(i = 0; i < RTSPCONF_CODECNAME_SIZE+1; i++) {
		if(g_conf->audio_encoder_name[i] != NULL)
			free(g_conf->audio_encoder_name[i]);
		if(g_conf->video_encoder_name[i] != NULL)
			free(g_conf->video_encoder_name[i]);
	}
	if(g_conf->video_decoder_codec != NULL) {
	}
	if(g_conf->audio_decoder_codec != NULL) {
	}
	if(g_conf->vso != NULL)
		delete g_conf->vso;
	bzero(g_conf, sizeof(struct RTSPConf));
	rtspconf_init(g_conf);
	// default android configuration
	g_conf->audio_device_format = AV_SAMPLE_FMT_S16;
	g_conf->audio_device_channel_layout = AV_CH_LAYOUT_STEREO;
	//
	return;
}

int
ga_client_init() {
	srand((unsigned) time(0));
	//
	av_register_all();
	avcodec_register_all();
	avformat_network_init();
	//
	ga_client_reset_configuration();
	//
	return 0;
}

int
ga_client_set_host(const char *host) {
	if(host == NULL) {
		ga_log("set_host: no host given.\n");
		return -1;
	}
	if(g_conf->servername != NULL)
		free(g_conf->servername);
	g_conf->servername = strdup(host);
	ga_log("set_host: %s\n", g_conf->servername);
	return 0;
}

int
ga_client_set_port(int port) {
	if(port < 1 || port > 65535) {
		ga_log("set_port: invalid port number %d.\n", port);
		return -1;
	}
	g_conf->serverport = port;
	ga_log("set_port: %d\n", port);
	return 0;
}

int
ga_client_set_object_path(const char *path) {
	if(path == NULL) {
		ga_log("set_object_path: no path given.\n");
		return -1;
	}
	strncpy(g_conf->object, path, RTSPCONF_OBJECT_SIZE);
	ga_log("set_object_path: %s\n", path);
	return 0;
}

int
ga_client_set_rtp_over_tcp(bool enabled) {
	g_conf->proto = enabled ? IPPROTO_TCP : IPPROTO_UDP;
	ga_log("set_rtp_over_tcp: %s\n", g_conf->proto == IPPROTO_TCP ? "true" : "false");
	return 0;
}

int
ga_client_set_ctrl_enable(bool enabled) {
	g_conf->ctrlenable = enabled ? 1 : 0;
	ga_log("set_ctrl_enable: %s\n", g_conf->ctrlenable? "true" : "false");
	return 0;
}

int
ga_client_set_ctrl_proto(bool tcp) {
	g_conf->ctrlproto = tcp ? IPPROTO_TCP : IPPROTO_UDP;
	ga_log("set_ctrl_proto: %s\n", g_conf->ctrlproto == IPPROTO_TCP ? "tcp" : "udp");
	return 0;
}

int
ga_client_set_ctrl_port(int port) {
	if(port < 1 || port > 65535) {
		ga_log("set_ctrl_port: invalid port number %d\n", port);
		return -1;
	}
	g_conf->ctrlport = port;
	ga_log("set_ctrl_port: %d\n", g_conf->ctrlport);
	return 0;
}

int
ga_client_set_builtin_audio(bool enabled) {
	g_conf->builtin_audio_decoder = enabled ? 1 : 0;
	ga_log("set_builtin_audio: %s", g_conf->builtin_audio_decoder ? "true" : "false");
	return 0;
}

int
ga_client_set_builtin_video(bool enabled) {
	g_conf->builtin_video_decoder = enabled ? 1 : 0;
	ga_log("set_builtin_video: %s", g_conf->builtin_video_decoder ? "true" : "false");
	return 0;
}

int
ga_client_set_audio_codec(int samplerate, int channels) {
	if(g_conf->audio_decoder_name[0] != NULL) {
		free(g_conf->audio_decoder_name[0]);
		g_conf->audio_decoder_name[0] = NULL;
	}
	//g_conf->audio_decoder_name[0] = strdup(scodec);
	g_conf->audio_samplerate = samplerate;
	g_conf->audio_channels = channels;
	ga_log("set_audio_codec: %s, samplerate=%d, channels=%d\n",
	       "codec auto-detect",
	       g_conf->audio_samplerate,
	       g_conf->audio_channels);
	return 0;
}

int
ga_client_set_drop_late_frame(int ms) {
	char value[16] = "-1";
	if(ms > 0) {
		snprintf(value, sizeof(value), "%d", ms * 1000);
	}
	ga_conf_writev("max-tolerable-video-delay", value);
	ga_error("libgaclient: configured max-tolerable-video-delay = %s\n", value);
	return 0;
}

//// control methods
int
ga_client_send_key(bool pressed, int scancode, int sym, int mod, int unicode) {
	sdlmsg_t m;
	if(g_conf == NULL || g_conf->ctrlenable == 0)
		return -1;
	sdlmsg_keyboard(&m, pressed ? 0 : 1, scancode, sym, mod, unicode);
	ctrl_client_sendmsg(&m, sizeof(sdlmsg_keyboard_t));
	return 0;
}

int
ga_client_send_mouse_button(bool pressed, int button, int x, int y) {
	sdlmsg_t m;
	if(g_conf == NULL || g_conf->ctrlenable == 0)
		return -1;
	sdlmsg_mousekey(&m, pressed ? 0 : 1, button, x, y);
	ctrl_client_sendmsg(&m, sizeof(sdlmsg_mouse_t));
	return 0;
}

int
ga_client_send_mouse_motion(int x, int y, int xrel, int yrel, int state, bool relative) {
	sdlmsg_t m;
	if(g_conf == NULL || g_conf->ctrlenable == 0)
		return -1;
	sdlmsg_mousemotion(&m, x, y, xrel, yrel, state, relative ? 0 : 1);
	ctrl_client_sendmsg(&m, sizeof(sdlmsg_mouse_t));
	return 0;
}

int
ga_client_send_mouse_wheel(int dx, int dy) {
	sdlmsg_t m;
	if(g_conf == NULL || g_conf->ctrlenable == 0)
		return -1;
	sdlmsg_mousewheel(&m, dx, dy);
	ctrl_client_sendmsg(&m, sizeof(sdlmsg_mouse_t));
	return 0;
}

int
ga_client_launch_controller() {
	if(g_conf->ctrlenable == 0)
		return 0;
	if(ctrl_queue_init(32768, sizeof(sdlmsg_t)) < 0) {
		//showToast(env, "Err: Controller disabled (no queue)");
		ga_log("Cannot initialize controller queue, controller disabled.\n");
		g_conf->ctrlenable = 0;
		return -1;
	}
	if(pthread_create(&ctrlthread, NULL, ctrl_client_thread, g_conf) != 0) {
		//showToast(env, "Err: Controller disabled (no thread)");
		ga_log("Cannot create controller thread, controller disabled.\n");
		g_conf->ctrlenable = 0;
		return -1;
	}
	pthread_detach(ctrlthread);
	return 0;
}

int
ga_client_start() {
	char urlbuf[2048];
	//
	if(g_conf->servername[0] == '\0'
	|| g_conf->object[0] == '\0') {
		ga_log("connect: No host or objpath given.\n");
		return -1;
	}
	snprintf(urlbuf, sizeof(urlbuf), "rtsp://%s:%d%s",
		 g_conf->servername, g_conf->serverport, g_conf->object);
	ga_log("connect: url [%s]\n", urlbuf);
	//
	ga_client_launch_controller();
	//
	bzero(&rtspThreadParam, sizeof(rtspThreadParam));
	rtspThreadParam.url = strdup(urlbuf);
	rtspThreadParam.running = true;
	rtspThreadParam.rtpOverTCP = (g_conf->proto == IPPROTO_TCP) ? true : false;
	for(int i = 0; i < VIDEO_SOURCE_CHANNEL_MAX; i++) {
		pthread_mutex_init(&rtspThreadParam.surfaceMutex[i], NULL);
	}
	//
	rtsp_thread(&rtspThreadParam);
	if(rtspThreadParam.url != NULL)
		free((void*) rtspThreadParam.url);
	//
	ctrl_client_sendmsg(NULL, 0);
	ga_client_cleanup();
	//
	return 0;
}

int
ga_client_stop() {
	//
	ctrl_client_sendmsg(NULL, 0);
	rtspThreadParam.running = false;
	rtspThreadParam.quitLive555 = 1;
	bzero(&ctrlthread, sizeof(ctrlthread));
	bzero(&rtspthread, sizeof(rtspthread));
	usleep(1500000);
	//ctrl_queue_free();
	//
	return 0;
}

int
ga_client_cleanup() {
	ga_client_reset_configuration();
	return 0;
}

