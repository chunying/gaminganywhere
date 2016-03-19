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
#include <pthread.h>
#ifndef WIN32
#include <unistd.h>
#include <netinet/in.h>
#include <sys/syscall.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <arpa/inet.h>
#endif	/* ! WIN32 */

#include "ga-common.h"
#include "ga-module.h"
#include "encoder-common.h"
#include "rtspconf.h"

#include "ga-liveserver.h"
#include "server-live555.h"

static pthread_t server_tid;

int
live_server_register_client(void *ccontext) {
	if(encoder_register_client(ccontext) < 0)
		return -1;
	return 0;
}

int
live_server_unregister_client(void *ccontext) {
	if(encoder_unregister_client(ccontext) < 0)
		return -1;
	return 0;
}

static int
live_server_init(void *arg) {
	// nothing to do now
	return 0;
}

static int
live_server_start(void *arg) {
	pthread_cancel_init();
	if(pthread_create(&server_tid, NULL, liveserver_main, NULL) != 0) {
		ga_error("start live-server failed.\n");
		return -1;
	}
	return 0;
}

static int
live_server_stop(void *arg) {
	pthread_cancel(server_tid);
	return 0;
}

static int
live_server_deinit(void *arg) {
	// nothing to do now
	return 0;
}

static int
live_server_send_packet(const char *prefix, int channelId, AVPacket *pkt, int64_t encoderPts, struct timeval *ptv) {
	encoder_pktqueue_append(channelId, pkt, encoderPts, ptv);
	return 0;
}

ga_module_t *
module_load() {
	static ga_module_t m;
	//
	bzero(&m, sizeof(m));
	m.type = GA_MODULE_TYPE_SERVER;
	m.name = strdup("live555-rtsp-server");
	m.init = live_server_init;
	m.start = live_server_start;
	m.stop = live_server_stop;
	m.deinit = live_server_deinit;
	m.send_packet = live_server_send_packet;
	//
	encoder_register_sinkserver(&m);
	//
	return &m;
}

