#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>

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
	RTSPConf *rtspconf = rtspconf_global();
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
	RTSPServer* rtspServer = RTSPServer::createNew(*env, 8554, authDB);
	//
	if (rtspServer == NULL) {
		//*env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
		ga_error("Failed to create RTSP server: %s\n", env->getResultMsg());
		exit(1);
	}
	//
	encoder_pktqueue_init(VIDEO_SOURCE_CHANNEL_MAX+1, 3 * 1024* 1024/*3MB*/);
	encoder_config_rtspserver(RTSPSERVER_TYPE_LIVE);
	//
	ServerMediaSession * sms
		= ServerMediaSession::createNew(*env, "desktop", "desktop", 
				"GamingAnywhere Server");
	for(cid = 0; cid < video_source_channels(); cid++) {
		sms->addSubsession(GAMediaSubsession::createNew(*env, cid, encoder_get_vencoder()->mimetype)); 
	}
	sms->addSubsession(GAMediaSubsession::createNew(*env, cid, encoder_get_aencoder()->mimetype)); 
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

	env->taskScheduler().doEventLoop(); // does not return

	return NULL;
}

