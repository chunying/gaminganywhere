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

#include "ga-common.h"
#include "ga-mediasubsession.h"
#include "ga-audiolivesource.h"
#include "ga-videolivesource.h"

GAMediaSubsession
::GAMediaSubsession(UsageEnvironment &env, const char *mimeType, portNumBits initialPortNum, Boolean multiplexRTCPWithRTP)
		: OnDemandServerMediaSubsession(env, True/*reuseFirstSource*/, initialPortNum, multiplexRTCPWithRTP) {
	this->mimeType = mimeType;
}

GAMediaSubsession * GAMediaSubsession
::createNew(UsageEnvironment &env, const char *mimeType,
		portNumBits initialPortNum,
		Boolean multiplexRTCPWithRTP) {
	return new GAMediaSubsession(env, mimeType, initialPortNum, multiplexRTCPWithRTP);
}

FramedSource* GAMediaSubsession
::createNewStreamSource(unsigned clientSessionId,
			unsigned& estBitrate) {
	FramedSource *result = NULL;
	const char *mimeType = this->mimeType;
	// TODO: try to get content type from encoder
	if(strncmp("audio/", mimeType, 6) == 0) {
		estBitrate = 128; /* Kbps */
		return GAAudioLiveSource::createNew(envir());
	} else if(strncmp("video/", mimeType, 6) == 0) {
		estBitrate = 500; /* Kbps */
		return GAVideoLiveSource::createNew(envir());
	}
	return result;
}

// "estBitrate" is the stream's estimated bitrate, in kbps
RTPSink* GAMediaSubsession
::createNewRTPSink(Groupsock* rtpGroupsock,
			unsigned char rtpPayloadTypeIfDynamic,
			FramedSource* inputSource) {
	RTPSink *result = NULL;
	const char *mimeType = this->mimeType;
	// TODO: try to get content type from encoder
	if(strcmp(mimeType, "audio/MPEG") == 0) {
		result = MPEG1or2AudioRTPSink::createNew(envir(), rtpGroupsock);
	} else if(strcmp(mimeType, "audio/AAC") == 0) {
		// TODO: not implememted
	} else if(strcmp(mimeType, "audio/AC3") == 0) {
		result = AC3AudioRTPSink
			::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
					44100/* TODO: sampling frequency */);
	} else if(strcmp(mimeType, "audio/OPUS") == 0) {
		result = SimpleRTPSink
			::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
					48000, "audio", "OPUS", 2, False/*only 1 Opus 'packet' in each RTP packet*/);
	} else if(strcmp(mimeType, "audio/VORBIS") == 0) {
		u_int8_t* identificationHeader = NULL; unsigned identificationHeaderSize = 0;
		u_int8_t* commentHeader = NULL; unsigned commentHeaderSize = 0;
		u_int8_t* setupHeader = NULL; unsigned setupHeaderSize = 0;
		// TODO: not implememted
		result = VorbisAudioRTPSink
			::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
					44100/* TODO: sampling frequency */,
					2/* TODO: # of channels */,
					identificationHeader, identificationHeaderSize,
					commentHeader, commentHeaderSize,
					setupHeader, setupHeaderSize);
	} else if(strcmp(mimeType, "video/THEORA") == 0) {
		u_int8_t* identificationHeader = NULL; unsigned identificationHeaderSize = 0;
		u_int8_t* commentHeader = NULL; unsigned commentHeaderSize = 0;
		u_int8_t* setupHeader = NULL; unsigned setupHeaderSize = 0;
		// TODO: not implememted
		result = TheoraVideoRTPSink
			::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
					identificationHeader, identificationHeaderSize,
					commentHeader, commentHeaderSize,
					setupHeader, setupHeaderSize);
	} else if(strcmp(mimeType, "video/H264") == 0) {
		unsigned profile_level_id = 0;
		u_int8_t* SPS = NULL; unsigned SPSSize = 0;
		u_int8_t* PPS = NULL; unsigned PPSSize = 0;
		// TODO: not implememted
		result = H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
				SPS, SPSSize, PPS, PPSSize, profile_level_id);
	} else if(strcmp(mimeType, "video/H265") == 0) {
		unsigned profileSpace = 0; // general_profile_space
		unsigned profileId = 0; // general_profile_idc
		unsigned tierFlag = 0; // general_tier_flag
		unsigned levelId = 0; // general_level_idc
		char interopConstraintsStr[100];
		//
		u_int8_t* VPS = NULL; unsigned VPSSize = 0;
		u_int8_t* SPS = NULL; unsigned SPSSize = 0;
		u_int8_t* PPS = NULL; unsigned PPSSize = 0;
		// TODO: not implememted
		result = H265VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
				VPS, VPSSize, SPS, SPSSize, PPS, PPSSize,
				profileSpace, profileId, tierFlag, levelId,
				interopConstraintsStr);
	} else if(strcmp(mimeType, "video/VP8") == 0) {
		result = VP8VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
	} 
	return result;
}

