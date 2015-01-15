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

#ifndef __GA_MEDIASUBSESSION_H__
#define __GA_MEDIASUBSESSION_H__

#include <stdio.h>
#include <OnDemandServerMediaSubsession.hh>

class GAMediaSubsession : public OnDemandServerMediaSubsession {
private:
	const char *mimetype;
	int channelId;
public:
	static GAMediaSubsession * createNew(UsageEnvironment &env,
			int cid, /* channel Id */
			const char *mimetype = NULL,
			portNumBits initialPortNum=6970,
			Boolean multiplexRTCPWithRTP=False);
protected:
	GAMediaSubsession(UsageEnvironment &env,
			int cid, /* channel Id */
			const char *mimetype = NULL,
			portNumBits initialPortNum=6970,
			Boolean multiplexRTCPWithRTP=False);
	virtual FramedSource* createNewStreamSource(unsigned clientSessionId,
			unsigned& estBitrate);
	// "estBitrate" is the stream's estimated bitrate, in kbps
	virtual RTPSink* createNewRTPSink(Groupsock* rtpGroupsock,
			unsigned char rtpPayloadTypeIfDynamic,
			FramedSource* inputSource);
};

#endif /* __GA_MEDIASUBSESSION_H__ */
