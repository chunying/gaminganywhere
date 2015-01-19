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

#include <H264VideoStreamDiscreteFramer.hh>
#include <H265VideoStreamDiscreteFramer.hh>
#include <H264VideoStreamFramer.hh>
#include <H265VideoStreamFramer.hh>

#include "ga-common.h"
#include "ga-conf.h"
#include "rtspconf.h"
#include "encoder-common.h"

#include "ga-liveserver.h"
#include "ga-mediasubsession.h"
#include "ga-audiolivesource.h"
#include "ga-videolivesource.h"
#include "ga-qossink.h"

GAMediaSubsession
::GAMediaSubsession(UsageEnvironment &env, int cid, const char *mimetype, portNumBits initialPortNum, Boolean multiplexRTCPWithRTP)
		: OnDemandServerMediaSubsession(env, True/*reuseFirstSource*/, initialPortNum, multiplexRTCPWithRTP) {
	this->mimetype = strdup(mimetype);
	this->channelId = cid;
}

GAMediaSubsession * GAMediaSubsession
::createNew(UsageEnvironment &env, int cid, const char *mimetype,
		portNumBits initialPortNum,
		Boolean multiplexRTCPWithRTP) {
	return new GAMediaSubsession(env, cid, mimetype, initialPortNum, multiplexRTCPWithRTP);
}

FramedSource* GAMediaSubsession
::createNewStreamSource(unsigned clientSessionId,
			unsigned& estBitrate) {
	FramedSource *result = NULL;
	struct RTSPConf *rtspconf = rtspconf_global();
	if(strncmp("audio/", this->mimetype, 6) == 0) {
		estBitrate = rtspconf->audio_bitrate / 1000; /* Kbps */
		result = GAAudioLiveSource::createNew(envir(), this->channelId);
	} else if(strncmp("video/", this->mimetype, 6) == 0) {
		//estBitrate = 500; /* Kbps */
		estBitrate = ga_conf_mapreadint("video-specific", "b") / 1000; /* Kbps */
		OutPacketBuffer::increaseMaxSizeTo(8000000);
		result = GAVideoLiveSource::createNew(envir(), this->channelId);
	}
	do if(result != NULL) {
		if(strcmp("video/H264", this->mimetype) == 0) {
#ifdef DISCRETE_FRAMER
			result = H264VideoStreamDiscreteFramer::createNew(envir(), result);
#else
			result = H264VideoStreamFramer::createNew(envir(), result);
#endif
			break;
		}
		if(strcmp("video/H265", this->mimetype) == 0) {
#ifdef DISCRETE_FRAMER
			result = H265VideoStreamDiscreteFramer::createNew(envir(), result);
#else
			result = H265VideoStreamFramer::createNew(envir(), result);
#endif
			break;
		}
	} while(0);
	return result;
}

