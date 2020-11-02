#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <map>

#include "ga-common.h"
#include "rtspconf.h"
#include "encoder-common.h"
#include "vsource.h"
#include "ga-mediasubsession.h"
#include "ga-liveserver.h"

static UsageEnvironment* env = NULL;

void *
liveserver_taskscheduler() {
	if(env == NULL)
		return NULL;
	return &env->taskScheduler();
}

void *
liveserver_main(void *arg) {
	int cid;
	ga_module_t *m;
	struct RTSPConf *rtspconf = rtspconf_global();
	TaskScheduler* scheduler = BasicTaskScheduler::createNew();
	UserAuthenticationDatabase* authDB = NULL;
	env = BasicUsageEnvironment::createNew(*scheduler);
#if 0	// need access control?
	// To implement client access control to the RTSP server, do the following:
	authDB = new UserAuthenticationDatabase;
	authDB->addUserRecord("username1", "password1"); // replace these with real strings
	// Repeat the above with each <username>, <password> that you wish to allow
	// access to the server.
#endif
	RTSPServer* rtspServer = RTSPServer::createNew(*env, rtspconf->serverport > 0 ? rtspconf->serverport : 8554, authDB);
	//
	if (rtspServer == NULL) {
		//*env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
		ga_error("Failed to create RTSP server: %s\n", env->getResultMsg());
		exit(1);
	}
	//
	encoder_pktqueue_init(VIDEO_SOURCE_CHANNEL_MAX+1, 3 * 1024* 1024/*3MB*/);
	//
	ServerMediaSession * sms
		= ServerMediaSession::createNew(*env,
				rtspconf->object[0] ? &rtspconf->object[1] : "ga",
				rtspconf->object[0] ? &rtspconf->object[1] : "ga",
				rtspconf->title[0] ? rtspconf->title : "GamingAnywhere Server");
	//
	qos_server_init();
	// add video session
	if((m = encoder_get_vencoder()) == NULL) {
		ga_error("live-server: FATAL - no video encoder registered.\n");
		exit(-1);
	}
	if(m->mimetype == NULL) {
		ga_error("live-server: FATAL - video encoder does not configure mimetype.\n");
		exit(-1);
	}
	for(cid = 0; cid < video_source_channels(); cid++) {
		sms->addSubsession(GAMediaSubsession::createNew(*env, cid, m->mimetype)); 
	}
	// add audio session, if necessary
	if((m = encoder_get_aencoder()) != NULL) {
		if(m->mimetype == NULL) {
			ga_error("live-server: FATAL - audio encoder does not configure mimetype.\n");
			exit(-1);
		}
		sms->addSubsession(GAMediaSubsession::createNew(*env, cid, m->mimetype)); 
	}
	rtspServer->addServerMediaSession(sms);

	if(rtspServer->setUpTunnelingOverHTTP(80)
	|| rtspServer->setUpTunnelingOverHTTP(8000)
	|| rtspServer->setUpTunnelingOverHTTP(8080)) {
		ga_error("(Use port %d for optional RTSP-over-HTTP tunneling.)\n",
			rtspServer->httpServerPortNum());
		//*env << "\n(We use port " << rtspServer->httpServerPortNum() << " for optional RTSP-over-HTTP tunneling.)\n";
	} else {
		ga_error("(RTSP-over-HTTP tunneling is not available.)\n");
		//*env << "\n(RTSP-over-HTTP tunneling is not available.)\n";
	}

	qos_server_start();
	env->taskScheduler().doEventLoop(); // does not return
	qos_server_stop();
	qos_server_deinit();

	return NULL;
}

//////// qos report functions

static std::map<RTPSink*, std::map<unsigned/*SSRC*/,qos_server_record_t> > sinkmap;
static TaskToken qos_task = NULL;
static int qos_started = 0;
static struct timeval qos_tv;

static void qos_server_schedule();

