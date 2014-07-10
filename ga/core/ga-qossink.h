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

#ifndef __GA_QOSSINK_H__
#define __GA_QOSSINK_H__

#include <MPEG1or2AudioRTPSink.hh>
#include <MPEG4GenericRTPSink.hh>
#include <AC3AudioRTPSink.hh>
#include <SimpleRTPSink.hh>
#include <VorbisAudioRTPSink.hh>
#include <H264VideoRTPSink.hh>
#include <H265VideoRTPSink.hh>
#include <VP8VideoRTPSink.hh>
#include <TheoraVideoRTPSink.hh>
#include <T140TextRTPSink.hh>

//////////////////////////////////////////////////////////////////////////////

class QoSMPEG1or2AudioRTPSink: public MPEG1or2AudioRTPSink {
public:
	static QoSMPEG1or2AudioRTPSink* createNew(UsageEnvironment& env, Groupsock* RTPgs);
protected:
	QoSMPEG1or2AudioRTPSink(UsageEnvironment& env, Groupsock* RTPgs);
	~QoSMPEG1or2AudioRTPSink();
};

//////////////////////////////////////////////////////////////////////////////

class QoSAC3AudioRTPSink: public AC3AudioRTPSink {
public:
	static QoSAC3AudioRTPSink* createNew(UsageEnvironment& env,
			Groupsock* RTPgs,
			u_int8_t rtpPayloadFormat,
			u_int32_t rtpTimestampFrequency);
protected:
	QoSAC3AudioRTPSink(UsageEnvironment& env, Groupsock* RTPgs,
			u_int8_t rtpPayloadFormat,
			u_int32_t rtpTimestampFrequency);
	~QoSAC3AudioRTPSink();
};

//////////////////////////////////////////////////////////////////////////////

class QoSSimpleRTPSink: public SimpleRTPSink {
public:
	  QoSSimpleRTPSink* createNew(UsageEnvironment& env, Groupsock* RTPgs,
			  unsigned char rtpPayloadFormat,
			  unsigned rtpTimestampFrequency,
			  char const* sdpMediaTypeString,
			  char const* rtpPayloadFormatName,
			  unsigned numChannels = 1,
			  Boolean allowMultipleFramesPerPacket = True,
			  Boolean doNormalMBitRule = True);
protected:
	  QoSSimpleRTPSink(UsageEnvironment& env, Groupsock* RTPgs,
			  unsigned char rtpPayloadFormat,
			  unsigned rtpTimestampFrequency,
			  char const* sdpMediaTypeString,
			  char const* rtpPayloadFormatName,
			  unsigned numChannels,
			  Boolean allowMultipleFramesPerPacket,
			  Boolean doNormalMBitRule);
	  ~QoSSimpleRTPSink();
};

//////////////////////////////////////////////////////////////////////////////

class QoSH264VideoRTPSink: public H264VideoRTPSink {
public:
	static QoSH264VideoRTPSink*
		createNew(UsageEnvironment& env, Groupsock* RTPgs, unsigned char rtpPayloadFormat);
	static QoSH264VideoRTPSink*
		createNew(UsageEnvironment& env, Groupsock* RTPgs, unsigned char rtpPayloadFormat,
				u_int8_t const* sps, unsigned spsSize, u_int8_t const* pps, unsigned ppsSize);
protected:
	QoSH264VideoRTPSink(UsageEnvironment& env, Groupsock* RTPgs, unsigned char rtpPayloadFormat,
			u_int8_t const* sps = NULL, unsigned spsSize = 0,
			u_int8_t const* pps = NULL, unsigned ppsSize = 0);
	~QoSH264VideoRTPSink();
};

#endif	/* __GA_QOSSINK_H__ */
