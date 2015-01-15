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

#ifndef __GA_LIVERSERVER_H__
#define __GA_LIVERSERVER_H__

#include "ga-common.h"
#include "rtspconf.h"
#include "liveMedia.hh"

#define DISCRETE_FRAMER		/* use discrete framer */

#define	QOS_SERVER_CHECK_INTERVAL_MS	(1 * 1000)	/* check every N seconds */
#define	QOS_SERVER_REPORT_INTERVAL_MS	(30 * 1000)	/* report every N seconds */
#define QOS_SERVER_PREFIX_LEN		64

typedef struct qos_server_record_s {
	unsigned long long pkts_lost;
	unsigned long long pkts_sent;
	unsigned long long bytes_sent;
	struct timeval timestamp;
}	qos_server_record_t;

void * liveserver_taskscheduler();
void * liveserver_main(void *arg);

int qos_server_start();
int qos_server_stop();
int qos_server_add_sink(const char *prefix, RTPSink *rtpsink);
int qos_server_remove_sink(RTPSink *rtpsink);
int qos_server_deinit();
int qos_server_init();

#endif /* __GA_LIVERSERVER_H__ */
