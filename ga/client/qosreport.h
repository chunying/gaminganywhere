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

#ifndef __QOSREPORT_H__
#define	__QOSREPORT_H__

#include "ga-common.h"
#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

#define	QOS_INTERVAL_MS	(30 * 1000)	/* report every N seconds */
#define QOS_PREFIX_LEN	64

typedef struct qos_record_s {
	char prefix[QOS_PREFIX_LEN];
	RTPSource *rtpsrc;
	unsigned pkts_expected;
	unsigned pkts_received;
	double KB_received;
}	qos_record_t;

int qos_start();
int qos_add_source(const char *prefix, RTPSource *rtpsrc);
int qos_deinit();
int qos_init(UsageEnvironment *ue);

#endif	/* __QOSREPORT_H__ */