static void
qos_server_report(void *clientData) {
	struct timeval now;
	std::map<RTPSink*, std::map<unsigned,qos_server_record_t> >::iterator mi;
	//
	gettimeofday(&now, NULL);
	for(mi = sinkmap.begin(); mi != sinkmap.end(); mi++) {
		RTPTransmissionStatsDB& db = mi->first->transmissionStatsDB();
		RTPTransmissionStatsDB::Iterator statsIter(db);
		RTPTransmissionStats *stats = NULL;
		while((stats = statsIter.next()) != NULL) {
			unsigned ssrc = stats->SSRC();
			std::map<unsigned,qos_server_record_t>::iterator mj;
			unsigned long long pkts_lost, d_pkt_lost;
			unsigned long long pkts_sent, d_pkt_sent;
			unsigned long long bytes_sent, d_byte_sent;
			unsigned pkts_sent_hi, pkts_sent_lo;
			unsigned bytes_sent_hi, bytes_sent_lo;
			long long elapsed;
			//
			if((mj = mi->second.find(ssrc)) == mi->second.end()) {
				qos_server_record_t qr;
				bzero(&qr, sizeof(qr));
				qr.timestamp = now;
				mi->second[ssrc] = qr;
				continue;
			}
			//
			elapsed = tvdiff_us(&now, &mj->second.timestamp);
			if(elapsed < QOS_SERVER_REPORT_INTERVAL_MS * 1000)
				continue;
			mj->second.timestamp = now;
			//
			pkts_lost = stats->totNumPacketsLost();
			stats->getTotalPacketCount(pkts_sent_hi, pkts_sent_lo);
			stats->getTotalOctetCount(bytes_sent_hi, bytes_sent_lo);
			pkts_sent = pkts_sent_hi;
			pkts_sent = (pkts_sent << 32) | pkts_sent_lo;
			bytes_sent = bytes_sent_hi;
			bytes_sent = (bytes_sent << 32) | bytes_sent_lo;
			// delta
			d_pkt_lost = pkts_lost - mj->second.pkts_lost;
			d_pkt_sent = pkts_sent - mj->second.pkts_sent;
			d_byte_sent = bytes_sent - mj->second.bytes_sent;
			//
			if(d_pkt_lost > d_pkt_sent) {
				ga_error("qos: invalid packet loss count detected (%d)\n", d_pkt_lost);
				d_pkt_lost = 0;
			}
			// nothing updated?
			if(d_pkt_lost == 0 && d_pkt_sent == 0 && d_byte_sent == 0)
				continue;
			// report
			ga_error("%s-report: %lldKB sent; pkt-loss=%lld/%lld,%.2f%%; bitrate=%.0fKbps; rtt=%u (%.3fms); jitter=%u (freq=%uHz)\n",
					mi->first->rtpPayloadFormatName(),
					d_byte_sent / 1024,
					d_pkt_lost, d_pkt_sent, 100.0*d_pkt_lost/d_pkt_sent,
					(7812.5/*8000000.0/1024*/)*d_byte_sent/elapsed,
					stats->roundTripDelay(),
					1000.0 * stats->roundTripDelay() / 65536,
					stats->jitter(),
					mi->first->rtpTimestampFrequency());
			//
			mj->second.pkts_lost = pkts_lost;
			mj->second.pkts_sent = pkts_sent;
			mj->second.bytes_sent = bytes_sent;
		}
	}
	// schedule next qos
	qos_tv = now;
	qos_server_schedule();
	return;
}

static void
qos_server_schedule() {
	struct timeval now, timeout;
	if(qos_started == 0)
		return;
	timeout.tv_sec = qos_tv.tv_sec;
	timeout.tv_usec = qos_tv.tv_usec + QOS_SERVER_CHECK_INTERVAL_MS * 1000;
	timeout.tv_sec += (timeout.tv_usec / 1000000);
	timeout.tv_usec %= 1000000;
	gettimeofday(&now, NULL);
	qos_task = env->taskScheduler().scheduleDelayedTask(
			tvdiff_us(&timeout, &now), (TaskFunc*) qos_server_report, NULL);
	return;
}

int
qos_server_start() {
	if(env == NULL)
		return -1;
	gettimeofday(&qos_tv, NULL);
	qos_started = 1;
	qos_server_schedule();
	return 0;
}

int
qos_server_stop() {
	qos_started = 0;
	return 0;
}

int
qos_server_add_sink(const char *prefix, RTPSink *rtpsink) {
	std::map<unsigned/*SSRC*/,qos_server_record_t> x;
	sinkmap[rtpsink] = x;
	ga_error("qos: add sink#%d for %s, rtpsink=%p\n", sinkmap.size(), prefix, rtpsink);
	return 0;
}

int
qos_server_remove_sink(RTPSink *rtpsink) {
	sinkmap.erase(rtpsink);
	return 0;
}

int
qos_server_deinit() {
	if(env != NULL) {
		env->taskScheduler().unscheduleDelayedTask(qos_task);
	}
	qos_task = NULL;
	sinkmap.clear();
	ga_error("qos-measurement: deinitialized.\n");
	return 0;
}

int
qos_server_init() {
	if(env == NULL) {
		ga_error("liveserver: UsageEnvironment has not been initialized.\n");
		return -1;
	}
	sinkmap.clear();
	ga_error("qos-measurement: initialized.\n");
	return 0;
}

