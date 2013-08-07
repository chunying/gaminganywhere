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
#include <netinet/in.h>
#ifndef GA_EMCC
#include <sys/syscall.h>
#endif /* ! GA_EMCC */
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <arpa/inet.h>
#endif	/* ! WIN32 */

#include "ga-common.h"
#include "rtspconf.h"
#include "server.h"
#include "rtspserver.h"

void *
rtspserver_main(void *arg) {
#ifdef WIN32
	SOCKET s, cs;
	int csinlen;
#else
	int s, cs;
	socklen_t csinlen;
#endif
	struct sockaddr_in sin, csin;
	struct RTSPConf *conf = rtspconf_global();
	//
	if((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		perror("socket");
		return (void *) -1;
	}
	//
	do {
#ifdef WIN32
		BOOL val = 1;
		setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*) &val, sizeof(val));
#else
		int val = 1;
		setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
#endif
	} while(0);
	//
	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(conf->serverport);
	//
	if(bind(s, (struct sockaddr*) &sin, sizeof(sin)) < 0) {
		perror("bind");
		return (void *) -1;
	}
	if(listen(s, 256) < 0) {
		perror("listen");
		return (void *) -1;
	}
	//
	do {
		pthread_t thread;
		//
		csinlen = sizeof(csin);
		bzero(&csin, sizeof(csin));
		if((cs = accept(s, (struct sockaddr*) &csin, &csinlen)) < 0) {
			perror("accept");
			return (void *) -1;
		}
		// tunning sending window
		do {
			int sndwnd = 8388608;	// 8MB
			if(setsockopt(cs, SOL_SOCKET, SO_SNDBUF, &sndwnd, sizeof(sndwnd)) == 0) {
				ga_error("*** set TCP sending buffer success.\n");
			} else {
				ga_error("*** set TCP sending buffer failed.\n");
			}
		} while(0);
		//
		if(pthread_create(&thread, NULL, rtspserver, &cs) != 0) {
			close(cs);
			ga_error("cannot create service thread.\n");
			continue;
		}
		pthread_detach(thread);
	} while(1);
	//
	return (void *) 0;
}

