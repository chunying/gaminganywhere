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

#include "ga-common.h"
#include "ga-liveserver.h"
#include "ga-qossink.h"

//////////////////////////////////////////////////////////////////////////////

QoSMPEG1or2AudioRTPSink*
QoSMPEG1or2AudioRTPSink
::createNew(UsageEnvironment& env, Groupsock* RTPgs) {
	return new QoSMPEG1or2AudioRTPSink(env, RTPgs);
}

QoSMPEG1or2AudioRTPSink
::QoSMPEG1or2AudioRTPSink(UsageEnvironment& env, Groupsock* RTPgs)
	: MPEG1or2AudioRTPSink(env, RTPgs) {
		qos_server_add_sink("MPEG1or2", this);
}

QoSMPEG1or2AudioRTPSink
::~QoSMPEG1or2AudioRTPSink() { qos_server_remove_sink(this); }

//////////////////////////////////////////////////////////////////////////////

QoSAC3AudioRTPSink*
QoSAC3AudioRTPSink
::createNew(UsageEnvironment& env,
		Groupsock* RTPgs,
		u_int8_t rtpPayloadFormat,
		u_int32_t rtpTimestampFrequency) {
	return new QoSAC3AudioRTPSink(env, RTPgs, rtpPayloadFormat, rtpTimestampFrequency);
}

QoSAC3AudioRTPSink
::QoSAC3AudioRTPSink(UsageEnvironment& env, Groupsock* RTPgs,
		u_int8_t rtpPayloadFormat,
		u_int32_t rtpTimestampFrequency)
	: AC3AudioRTPSink(env, RTPgs, rtpPayloadFormat, rtpTimestampFrequency) {
		qos_server_add_sink("AC3", this);
}

QoSAC3AudioRTPSink
::~QoSAC3AudioRTPSink() { qos_server_remove_sink(this); }

//////////////////////////////////////////////////////////////////////////////

QoSSimpleRTPSink*
QoSSimpleRTPSink
::createNew(UsageEnvironment& env, Groupsock* RTPgs,
		unsigned char rtpPayloadFormat,
		unsigned rtpTimestampFrequency,
		char const* sdpMediaTypeString,
		char const* rtpPayloadFormatName,
		unsigned numChannels,
		Boolean allowMultipleFramesPerPacket,
		Boolean doNormalMBitRule) {
	return new QoSSimpleRTPSink(env, RTPgs,
			rtpPayloadFormat, rtpTimestampFrequency,
			sdpMediaTypeString, rtpPayloadFormatName,
			numChannels,
			allowMultipleFramesPerPacket,
			doNormalMBitRule);
}

QoSSimpleRTPSink
::QoSSimpleRTPSink(UsageEnvironment& env, Groupsock* RTPgs,
		unsigned char rtpPayloadFormat,
		unsigned rtpTimestampFrequency,
		char const* sdpMediaTypeString,
		char const* rtpPayloadFormatName,
		unsigned numChannels,
		Boolean allowMultipleFramesPerPacket,
		Boolean doNormalMBitRule)
	: SimpleRTPSink(env, RTPgs, rtpPayloadFormat,
			rtpTimestampFrequency, sdpMediaTypeString,
			rtpPayloadFormatName, numChannels,
			allowMultipleFramesPerPacket, doNormalMBitRule) {
	qos_server_add_sink("SimpleRTPSink", this);
}

QoSSimpleRTPSink
::~QoSSimpleRTPSink() { qos_server_remove_sink(this); }

//////////////////////////////////////////////////////////////////////////////

QoSVorbisAudioRTPSink*
QoSVorbisAudioRTPSink::createNew(UsageEnvironment& env, Groupsock* RTPgs, u_int8_t rtpPayloadFormat,
		u_int32_t rtpTimestampFrequency, unsigned numChannels,
		u_int8_t* identificationHeader, unsigned identificationHeaderSize,
		u_int8_t* commentHeader, unsigned commentHeaderSize,
		u_int8_t* setupHeader, unsigned setupHeaderSize,
		u_int32_t identField) {
	return new QoSVorbisAudioRTPSink(env, RTPgs, rtpPayloadFormat,
			rtpTimestampFrequency, numChannels,
			identificationHeader, identificationHeaderSize,
			commentHeader, commentHeaderSize,
			setupHeader, setupHeaderSize, identField);
}

QoSVorbisAudioRTPSink
::QoSVorbisAudioRTPSink(UsageEnvironment& env, Groupsock* RTPgs,
		u_int8_t rtpPayloadFormat, u_int32_t rtpTimestampFrequency, unsigned numChannels,
		u_int8_t* identificationHeader, unsigned identificationHeaderSize,
		u_int8_t* commentHeader, unsigned commentHeaderSize,
		u_int8_t* setupHeader, unsigned setupHeaderSize,
		u_int32_t identField)
	: VorbisAudioRTPSink(env, RTPgs, rtpPayloadFormat,
			rtpTimestampFrequency, numChannels,
			identificationHeader, identificationHeaderSize,
			commentHeader, commentHeaderSize,
			setupHeader, setupHeaderSize, identField) {
	qos_server_add_sink("Vorbis", this);
}


QoSVorbisAudioRTPSink
::~QoSVorbisAudioRTPSink() { qos_server_remove_sink(this); }

