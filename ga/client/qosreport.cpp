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
#include <string.h>

#include "ga-common.h"
#include "vsource.h"
#include "rtspclient.h"
#include "qosreport.h"

#define	Q_MAX		(VIDEO_SOURCE_CHANNEL_MAX+1)

static UsageEnvironment *env = NULL;
static TaskToken qos_task = NULL;
//
static int n_qrec = 0;
static qos_record_t qrec[Q_MAX];
static struct timeval qos_tv;

static void qos_schedule();

static void
qos_report(void *clientData) {
	int i;
	struct timeval now;
	long long elapsed;
	//
	gettimeofday(&now, NULL);
	elapsed = tvdiff_us(&now, &qos_tv);
	for(i = 0; i < n_qrec; i++) {
		RTPReceptionStatsDB::Iterator statsIter(qrec[i].rtpsrc->receptionStatsDB());
		// Assume that there's only one SSRC source (usually the case):
		RTPReceptionStats* stats = statsIter.next(True);
		unsigned pkts_expected, dExp;
		unsigned pkts_received, dRcvd;
		double KB_received, dKB;
		//
		if(stats == NULL)
			continue;
		pkts_expected = stats->totNumPacketsExpected();
		pkts_received = stats->totNumPacketsReceived();
		KB_received = stats->totNumKBytesReceived();
		// delta ...
		dExp = pkts_expected - qrec[i].pkts_expected;
		dRcvd = pkts_received - qrec[i].pkts_received;
		dKB = KB_received - qrec[i].KB_received;
		// show info
		ga_error("%s-report: %.0fKB rcvd; pkt-loss=%d/%d,%.2f%%; bitrate=%.0fKbps; jitter=%u (freq=%uHz)\n",
			//now.tv_sec, now.tv_usec,
			qrec[i].prefix, dKB, dExp-dRcvd, dExp, 100.0*(dExp-dRcvd)/dExp,
			8000000.0*dKB/elapsed,
			stats->jitter(),
			qrec[i].rtpsrc->timestampFrequency());
		//
		qrec[i].pkts_expected = pkts_expected;
		qrec[i].pkts_received = pkts_received;
		qrec[i].KB_received = KB_received;
	}
	// schedule next qos
	qos_tv = now;
	qos_schedule();
	return;
}

static void
qos_schedule() {
	struct timeval now, timeout;
	timeout.tv_sec = qos_tv.tv_sec;
	timeout.tv_usec = qos_tv.tv_usec + QOS_INTERVAL_MS * 1000;
	timeout.tv_sec += (timeout.tv_usec / 1000000);
	timeout.tv_usec %= 1000000;
	gettimeofday(&now, NULL);
	qos_task = env->taskScheduler().scheduleDelayedTask(
			tvdiff_us(&timeout, &now), (TaskFunc*) qos_report, NULL);
	return;
}

int
qos_start() {
	if(env == NULL)
		return -1;
	if(n_qrec <= 0)
		return 0;
	gettimeofday(&qos_tv, NULL);
	qos_schedule();
	return 0;
}

int
qos_add_source(const char *prefix, RTPSource *rtpsrc) {
	if(n_qrec >= Q_MAX) {
		ga_error("qos-measurement: too many channels (limit=%d).\n", Q_MAX);
		return -1;
	}
	if(rtpsrc == NULL) {
		ga_error("qos-measurement: invalid RTPSource object.\n");
		return -1;
	}
	snprintf(qrec[n_qrec].prefix, QOS_PREFIX_LEN, "%s", prefix);
	qrec[n_qrec].rtpsrc = rtpsrc;
	ga_error("qos-measurement: source #%d added, prefix=%d\n", n_qrec, prefix);
	n_qrec++;
	return 0;
}

int
qos_deinit() {
	if(env != NULL) {
		env->taskScheduler().unscheduleDelayedTask(qos_task);
	}
	qos_task = NULL;
	env = NULL;
	n_qrec = 0;
	bzero(qrec, sizeof(qrec));
	ga_error("qos-measurement: deinitialized.\n");
	return 0;
}

int
qos_init(UsageEnvironment *ue) {
	env = ue;
	n_qrec = 0;
	bzero(qrec, sizeof(qrec));
	ga_error("qos-measurement: initialized.\n");
	return 0;
}