// "estBitrate" is the stream's estimated bitrate, in kbps
RTPSink* GAMediaSubsession
::createNewRTPSink(Groupsock* rtpGroupsock,
			unsigned char rtpPayloadTypeIfDynamic,
			FramedSource* inputSource) {
	RTPSink *result = NULL;
	struct RTSPConf *rtspconf = rtspconf_global();
	const char *mimetype = this->mimetype;
	int err;
	//
	if(strcmp(mimetype, "audio/MPEG") == 0) {
		result = QoSMPEG1or2AudioRTPSink::createNew(envir(), rtpGroupsock);
	} else if(strcmp(mimetype, "audio/AAC") == 0) {
		// TODO: not implememted
		ga_error("GAMediaSubsession: %s NOT IMPLEMENTED\n", mimetype);
		exit(-1);
	} else if(strcmp(mimetype, "audio/AC3") == 0) {
		result = QoSAC3AudioRTPSink
			::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
					rtspconf->audio_samplerate);
	} else if(strcmp(mimetype, "audio/OPUS") == 0) {
		result = QoSSimpleRTPSink
			::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
					rtspconf->audio_samplerate,
					"audio", "OPUS", 2, False/*only 1 Opus 'packet' in each RTP packet*/);
	} else if(strcmp(mimetype, "audio/VORBIS") == 0) {
		u_int8_t* identificationHeader = NULL; unsigned identificationHeaderSize = 0;
		u_int8_t* commentHeader = NULL; unsigned commentHeaderSize = 0;
		u_int8_t* setupHeader = NULL; unsigned setupHeaderSize = 0;
		// TODO: not implememted
		ga_error("GAMediaSubsession: %s NOT IMPLEMENTED\n", mimetype);
		exit(-1);
		result = QoSVorbisAudioRTPSink
			::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
					rtspconf->audio_samplerate,
					rtspconf->audio_channels,
					identificationHeader, identificationHeaderSize,
					commentHeader, commentHeaderSize,
					setupHeader, setupHeaderSize);
	} else if(strcmp(mimetype, "video/THEORA") == 0) {
		u_int8_t* identificationHeader = NULL; unsigned identificationHeaderSize = 0;
		u_int8_t* commentHeader = NULL; unsigned commentHeaderSize = 0;
		u_int8_t* setupHeader = NULL; unsigned setupHeaderSize = 0;
		// TODO: not implememted
		ga_error("GAMediaSubsession: %s NOT IMPLEMENTED\n", mimetype);
		exit(-1);
		result = QoSTheoraVideoRTPSink
			::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
					identificationHeader, identificationHeaderSize,
					commentHeader, commentHeaderSize,
					setupHeader, setupHeaderSize);
	} else if(strcmp(mimetype, "video/H264") == 0) {
		ga_module_t *m = encoder_get_vencoder();
		ga_ioctl_buffer_t mb;
		unsigned profile_level_id = 0;
		u_int8_t SPS[256]; int SPSSize = 0;
		u_int8_t PPS[256]; int PPSSize = 0;
		//
		mb.id = this->channelId;
		mb.ptr = SPS;
		mb.size = sizeof(SPS);
		if((err = ga_module_ioctl(m, GA_IOCTL_GETSPS, sizeof(mb), &mb)) < 0) {
			ga_error("unable to get SPS from %s, err=%d\n", m->name, err);
			exit(-1);
		}
		SPSSize = mb.size;
		//
		mb.id = this->channelId;
		mb.ptr = PPS;
		mb.size = sizeof(PPS);
		if((err = ga_module_ioctl(m, GA_IOCTL_GETPPS, sizeof(mb), &mb)) < 0) {
			ga_error("unable to get PPS from %s, err=%d\n", m->name, err);
			exit(-1);
		}
		PPSSize = mb.size;
		//
		if (SPSSize >= 1/*'profile_level_id' offset within SPS*/ + 3/*num bytes needed*/) {
			profile_level_id = (SPS[1]<<16) | (SPS[2]<<8) | SPS[3];
		}
		ga_error("GAMediaSubsession: %s SPS=%p(%d); PPS=%p(%d); profile_level_id=%x\n",
			mimetype,
			SPS, SPSSize, PPS, PPSSize, profile_level_id);
		result = QoSH264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
				SPS, SPSSize, PPS, PPSSize/*, profile_level_id*/);
	} else if(strcmp(mimetype, "video/H265") == 0) {
		ga_module_t *m = encoder_get_vencoder();
		ga_ioctl_buffer_t mb;
		u_int8_t VPS[256]; int VPSSize = 0;
		u_int8_t SPS[256]; int SPSSize = 0;
		u_int8_t PPS[256]; int PPSSize = 0;
		//
		mb.id = this->channelId;
		mb.ptr = SPS;
		mb.size = sizeof(SPS);
		if((err = ga_module_ioctl(m, GA_IOCTL_GETSPS, sizeof(mb), &mb)) < 0) {
			ga_error("unable to get SPS from %s, err=%d\n", m->name, err);
			exit(-1);
		}
		SPSSize = mb.size;
		//
		mb.id = this->channelId;
		mb.ptr = PPS;
		mb.size = sizeof(PPS);
		if((err = ga_module_ioctl(m, GA_IOCTL_GETPPS, sizeof(mb), &mb)) < 0) {
			ga_error("unable to get PPS from %s, err=%d\n", m->name, err);
			exit(-1);
		}
		PPSSize = mb.size;
		//
		mb.id = this->channelId;
		mb.ptr = VPS;
		mb.size = sizeof(VPS);
		if((err = ga_module_ioctl(m, GA_IOCTL_GETVPS, sizeof(mb), &mb)) < 0) {
			ga_error("unable to get VPS from %s, err=%d\n", m->name, err);
			exit(-1);
		}
		VPSSize = mb.size;
		//
		ga_error("GAMediaSubsession: %s SPS=%p(%d); PPS=%p(%d); VPS=%p(%d)\n",
			mimetype,
			SPS, SPSSize, PPS, PPSSize, VPS, VPSSize);
		result = QoSH265VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
				VPS, VPSSize, SPS, SPSSize, PPS, PPSSize/*,
				profileSpace, profileId, tierFlag, levelId,
				interopConstraintsStr*/);
	} else if(strcmp(mimetype, "video/VP8") == 0) {
		result = QoSVP8VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
	} 
	if(result == NULL) {
		ga_error("GAMediaSubsession: create RTP sink for %s failed.\n", mimetype);
	}
	return result;
}