//////////////////////////////////////////////////////////////////////////////

QoSTheoraVideoRTPSink*
QoSTheoraVideoRTPSink
::createNew(UsageEnvironment& env, Groupsock* RTPgs, u_int8_t rtpPayloadFormat,
		u_int8_t* identificationHeader, unsigned identificationHeaderSize,
		u_int8_t* commentHeader, unsigned commentHeaderSize,
		u_int8_t* setupHeader, unsigned setupHeaderSize,
		u_int32_t identField) {
	return new QoSTheoraVideoRTPSink(env, RTPgs, rtpPayloadFormat,
			identificationHeader, identificationHeaderSize,
			commentHeader, commentHeaderSize,
			setupHeader, setupHeaderSize, identField);
}

QoSTheoraVideoRTPSink
::QoSTheoraVideoRTPSink(UsageEnvironment& env, Groupsock* RTPgs,
		u_int8_t rtpPayloadFormat,
		u_int8_t* identificationHeader, unsigned identificationHeaderSize,
		u_int8_t* commentHeader, unsigned commentHeaderSize,
		u_int8_t* setupHeader, unsigned setupHeaderSize,
		u_int32_t identField)
	: TheoraVideoRTPSink(env, RTPgs, rtpPayloadFormat,
			identificationHeader, identificationHeaderSize,
			commentHeader, commentHeaderSize,
			setupHeader, setupHeaderSize, identField) {
	qos_server_add_sink("Theora", this);
}

QoSTheoraVideoRTPSink
::~QoSTheoraVideoRTPSink() { qos_server_remove_sink(this); }

//////////////////////////////////////////////////////////////////////////////

QoSH264VideoRTPSink*
QoSH264VideoRTPSink
::createNew(UsageEnvironment& env, Groupsock* RTPgs, unsigned char rtpPayloadFormat) {
	return new QoSH264VideoRTPSink(env, RTPgs, rtpPayloadFormat);
}

QoSH264VideoRTPSink*
QoSH264VideoRTPSink
::createNew(UsageEnvironment& env, Groupsock* RTPgs, unsigned char rtpPayloadFormat,
		u_int8_t const* sps, unsigned spsSize, u_int8_t const* pps, unsigned ppsSize) {
	return new QoSH264VideoRTPSink(env, RTPgs, rtpPayloadFormat, sps, spsSize, pps, ppsSize);
}

QoSH264VideoRTPSink
::QoSH264VideoRTPSink(UsageEnvironment& env, Groupsock* RTPgs, unsigned char rtpPayloadFormat,
		u_int8_t const* sps, unsigned spsSize,
		u_int8_t const* pps, unsigned ppsSize)
		: H264VideoRTPSink(env, RTPgs, rtpPayloadFormat, sps, spsSize, pps, ppsSize) {

	qos_server_add_sink("H.264", this);
}

QoSH264VideoRTPSink
::~QoSH264VideoRTPSink() { qos_server_remove_sink(this); }

//////////////////////////////////////////////////////////////////////////////

QoSH265VideoRTPSink*
QoSH265VideoRTPSink
::createNew(UsageEnvironment& env, Groupsock* RTPgs, unsigned char rtpPayloadFormat) {
	return new QoSH265VideoRTPSink(env, RTPgs, rtpPayloadFormat);
}

QoSH265VideoRTPSink*
QoSH265VideoRTPSink
::createNew(UsageEnvironment& env, Groupsock* RTPgs, unsigned char rtpPayloadFormat,
		u_int8_t const* vps, unsigned vpsSize,
		u_int8_t const* sps, unsigned spsSize,
		u_int8_t const* pps, unsigned ppsSize) {
	return new QoSH265VideoRTPSink(env, RTPgs, rtpPayloadFormat, vps, vpsSize, sps, spsSize, pps, ppsSize);
}

QoSH265VideoRTPSink
::QoSH265VideoRTPSink(UsageEnvironment& env, Groupsock* RTPgs, unsigned char rtpPayloadFormat,
		u_int8_t const* vps, unsigned vpsSize,
		u_int8_t const* sps, unsigned spsSize,
		u_int8_t const* pps, unsigned ppsSize)
		: H265VideoRTPSink(env, RTPgs, rtpPayloadFormat, vps, vpsSize, sps, spsSize, pps, ppsSize) {

	qos_server_add_sink("H.265", this);
}

QoSH265VideoRTPSink
::~QoSH265VideoRTPSink() { qos_server_remove_sink(this); }

//////////////////////////////////////////////////////////////////////////////

QoSVP8VideoRTPSink*
QoSVP8VideoRTPSink
::createNew(UsageEnvironment& env, Groupsock* RTPgs, unsigned char rtpPayloadFormat) {
	return new QoSVP8VideoRTPSink(env, RTPgs, rtpPayloadFormat);
}

QoSVP8VideoRTPSink
::QoSVP8VideoRTPSink(UsageEnvironment& env, Groupsock* RTPgs, unsigned char rtpPayloadFormat)
	: VP8VideoRTPSink(env, RTPgs, rtpPayloadFormat) {
	qos_server_add_sink("VP8", this);
}

QoSVP8VideoRTPSink
::~QoSVP8VideoRTPSink() { qos_server_remove_sink(this); }

//////////////////////////////////////////////////////////////////////////////

