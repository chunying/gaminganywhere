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

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#if 0
/* XXX: not include GroupsockHelper.hh due to the conflict on gettimeofday() */
#include "GroupsockHelper.hh"
#else
/* XXX: this one must be consistent to that defined in GroupsockHelper.hh" */
unsigned increaseReceiveBufferTo(UsageEnvironment& env,
		 int socket, unsigned requestedSize);
#endif

#ifndef ANDROID
#include "vsource.h"
#endif
#include "rtspclient.h"

#include "ga-common.h"
#include "ga-conf.h"
#include "ga-avcodec.h"
#include "controller.h"
#include "minih264.h"
#include "qosreport.h"
#ifdef ANDROID
#include "android-decoders.h"
#endif
#include "vconverter.h"

#ifdef ANDROID
#include "libgaclient.h"
#endif

#include <string.h>
#include <list>
#include <map>
using namespace std;

#ifndef	AVCODEC_MAX_AUDIO_FRAME_SIZE
#define	AVCODEC_MAX_AUDIO_FRAME_SIZE	192000 // 1 second of 48khz 32bit audio
#endif

#define RCVBUF_SIZE		2097152

#define	COUNT_FRAME_RATE	600	// every N frames

#define MAX_TOLERABLE_VIDEO_DELAY_US		200000LL	/* 200 ms */
#define	DEF_RTP_PACKET_REORDERING_THRESHOLD	300000		/* 300 ms */

//#define PRINT_LATENCY	1
//#define SAVE_ENC        "save.raw"

#ifdef SAVE_ENC
static FILE *fout = NULL;
#endif

struct RTSPConf *rtspconf;
int image_rendered = 0;

static int video_sess_fmt = -1;
static int audio_sess_fmt = -1;
static const char *video_codec_name = NULL;
static const char *audio_codec_name = NULL;
static enum AVCodecID video_codec_id = AV_CODEC_ID_NONE;
static enum AVCodecID audio_codec_id = AV_CODEC_ID_NONE;
#define	MAX_FRAMING_SIZE	8
static int video_framing = 0;
static int audio_framing = 0;
static int log_rtp = 0;

#ifdef COUNT_FRAME_RATE
static int cf_frame[VIDEO_SOURCE_CHANNEL_MAX];
static struct timeval cf_tv0[VIDEO_SOURCE_CHANNEL_MAX];
static struct timeval cf_tv1[VIDEO_SOURCE_CHANNEL_MAX];
static long long cf_interval[VIDEO_SOURCE_CHANNEL_MAX];
#endif

// save files
static FILE *savefp_yuv = NULL;
static FILE *savefp_yuvts = NULL;

static unsigned rtp_packet_reordering_threshold = DEF_RTP_PACKET_REORDERING_THRESHOLD;

static unsigned rtspClientCount = 0; // Counts how many streams (i.e., "RTSPClient"s) are currently in use.

// RTSP 'response handlers':
static void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString);
static void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString);
static void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString);

static void subsessionAfterPlaying(void* clientData);
static void subsessionByeHandler(void* clientData);
static void streamTimerHandler(void* clientData);

static RTSPClient * openURL(UsageEnvironment& env, char const* rtspURL);
static void setupNextSubsession(RTSPClient* rtspClient);
static void shutdownStream(RTSPClient* rtspClient, int exitCode = 1);

//static char eventLoopWatchVariable = 0;

// for audio: ref from ffmpeg tutorial03
//	http://dranger.com/ffmpeg/tutorial03.html
struct PacketQueue {
	list<AVPacket> queue;
	int size;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
};

static RTSPThreadParam *rtspParam = NULL;
static AVCodecContext *vdecoder[VIDEO_SOURCE_CHANNEL_MAX];
static map<unsigned short,int> port2channel;
static AVFrame *vframe[VIDEO_SOURCE_CHANNEL_MAX];
static AVCodecContext *adecoder = NULL;
static AVFrame *aframe = NULL;

static int packet_queue_initialized = 0;
static int packet_queue_limit = 5;	// limit the queue size
static int packet_queue_dropfactor = 2;	// default drop half
static PacketQueue audioq;

void
packet_queue_init(PacketQueue *q) {
	int val;
	char buf[8];
	packet_queue_initialized = 1;
	q->queue.clear();
	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->cond, NULL);
	if(ga_conf_readv("audio-playback-queue-limit", buf, sizeof(buf)) != NULL) {
		// must ensure that we have configured audio-playback-queue-limit
		if((val = ga_conf_readint("audio-playback-queue-limit")) >= 0) {
			packet_queue_limit = val;
		}
	}
	if((val = ga_conf_readint("audio-playback-queue-dropfactor")) > 0) {
		packet_queue_dropfactor = val;
	}
	ga_error("packet queue: initialized - limit %d%s; dropfactor %d%s\n",
		packet_queue_limit,
		packet_queue_limit <= 0 ? " (unlimited)" : "",
		packet_queue_dropfactor,
		packet_queue_dropfactor <= 0 ? " (no drop)" : "");
	return;
}

int
packet_queue_put(PacketQueue *q, AVPacket *pkt) {
	if(av_dup_packet(pkt) < 0) {
		rtsperror("packet queue put failed\n");
		return -1;
	}
	pthread_mutex_lock(&q->mutex);
	q->queue.push_back(*pkt);
	q->size += pkt->size;
	pthread_mutex_unlock(&q->mutex);
	pthread_cond_signal(&q->cond);
	return 0;
}

int
packet_queue_get(PacketQueue *q, AVPacket *pkt, int block) {
	int ret;
	if(packet_queue_initialized == 0) {
		rtsperror("packet queue not initialized\n");
		return -1;
	}
	pthread_mutex_lock(&q->mutex);
	for(;;) {
		if(q->queue.size() > 0) {
			*pkt = q->queue.front();
			q->queue.pop_front();
			q->size -= pkt->size;
			ret = 1;
			break;
		} else if(!block) {
			ret = 0;
			break;
		} else {
			struct timespec ts;
#if defined(__APPLE__) || defined(WIN32)
			struct timeval tv;
			gettimeofday(&tv, NULL);
			ts.tv_sec = tv.tv_sec;
			ts.tv_nsec = tv.tv_usec * 1000;
#else
			clock_gettime(CLOCK_REALTIME, &ts);
#endif
			ts.tv_nsec += 50000000LL /* 50ms */;
			if(ts.tv_nsec >= 1000000000LL) {
				ts.tv_sec++;
				ts.tv_nsec -= 1000000000LL;
			}
			if(pthread_cond_timedwait(&q->cond, &q->mutex, &ts) != 0) {
				ret = -1;
				break;
			}
			//pthread_cond_wait(&q->cond, &q->mutex);
		}
	}
	pthread_mutex_unlock(&q->mutex);
	return ret;
}

int
packet_queue_drop(PacketQueue *q) {
	int dropped, count = 0;
	AVPacket pkt;
	// dropping enabled?
	if(packet_queue_limit <= 0 || packet_queue_dropfactor <= 0)
		return 0;
	pthread_mutex_lock(&q->mutex);
	// queue size exceeded?
	if(q->queue.size() <= packet_queue_limit) {
		pthread_mutex_unlock(&q->mutex);
		return 0;
	}
	// start dropping
	dropped = q->queue.size() / packet_queue_dropfactor;
	// keep at least one
	if(dropped == q->queue.size())
		dropped--;
	while(dropped-- > 0) {
		if(q->queue.size() <= 0)
			break;
		pkt = q->queue.front();
		q->queue.pop_front();
		q->size -= pkt.size;
		count++;
	}
	pthread_mutex_unlock(&q->mutex);
	return count;
}

UsageEnvironment&
operator<<(UsageEnvironment& env, const RTSPClient& rtspClient) {
	return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

UsageEnvironment&
operator<<(UsageEnvironment& env, const MediaSubsession& subsession) {
	return env << subsession.mediumName() << "/" << subsession.codecName();
}

void
rtsperror(const char *fmt, ...) {
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	va_list ap;
	pthread_mutex_lock(&mutex);
	va_start(ap, fmt);
#ifdef ANDROID
	__android_log_vprint(ANDROID_LOG_INFO, "ga_log", fmt, ap);
#else
	vfprintf(stderr, fmt, ap);
#endif
	va_end(ap);
	pthread_mutex_unlock(&mutex);
	return;
}

static unsigned char *
decode_sprop(AVCodecContext *ctx, const char *sprop) {
	unsigned char startcode[] = {0, 0, 0, 1};
	int sizemax = ctx->extradata_size + strlen(sprop) * 3;
	unsigned char *extra = (unsigned char*) malloc(sizemax);
	unsigned char *dest = extra;
	int extrasize = 0;
	int spropsize = strlen(sprop);
	char *mysprop = strdup(sprop);
	unsigned char *tmpbuf = (unsigned char *) strdup(sprop);
	char *s0 = mysprop, *s1;
	// already have extradata?
	if(ctx->extradata) {
		bcopy(ctx->extradata, extra, ctx->extradata_size);
		extrasize = ctx->extradata_size;
		dest += extrasize;
	}
	// start converting
	while(*s0) {
		int blen, more = 0;
		for(s1 = s0; *s1; s1++) {
			if(*s1 == ',' || *s1 == '\0')
				break;
		}
		if(*s1 == ',')
			more = 1;
		*s1 = '\0';
		if((blen = av_base64_decode(tmpbuf, s0, spropsize)) > 0) {
			int offset = 0;
			// no start code?
			if(memcmp(startcode, tmpbuf, sizeof(startcode)) != 0) {
				bcopy(startcode, dest, sizeof(startcode));
				offset += sizeof(startcode);
			}
			bcopy(tmpbuf, dest + offset, blen);
			dest += offset + blen;
			extrasize += offset + blen;
		}
		s0 = s1;
		if(more) {
			s0++;
		}
	}
	// release
	free(mysprop);
	free(tmpbuf);
	// show decoded sprop
	if(extrasize > 0) {
		if(ctx->extradata)
			free(ctx->extradata);
		ctx->extradata = extra;
		ctx->extradata_size = extrasize;
#ifdef SAVE_ENC
		if(fout != NULL) {
			fwrite(extra, sizeof(char), extrasize, fout);
		}
#endif
		return ctx->extradata;
	}
	free(extra);
	return NULL;
}

int
init_vdecoder(int channel, const char *sprop) {
	AVCodec *codec = NULL; //rtspconf->video_decoder_codec;
	AVCodecContext *ctx;
	AVFrame *frame;
	const char **names = NULL;
	//
	if(channel > VIDEO_SOURCE_CHANNEL_MAX) {
		rtsperror("video decoder(%d): too many decoders.\n", channel);
		return -1;
	}
	//
	if(video_codec_name == NULL) {
		rtsperror("video decoder: no codec specified.\n");
		return -1;
	}
	if((names = ga_lookup_ffmpeg_decoders(video_codec_name)) == NULL) {
		rtsperror("video decoder: cannot find decoder names for %s\n", video_codec_name);
		return -1;
	}
	video_codec_id = ga_lookup_codec_id(video_codec_name);
	if((codec = ga_avcodec_find_decoder(names, AV_CODEC_ID_NONE)) == NULL) {
		rtsperror("video decoder: cannot find the decoder for %s\n", video_codec_name);
		return -1;
	}
	rtspconf->video_decoder_codec = codec;
	rtsperror("video decoder: use decoder %s\n", names[0]);
	//
	if((frame = av_frame_alloc()) == NULL) {
		rtsperror("video decoder(%d): allocate frame failed\n", channel);
		return -1;
	}
	if((ctx = avcodec_alloc_context3(codec)) == NULL) {
		rtsperror("video decoder(%d): cannot allocate context\n", channel);
		return -1;
	}
	if(codec->capabilities & CODEC_CAP_TRUNCATED) {
		rtsperror("video decoder(%d): codec support truncated data\n", channel);
		ctx->flags |= CODEC_FLAG_TRUNCATED;
	}
	if(sprop != NULL) {
		if(decode_sprop(ctx, sprop) != NULL) {
			int extrasize = ctx->extradata_size;
			rtsperror("video decoder(%d): sprop configured with '%s', decoded-size=%d\n",
				channel, sprop, extrasize);
			fprintf(stderr, "SPROP = [");
			for(unsigned char *ptr = ctx->extradata; extrasize > 0; extrasize--) {
				fprintf(stderr, " %02x", *ptr++);
			}
			fprintf(stderr, " ]\n");
		}
	}
	if(avcodec_open2(ctx, codec, NULL) != 0) {
		rtsperror("video decoder(%d): cannot open decoder\n", channel);
		return -1;
	}
	rtsperror("video decoder(%d): codec %s (%s)\n", channel, codec->name, codec->long_name);
	//
	vdecoder[channel] = ctx;
	vframe[channel] = frame;
	//
	return 0;
}

int
init_adecoder() {
	AVCodec *codec = NULL; //rtspconf->audio_decoder_codec;
	AVCodecContext *ctx;
	const char **names = NULL;
	//
	if(audio_codec_name == NULL) {
		rtsperror("audio decoder: no codec specified.\n");
		return -1;
	}
	if((names = ga_lookup_ffmpeg_decoders(audio_codec_name)) == NULL) {
		rtsperror("audio decoder: cannot find decoder names for %s\n", audio_codec_name);
		return -1;
	}
	audio_codec_id = ga_lookup_codec_id(audio_codec_name);
	if((codec = ga_avcodec_find_decoder(names, AV_CODEC_ID_NONE)) == NULL) {
		rtsperror("audio decoder: cannot find the decoder for %s\n", audio_codec_name);
		return -1;
	}
	rtspconf->audio_decoder_codec = codec;
	rtsperror("audio decoder: use decoder %s\n", names[0]);
	//
#ifdef ANDROID
	if(rtspconf->builtin_audio_decoder == 0) {
#endif
	packet_queue_init(&audioq);
#ifdef ANDROID
	}
#endif
	//
	if((aframe = av_frame_alloc()) == NULL) {
		rtsperror("audio decoder: allocate frame failed\n");
		return -1;
	}
	if((ctx = avcodec_alloc_context3(codec)) == NULL) {
		rtsperror("audio decoder: cannot allocate context\n");
		return -1;
	}
	// some audio decoders will need these, e.g. opus
	ctx->channels = rtspconf->audio_channels;
	ctx->sample_rate = rtspconf->audio_samplerate;
	if(ctx->channels == 1) {
		ctx->channel_layout = AV_CH_LAYOUT_MONO;
	} else if(ctx->channels == 2) {
		ctx->channel_layout = AV_CH_LAYOUT_STEREO;
	} else {
		rtsperror("audio decoder: unsupported number of channels (%d)\n",
			(int) ctx->channels);
		return -1;
	}
	rtsperror("audio decoder: %d channels, samplerate %d\n",
		(int) ctx->channels, (int) ctx->sample_rate);
	//
	if(avcodec_open2(ctx, codec, NULL) != 0) {
		rtsperror("audio decoder: cannot open decoder\n");
		return -1;
	}
	rtsperror("audio decoder: codec %s (%s)\n", codec->name, codec->long_name);
	adecoder = ctx;
	return 0;
}

//// packet loss monitor

typedef struct pktloss_record_s {
	/* XXX: ssrc is 32-bit, and seqnum is 16-bit */
	int reset;	/* 1 - this record should be reset */
	int lost;	/* count of lost packets */
	unsigned int ssrc;	/* SSRC */
	unsigned short initseq;	/* the 1st seqnum in the observation */
	unsigned short lastseq;	/* the last seqnum in the observation */
}	pktloss_record_t;

static map<unsigned int, pktloss_record_t> _pktmap;

int
pktloss_monitor_init() {
	_pktmap.clear();
	return 0;
}

void
pktloss_monitor_update(unsigned int ssrc, unsigned short seqnum) {
	map<unsigned int, pktloss_record_t>::iterator mi;
	if((mi = _pktmap.find(ssrc)) == _pktmap.end()) {
		pktloss_record_t r;
		r.reset = 0;
		r.lost = 0;
		r.ssrc = ssrc;
		r.initseq = seqnum;
		r.lastseq = seqnum;
		_pktmap[ssrc] = r;
		return;
	}
	if(mi->second.reset != 0) {
		mi->second.reset = 0;
		mi->second.lost = 0;
		mi->second.initseq = seqnum;
		mi->second.lastseq = seqnum;
		return;
	}
	if((seqnum-1) != mi->second.lastseq) {
		mi->second.lost += (seqnum - 1 - mi->second.lastseq);
	}
	mi->second.lastseq = seqnum;
	return;
}

void
pktloss_monitor_reset(unsigned int ssrc) {
	map<unsigned int, pktloss_record_t>::iterator mi;
	if((mi = _pktmap.find(ssrc)) != _pktmap.end()) {
		mi->second.reset = 1;
	}
	return;
}

int
pktloss_monitor_get(unsigned int ssrc, int *count = NULL, int reset = 0) {
	map<unsigned int, pktloss_record_t>::iterator mi;
	if((mi = _pktmap.find(ssrc)) == _pktmap.end())
		return -1;
	if(reset != 0)
		mi->second.reset = 1;
	if(count != NULL)
		*count = (mi->second.lastseq - mi->second.initseq) + 1;
	return mi->second.lost;
}

//// bandwidth estimator

typedef struct bwe_record_s {
	struct timeval initTime;
	unsigned int framecount;
	unsigned int pktcount;
	unsigned int pktloss;
	unsigned int bytesRcvd;
	unsigned short lastPktSeq;
	struct timeval lastPktRcvdTimestamp;
	unsigned int lastPktSentTimestamp;
	unsigned int lastPktSize;
	// for estimating capacity
	unsigned int samples;
	unsigned int totalBytes;
	unsigned int totalElapsed;
}	bwe_record_t;

static map<unsigned int,bwe_record_t> bwe_watchlist;

void
bandwidth_estimator_update(unsigned int ssrc, unsigned short seq, struct timeval rcvtv, unsigned int timestamp, unsigned int pktsize) {
	bwe_record_t r;
	map<unsigned int,bwe_record_t>::iterator mi;
	bool sampleframe = true;
	//
	if((mi = bwe_watchlist.find(ssrc)) == bwe_watchlist.end()) {
		bzero(&r, sizeof(r));
		r.initTime = rcvtv;
		r.framecount = 1;
		r.pktcount = 1;
		r.bytesRcvd = pktsize;
		r.lastPktSeq = seq;
		r.lastPktRcvdTimestamp = rcvtv;
		r.lastPktSentTimestamp = timestamp;
		r.lastPktSize = pktsize;
		bwe_watchlist[ssrc] = r;
		return;
	}
	// new frame?
	if(timestamp != mi->second.lastPktSentTimestamp) {
		mi->second.framecount++;
		sampleframe = false;
	}
	// no packet loss && is the same frame
	if((seq-1) == mi->second.lastPktSeq) {
		if(sampleframe) {
			unsigned cbw;
			unsigned elapsed = tvdiff_us(&rcvtv, &mi->second.lastPktRcvdTimestamp);
			//cbw = 8.0 * mi->second.lastPktSize / (elapsed / 1000000.0);
			//ga_error("XXX: sampled bw = %u bps\n", cbw);
			mi->second.samples++;
			mi->second.totalElapsed += elapsed;
			mi->second.totalBytes += mi->second.lastPktSize;
			if(mi->second.framecount >= 240 && mi->second.samples >= 3000) {
				ctrlmsg_t m;
				cbw = 8.0 * mi->second.totalBytes / (mi->second.totalElapsed / 1000000.0);
				ga_error("XXX: received: %uKB; capacity = %.3fKbps (%d samples); loss-rate = %.2f%% (%u/%u); average per-frame overhead factor = %.2f\n",
					mi->second.bytesRcvd / 1024,
					cbw / 1024.0, mi->second.samples,
					100.0 * mi->second.pktloss / mi->second.pktcount,
					mi->second.pktloss, mi->second.pktcount,
					1.0 * mi->second.pktcount / mi->second.framecount);
				// send back to the server
				ctrlsys_netreport(&m,
					tvdiff_us(&rcvtv, &mi->second.initTime),
					mi->second.framecount,
					mi->second.pktcount,
					mi->second.pktloss,
					mi->second.bytesRcvd,
					cbw);
				ctrl_client_sendmsg(&m, sizeof(ctrlmsg_system_netreport_t));
				//
				bzero(&mi->second, sizeof(bwe_record_t));
				mi->second.initTime = rcvtv;
				mi->second.framecount = 1;
			}
		}
	// has packet loss
	} else {
		unsigned short delta = (seq - mi->second.lastPktSeq - 1);
		mi->second.pktloss += delta;
		mi->second.pktcount += delta;
	}
	// update the rest
	mi->second.pktcount++;
	mi->second.bytesRcvd += pktsize;
	mi->second.lastPktSeq = seq;
	mi->second.lastPktRcvdTimestamp = rcvtv;
	mi->second.lastPktSentTimestamp = timestamp;
	mi->second.lastPktSize = pktsize;
	return;
}

////

#if defined(WIN32) && !defined(MSYS)
#pragma pack(push, 1)
#endif
struct rtp_pkt_minimum_s {
	unsigned short flags;
	unsigned short seqnum;
	unsigned int timestamp;
	unsigned int ssrc;
}
#if defined(WIN32) && !defined(MSYS)
#pragma pack(pop)
#else
__attribute__((__packed__))
#endif
;

typedef struct rtp_pkt_minimum_s rtp_pkt_minimum_t;

void
rtp_packet_handler(void *clientData, unsigned char *packet, unsigned &packetSize) {
	rtp_pkt_minimum_t *rtp = (rtp_pkt_minimum_t*) packet;
	unsigned int ssrc;
	unsigned short seqnum;
	unsigned short flags;
	unsigned int timestamp;
	struct timeval tv;
	if(packet == NULL || packetSize < 12)
		return;
	gettimeofday(&tv, NULL);
	ssrc = ntohl(rtp->ssrc);
	seqnum = ntohs(rtp->seqnum);
	flags = ntohs(rtp->flags);
	timestamp = ntohl(rtp->timestamp);
	//
	if(log_rtp > 0) {
#ifdef ANDROID
		ga_log("%10u.%06u log_rtp: flags %04x seq %u ts %u ssrc %u size %u\n",
			tv.tv_sec, tv.tv_usec, flags, seqnum, timestamp, ssrc, packetSize);
#else
		ga_log("log_rtp: flags %04x seq %u ts %u ssrc %u size %u\n",
			flags, seqnum, timestamp, ssrc, packetSize);
#endif
	}
	//
	bandwidth_estimator_update(ssrc, seqnum, tv, timestamp, packetSize);
	pktloss_monitor_update(ssrc, seqnum);
	//
	return;
}

//// drop frame feature

typedef struct drop_vframe_s {
	struct timeval tv_real_start;	// 1st wall-clock timestamp
	struct timeval tv_stream_start;	// 1st pkt timestamp
	int no_drop;			// keep N frames not dropped
}	drop_vframe_t;

static long long max_tolerable_video_delay_us = -1;
static drop_vframe_t drop_vframe_ctx[VIDEO_SOURCE_CHANNEL_MAX];

static void
drop_video_frame_init(long long max_delay_us) {
	bzero(&drop_vframe_ctx, sizeof(drop_vframe_ctx));
	max_tolerable_video_delay_us = max_delay_us;
	if(max_tolerable_video_delay_us > 0) {
		ga_error("rtspclient: max tolerable video delay = %lldus\n", max_tolerable_video_delay_us);
	} else {
		ga_error("rtspclient: max tolerable video delay disabled.\n");
	}
	return;
}

static int
drop_video_frame(int ch/*channel*/, unsigned char *buffer, int bufsize, struct timeval pts) {
	struct timeval now;
	// disabled?
	if(max_tolerable_video_delay_us <= 0)
		return 0;
	//
	gettimeofday(&now, NULL);
	if(drop_vframe_ctx[ch].tv_real_start.tv_sec == 0) {
		drop_vframe_ctx[ch].tv_real_start = now;
		drop_vframe_ctx[ch].tv_stream_start = pts;
		ga_error("rtspclient: frame dropping initialized real=%lu.%06ld stream=%lu.%06ld (latency=%lld).\n",
			now.tv_sec, now.tv_usec, pts.tv_sec, pts.tv_usec,
			max_tolerable_video_delay_us);
		return 0;
	}
	//
	do {
		if(video_codec_id == AV_CODEC_ID_H264) {
			int offset, nalt;
			// no frame start?
			if(buffer[0] != 0 && buffer[1] != 0) {
				break;
			}
			// valid framing?
			if(buffer[2] == 1) {
				offset = 3;
			} else if(buffer[2] == 0 && buffer[3] == 1) {
				offset = 4;
			} else {
				break;
			}
			nalt = buffer[offset] & 0x1f;
			if(nalt == 0x1c) { /* first byte 0x7c is an I-frame? */ 
				drop_vframe_ctx[ch].no_drop = 2;
			} else if(nalt == 0x07) { /* sps */
				drop_vframe_ctx[ch].no_drop = 4;
			} else if(nalt == 0x08) { /* pps */
				drop_vframe_ctx[ch].no_drop = 3;
			}
			//
			long long dstream = tvdiff_us(&pts, &drop_vframe_ctx[ch].tv_stream_start);
			long long dreal = tvdiff_us(&now, &drop_vframe_ctx[ch].tv_real_start);
			if(drop_vframe_ctx[ch].no_drop > 0)
				drop_vframe_ctx[ch].no_drop--;
			if(dreal-dstream > max_tolerable_video_delay_us) {
				if(drop_vframe_ctx[ch].no_drop > 0)
					break;
				ga_error("drop_frame: packet dropped (delay=%lldus)\n", dreal-dstream);
				return 1;
			}
		}
	} while(0);
	//
	return 0;
}

////

static int
play_video_priv(int ch/*channel*/, unsigned char *buffer, int bufsize, struct timeval pts) {
	AVPacket avpkt;
	int got_picture, len;
#ifndef ANDROID
	union SDL_Event evt;
#endif
	dpipe_buffer_t *data = NULL;
	AVPicture *dstframe = NULL;
	struct timeval ftv;
	static unsigned fcount = 0;
#ifdef PRINT_LATENCY
	static struct timeval btv0 = {0, 0};
	struct timeval ptv0, ptv1, btv1;
	// measure buffering time
	if(btv0.tv_sec == 0) {
		gettimeofday(&btv0, NULL);
	} else {
		long long dt;
		gettimeofday(&btv1, NULL);
		dt = tvdiff_us(&btv1, &btv0);
		if(dt < 2000000) {
			ga_aggregated_print(0x8000, 599, dt);
		}
		btv0 = btv1;
	}
#endif
	// drop the frame?
	if(drop_video_frame(ch, buffer, bufsize, pts)) {
		return bufsize;
	}
	//
#ifdef SAVE_ENC
	if(fout != NULL) {
		fwrite(buffer, sizeof(char), bufsize, fout);
	}
#endif
	//
	av_init_packet(&avpkt);
	avpkt.size = bufsize;
	avpkt.data = buffer;
#if 0	// XXX: dump nal units
	do {
		int codelen = 0;
		unsigned char *ptr = NULL;
		//
		fprintf(stderr, "[XXX-nalcode]");
		for(	ptr = ga_find_startcode(avpkt.data, avpkt.data+avpkt.size, &codelen);
			ptr != NULL;
			ptr = ga_find_startcode(ptr+codelen, avpkt.data+avpkt.size, &codelen)) {
			//
			fprintf(stderr, " (+%d|%d)-%02x", ptr-avpkt.data, codelen, ptr[codelen] & 0x1f);
		}
		fprintf(stderr, "\n");
	} while(0);
#endif
	//
	while(avpkt.size > 0) {
		//
#ifdef PRINT_LATENCY
		gettimeofday(&ptv0, NULL);
#endif
		if((len = avcodec_decode_video2(vdecoder[ch], vframe[ch], &got_picture, &avpkt)) < 0) {
			//rtsperror("decode video frame %d error\n", frame);
			break;
		}
		if(got_picture) {
#ifdef COUNT_FRAME_RATE
			cf_frame[ch]++;
			if(cf_tv0[ch].tv_sec == 0) {
				gettimeofday(&cf_tv0[ch], NULL);
			}
			if(cf_frame[ch] == COUNT_FRAME_RATE) {
				gettimeofday(&cf_tv1[ch], NULL);
				cf_interval[ch] = tvdiff_us(&cf_tv1[ch], &cf_tv0[ch]);
				rtsperror("# %u.%06u player frame rate: decoder %d @ %.4f fps\n",
					cf_tv1[ch].tv_sec,
					cf_tv1[ch].tv_usec,
					ch,
					1000000.0 * cf_frame[ch] / cf_interval[ch]);
				cf_tv0[ch] = cf_tv1[ch];
				cf_frame[ch] = 0;
			}
#endif
			// create surface & bitmap for the first time
			pthread_mutex_lock(&rtspParam->surfaceMutex[ch]);
			if(rtspParam->swsctx[ch] == NULL) {
				rtspParam->width[ch] = vframe[ch]->width;
				rtspParam->height[ch] = vframe[ch]->height;
				rtspParam->format[ch] = (AVPixelFormat) vframe[ch]->format;
#ifdef ANDROID
				create_overlay(ch, vframe[0]->width, vframe[0]->height, (AVPixelFormat) vframe[0]->format);
#else
				pthread_mutex_unlock(&rtspParam->surfaceMutex[ch]);
				bzero(&evt, sizeof(evt));
				evt.user.type = SDL_USEREVENT;
				evt.user.timestamp = time(0);
				evt.user.code = SDL_USEREVENT_CREATE_OVERLAY;
				evt.user.data1 = rtspParam;
				evt.user.data2 = (void*) ch;
				SDL_PushEvent(&evt);
				// skip the initial frame:
				// for event handler to create/setup surfaces
				goto skip_frame;
#endif
			}
			pthread_mutex_unlock(&rtspParam->surfaceMutex[ch]);
			// copy into pool
			data = dpipe_get(rtspParam->pipe[ch]);
			dstframe = (AVPicture*) data->pointer;
			// do scaling
			if(vframe[ch]->width  == rtspParam->width[ch]
			&& vframe[ch]->height == rtspParam->height[ch]
			&& vframe[ch]->format == rtspParam->format[ch]) {
				/* fast path? no lookup on converter */
				sws_scale(rtspParam->swsctx[ch],
					// source: decoded frame
					vframe[ch]->data, vframe[ch]->linesize,
					0, vframe[ch]->height,
					// destination: texture
					dstframe->data, dstframe->linesize);
			} else {
				/* slower path - need to lookup converter */
				SwsContext *swsctx;
				if((swsctx = create_frame_converter(
						vframe[ch]->width,
						vframe[ch]->height,
						(AVPixelFormat) vframe[ch]->format,
						rtspParam->width[ch],
						rtspParam->height[ch],
#ifdef ANDROID
						PIX_FMT_RGB565
#else
						(AVPixelFormat) rtspParam->format[ch]
#endif
						)) == NULL) {
					ga_error("*** FATAL *** Create frame converter failed.\n");
#ifdef ANDROID
					rtspParam->quitLive555 = 1;
					return -1;
#else
					exit(-1);
#endif
				}
				sws_scale(swsctx,
					// source: decoded frame
					vframe[ch]->data, vframe[ch]->linesize,
					0, vframe[ch]->height,
					// destination: texture
					dstframe->data, dstframe->linesize);
			}
			if(ch==0 && savefp_yuv != NULL) {
				ga_save_yuv420p(savefp_yuv, vframe[0]->width, vframe[0]->height, dstframe->data, dstframe->linesize);
				if(savefp_yuvts != NULL) {
					gettimeofday(&ftv, NULL);
					ga_save_printf(savefp_yuvts, "Frame #%08d: %u.%06u\n", fcount++, ftv.tv_sec, ftv.tv_usec);
				}
			}
			dpipe_store(rtspParam->pipe[ch], data);
			// request to render it
#ifdef PRINT_LATENCY
			gettimeofday(&ptv1, NULL);
			ga_aggregated_print(0x8001, 601, tvdiff_us(&ptv1, &ptv0));
#endif
#ifdef ANDROID
			requestRender(rtspParam->jnienv);
#else
			bzero(&evt, sizeof(evt));
			evt.user.type = SDL_USEREVENT;
			evt.user.timestamp = time(0);
			evt.user.code = SDL_USEREVENT_RENDER_IMAGE;
			evt.user.data1 = rtspParam;
			evt.user.data2 = (void*) ch;
			SDL_PushEvent(&evt);
#endif
		}
skip_frame:
		avpkt.size -= len;
		avpkt.data += len;
	}
	return avpkt.size;
}

#define	PRIVATE_BUFFER_SIZE	1048576

struct decoder_buffer {
	unsigned int privbuflen;
	unsigned char *privbuf;
	struct timeval lastpts;
	// for alignment
	unsigned int offset;
	unsigned char *privbuf_unaligned;
};

static struct decoder_buffer db[VIDEO_SOURCE_CHANNEL_MAX];

static void
deinit_decoder_buffer() {
	int i;
	for(i = 0; i < VIDEO_SOURCE_CHANNEL_MAX; i++) {
		if(db[i].privbuf_unaligned != NULL) {
			free(db[i].privbuf_unaligned);
		}
	}
	bzero(db, sizeof(db));
	return;
}

static int
init_decoder_buffer() {
	int i;
	//
	deinit_decoder_buffer();
	//
	for(i = 0; i < VIDEO_SOURCE_CHANNEL_MAX; i++) {
		db[i].privbuf_unaligned = (unsigned char*) malloc(PRIVATE_BUFFER_SIZE+16);
		if(db[i].privbuf_unaligned == NULL) {
			rtsperror("FATAL: cannot allocate private buffer (%d:%d bytes): %s\n",
				i, PRIVATE_BUFFER_SIZE, strerror(errno));
			goto adb_failed;
		}
#if 0
#if defined(__LP64__) || defined(_LP64) || defined(WIN64) /* 64-bit */
		db[i].offset = 16 - (((unsigned long long) db[i].privbuf_unaligned) & 0x0f);
#else
		db[i].offset = 16 - (((unsigned) db[i].privbuf_unaligned) & 0x0f);
#endif
#else
		db[i].offset = 16 - (((size_t) db[i].privbuf_unaligned) & 0x0f);
#endif
		db[i].privbuf = db[i].privbuf_unaligned + db[i].offset;
	}
	return 0;
adb_failed:
	deinit_decoder_buffer();
	return -1;
}

static void
play_video(int channel, unsigned char *buffer, int bufsize, struct timeval pts, bool marker) {
	struct decoder_buffer *pdb = &db[channel];
	int left;
	//
	if(bufsize <= 0 || buffer == NULL) {
		rtsperror("empty buffer?\n");
		return;
	}
#ifdef ANDROID
	if(rtspconf->builtin_video_decoder != 0) {
		//////// Work with built-in decoders
		if(video_codec_id == AV_CODEC_ID_H264) {
			if(android_decode_h264(rtspParam, buffer, bufsize, pts, marker) < 0)
				return;
		} else if(video_codec_id == AV_CODEC_ID_VP8) {
			if(android_decode_vp8(rtspParam, buffer, bufsize, pts, marker) < 0)
				return;
		}
		image_rendered = 1;
	} else {
	//////// Work with ffmpeg
#endif
	if(pts.tv_sec != pdb->lastpts.tv_sec
	|| pts.tv_usec != pdb->lastpts.tv_usec) {
		if(pdb->privbuflen > 0) {
			//fprintf(stderr, "DEBUG: video pts=%08ld.%06ld\n",
			//	lastpts.tv_sec, lastpts.tv_usec);
			left = play_video_priv(channel, pdb->privbuf,
				pdb->privbuflen, pdb->lastpts);
			if(left > 0) {
				bcopy(pdb->privbuf + pdb->privbuflen - left,
					pdb->privbuf, left);
				pdb->privbuflen = left;
				rtsperror("decoder: %d bytes left, preserved for next round\n", left);
			} else {
				pdb->privbuflen = 0;
			}
		}
		pdb->lastpts = pts;
	}
	if(pdb->privbuflen + bufsize <= PRIVATE_BUFFER_SIZE) {
		bcopy(buffer, &pdb->privbuf[pdb->privbuflen], bufsize);
		pdb->privbuflen += bufsize;
		if(marker && pdb->privbuflen > 0) {
			left = play_video_priv(channel, pdb->privbuf,
				pdb->privbuflen, pdb->lastpts);
			if(left > 0) {
				bcopy(pdb->privbuf + pdb->privbuflen - left,
					pdb->privbuf, left);
				pdb->privbuflen = left;
				rtsperror("decoder: %d bytes left, leave for next round\n", left);
			} else {
				pdb->privbuflen = 0;
			}
		}
	} else {
		rtsperror("WARNING: video private buffer overflow.\n");
		left = play_video_priv(channel, pdb->privbuf,
				pdb->privbuflen, pdb->lastpts);
		if(left > 0) {
			bcopy(pdb->privbuf + pdb->privbuflen - left,
				pdb->privbuf, left);
			pdb->privbuflen = left;
			rtsperror("decoder: %d bytes left, leave for next round\n", left);
		} else {
			pdb->privbuflen = 0;
		}
	}
#ifdef ANDROID
	}
#endif
	return;
}

static const int abmaxsize = AVCODEC_MAX_AUDIO_FRAME_SIZE*4;
static unsigned char *audiobuf = NULL;
static unsigned int absize = 0;
static unsigned int abpos = 0;
// need a converter?
static struct SwrContext *swrctx = NULL;
static unsigned char *convbuf = NULL;
static int max_decoder_size = 0;
static int audio_start = 0;

unsigned char *
audio_buffer_init() {
	if(audiobuf == NULL) {
		audiobuf = (unsigned char*) malloc(abmaxsize);
		if(audiobuf == NULL) {
			return NULL;
		}
	}
	return audiobuf;
}

int
audio_buffer_decode(AVPacket *pkt, unsigned char *dstbuf, int dstlen) {
	const unsigned char *srcplanes[SWR_CH_MAX];
	unsigned char *dstplanes[SWR_CH_MAX];
	unsigned char *saveptr;
	int filled = 0;
	//
	saveptr = pkt->data;
	while(pkt->size > 0) {
		int len, got_frame = 0;
		unsigned char *srcbuf = NULL;
		int datalen = 0;
		//
		av_frame_unref(aframe);
		if((len = avcodec_decode_audio4(adecoder, aframe, &got_frame, pkt)) < 0) {
			rtsperror("decode audio failed.\n");
			return -1;
		}
		if(got_frame == 0) {
			pkt->size -= len;
			pkt->data += len;
			continue;
		}
		//
		if(aframe->format == rtspconf->audio_device_format) {
			datalen = av_samples_get_buffer_size(NULL,
					aframe->channels/*rtspconf->audio_channels*/,
					aframe->nb_samples,
					(AVSampleFormat) aframe->format, 1/*no-alignment*/);
			srcbuf = aframe->data[0];
		} else {
			// aframe->format != rtspconf->audio_device_format
			// need conversion!
			if(swrctx == NULL) {
				if((swrctx = swr_alloc_set_opts(NULL,
						rtspconf->audio_device_channel_layout,
						rtspconf->audio_device_format,
						rtspconf->audio_samplerate,
						aframe->channel_layout,
						(AVSampleFormat) aframe->format,
						aframe->sample_rate,
						0, NULL)) == NULL) {
					rtsperror("audio decoder: cannot allocate swrctx.\n");
					return -1;
				}
				if(swr_init(swrctx) < 0) {
					rtsperror("audio decoder: cannot initialize swrctx.\n");
					return -1;
				}
				max_decoder_size = av_samples_get_buffer_size(NULL,
						rtspconf->audio_channels,
						rtspconf->audio_samplerate*2/* max buffer for 2 seconds */,
						rtspconf->audio_device_format, 1/*no-alignment*/);
				if((convbuf = (unsigned char*) malloc(max_decoder_size)) == NULL) {
					rtsperror("audio decoder: cannot allocate conversion buffer.\n");
					return -1;
				}
				rtsperror("audio decoder: on-the-fly audio format conversion enabled.\n");
				rtsperror("audio decoder: convert from %dch(%x)@%dHz (%s) to %dch(%x)@%dHz (%s).\n",
						(int) aframe->channels, (int) aframe->channel_layout, (int) aframe->sample_rate,
						av_get_sample_fmt_name((AVSampleFormat) aframe->format),
						(int) rtspconf->audio_channels,
						(int) rtspconf->audio_device_channel_layout,
						(int) rtspconf->audio_samplerate,
						av_get_sample_fmt_name(rtspconf->audio_device_format));
			}
			datalen = av_samples_get_buffer_size(NULL,
					rtspconf->audio_channels,
					aframe->nb_samples,
					rtspconf->audio_device_format,
					1/*no-alignment*/);
			if(datalen > max_decoder_size) {
				rtsperror("audio decoder: FATAL - conversion input too lengthy (%d > %d)\n",
						datalen, max_decoder_size);
				return -1;
			}
			// srcplanes: assume no-alignment
			srcplanes[0] = aframe->data[0];
			if(av_sample_fmt_is_planar((AVSampleFormat) aframe->format) != 0) {
				// planar
				int i;
#if 0
				// obtain source line size - for calaulating buffer pointers
				av_samples_get_buffer_size(srclines,
						aframe->channels,
						aframe->nb_samples,
						(AVSampleFormat) aframe->format, 1/*no-alignment*/);
				//
#endif
				for(i = 1; i < aframe->channels; i++) {
					//srcplanes[i] = srcplanes[i-1] + srclines[i-1];
					srcplanes[i] = aframe->data[i];
				}
				srcplanes[i] = NULL;
			} else {
				srcplanes[1] = NULL;
			}
			// dstplanes: assume always in packed (interleaved) format
			dstplanes[0] = convbuf;
			dstplanes[1] = NULL;
			//
			swr_convert(swrctx, dstplanes, aframe->nb_samples,
					srcplanes, aframe->nb_samples);
			srcbuf = convbuf;
		}
		if(datalen > dstlen) {
			rtsperror("decoded audio truncated.\n");
			datalen = dstlen;
		}
		//
		bcopy(srcbuf, dstbuf, datalen);
		dstbuf += datalen;
		dstlen -= datalen;
		filled += datalen;
		//
		pkt->size -= len;
		pkt->data += len;
	}
	pkt->data = saveptr;
	if(pkt->data)
		av_free_packet(pkt);
	return filled;
}

int
audio_buffer_fill(void *userdata, unsigned char *stream, int ssize) {
	int filled = 0;
	AVPacket avpkt;
#ifdef ANDROID
	// XXX: use global adecoder
#else
	AVCodecContext *adecoder = (AVCodecContext*) userdata;
#endif
	//
	if(audio_buffer_init() == NULL) {
		rtsperror("audio decoder: cannot allocate audio buffer\n");
#ifdef ANDROID
		rtspParam->quitLive555 = 1;
		return -1;
#else
		exit(-1);
#endif
	}
	while(filled < ssize) {
		int dsize = 0, delta = 0;;
		// buffer has enough data
		if(absize - abpos >= ssize - filled) {
			delta = ssize - filled;
			bcopy(audiobuf+abpos, stream, delta);
			abpos += delta;
			filled += delta;
			return ssize;
		} else if(absize - abpos > 0) {
			delta = absize - abpos;
			bcopy(audiobuf+abpos, stream, delta);
			stream += delta;
			filled += delta;
			abpos = absize = 0;
		}
		// move data to head, leave more ab buffers
		if(abpos != 0) {
			bcopy(audiobuf+abpos, audiobuf, absize-abpos);
			absize -= abpos;
			abpos = 0;
		}
		// decode more packets
		if(packet_queue_get(&audioq, &avpkt, 0) <= 0)
			break;
		if((dsize = audio_buffer_decode(&avpkt, audiobuf+absize, abmaxsize-absize)) < 0)
			break;
		absize += dsize;
	}
	//
	return filled;
}

void
audio_buffer_fill_sdl(void *userdata, unsigned char *stream, int ssize) {
	int filled;
	if((filled = audio_buffer_fill(userdata, stream, ssize)) < 0) {
		rtsperror("audio buffer fill failed.\n");
		exit(-1);
	}
	if(image_rendered == 0) {
		bzero(stream, ssize);
		return;
	}
	bzero(stream+filled, ssize-filled);
	return;
}

static void
play_audio(unsigned char *buffer, int bufsize, struct timeval pts) {
#ifdef ANDROID
	if(rtspconf->builtin_audio_decoder != 0) {
		android_decode_audio(rtspParam, buffer, bufsize, pts);
	} else {
	////////////////////////////////////////
#endif
	AVPacket avpkt;
	//
	av_init_packet(&avpkt);
	avpkt.data = buffer;
	avpkt.size = bufsize;
	if(avpkt.size > 0) {
#if 0
		fprintf(stderr, "DEBUG: audio pts=%08ld.%06ld queue-count=%u queue-size=%u\n",
			pts.tv_sec, pts.tv_usec,
			audioq.queue.size(), audioq.size);
#endif
		packet_queue_put(&audioq, &avpkt);
		packet_queue_drop(&audioq);
	}
#ifndef ANDROID
	if(rtspParam->audioOpened == false) {
		//open_audio();
		union SDL_Event evt;
		bzero(&evt, sizeof(evt));
		evt.user.type = SDL_USEREVENT;
		evt.user.timestamp = time(0);
		evt.user.code = SDL_USEREVENT_OPEN_AUDIO;
		evt.user.data1 = rtspParam;
		evt.user.data2 = adecoder;
		SDL_PushEvent(&evt);
	}
#endif
#ifdef ANDROID
	////////////////////////////////////////
	}
#endif
	return;
}

void *
rtsp_thread(void *param) {
	RTSPClient *client = NULL;
	BasicTaskScheduler0 *bs = BasicTaskScheduler::createNew();
	TaskScheduler* scheduler = bs;
	UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);
	char savefile_yuv[128];
	char savefile_yuvts[128];
	// XXX: reset everything
	ga_aggregated_reset();
	drop_video_frame_init(ga_conf_readint("max-tolerable-video-delay"));
	// save-file features
	if(savefp_yuv != NULL)
		ga_save_close(savefp_yuv);
	if(savefp_yuvts != NULL)
		ga_save_close(savefp_yuvts);
	savefp_yuv = savefp_yuvts = NULL;
	//
	if(ga_conf_readbool("log-rtp-packet", 0) != 0)
		log_rtp = 1;
	if(ga_conf_readv("save-yuv-image", savefile_yuv, sizeof(savefile_yuv)) != NULL)
		savefp_yuv = ga_save_init(savefile_yuv);
	if(savefp_yuv != NULL
	&& ga_conf_readv("save-yuv-image-timestamp", savefile_yuvts, sizeof(savefile_yuvts)) != NULL)
		savefp_yuvts = ga_save_init_txt(savefile_yuvts);
	rtsperror("*** SAVEFILE: YUV image saved to '%s'; timestamp saved to '%s'.\n",
		savefp_yuv   ? savefile_yuv   : "NULL",
		savefp_yuvts ? savefile_yuvts : "NULL");
	//
	if(ga_conf_readint("rtp-reordering-threshold") > 0) {
		rtp_packet_reordering_threshold = ga_conf_readint("rtp-reordering-threshold");
	}
	rtsperror("RTP reordering threshold = %d\n", rtp_packet_reordering_threshold);
	//
	pktloss_monitor_init();
	port2channel.clear();
	video_sess_fmt = -1;
	audio_sess_fmt = -1;
	if(video_codec_name != NULL)	free((void*) video_codec_name);
	if(audio_codec_name != NULL)	free((void*) audio_codec_name);
	video_codec_name = NULL;
	audio_codec_name = NULL;
	video_framing = 0;
	audio_framing = 0;
	rtspClientCount = 0;
	//
	rtspconf = rtspconf_global();
	rtspParam = (RTSPThreadParam*) param;
	rtspParam->videostate = RTSP_VIDEOSTATE_NULL;
	//
	if(init_decoder_buffer() < 0) {
		rtsperror("init decode buffer failed.\n");
		return NULL;
	}
	//
	if(qos_init(env) < 0) {
		deinit_decoder_buffer();
		rtsperror("qos-measurement: init failed.\n");
		return NULL;
	}
	//
	if((client = openURL(*env, rtspParam->url)) == NULL) {
		deinit_decoder_buffer();
		rtsperror("connect to %s failed.\n", rtspParam->url);
		return NULL;
	}
	while(rtspParam->quitLive555 == 0) {
		bs->SingleStep(1000000);
	}
	//
	qos_deinit();
	if(savefp_yuv != NULL) {
		ga_save_close(savefp_yuv);
		savefp_yuv = NULL;
	}
	if(savefp_yuvts != NULL) {
		ga_save_close(savefp_yuvts);
		savefp_yuvts = NULL;
	}
	//
	shutdownStream(client);
	deinit_decoder_buffer();
	// release resources in rtspThreadParam
	for(int i = 0; i < VIDEO_SOURCE_CHANNEL_MAX; i++) {
		if(rtspParam->pipe[i] != NULL) {
			dpipe_destroy(rtspParam->pipe[i]);
			rtspParam->pipe[i] = NULL;
		}
	}
	//
	rtsperror("rtsp thread: terminated.\n");
#ifndef ANDROID
	exit(0);
#endif
	return NULL;
}

//////////////////////////////////////////////////////////////////////////////

class StreamClientState {
public:
	StreamClientState();
	virtual ~StreamClientState();

public:
	MediaSubsessionIterator* iter;
	MediaSession* session;
	MediaSubsession* subsession;
	TaskToken streamTimerTask;
	double duration;
};

class ourRTSPClient: public RTSPClient {
public:
	static ourRTSPClient* createNew(UsageEnvironment& env, char const* rtspURL,
			int verbosityLevel = 0,
			char const* applicationName = NULL,
			portNumBits tunnelOverHTTPPortNum = 0);

protected:
	ourRTSPClient(UsageEnvironment& env, char const* rtspURL,
			int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum);
	// called only by createNew();
	virtual ~ourRTSPClient();

public:
	StreamClientState scs;
};

class DummySink: public MediaSink {
public:
	static DummySink* createNew(UsageEnvironment& env,
			MediaSubsession& subsession, // identifies the kind of data that's being received
			char const* streamId = NULL); // identifies the stream itself (optional)

private:
	DummySink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId);
	// called only by "createNew()"
	virtual ~DummySink();

	static void afterGettingFrame(void* clientData, unsigned frameSize,
			unsigned numTruncatedBytes,
			struct timeval presentationTime,
			unsigned durationInMicroseconds);
	void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
			struct timeval presentationTime, unsigned durationInMicroseconds);

private:
	// redefined virtual functions:
	virtual Boolean continuePlaying();

private:
	u_int8_t* fReceiveBuffer;
	MediaSubsession& fSubsession;
	char* fStreamId;
};

#define RTSP_CLIENT_VERBOSITY_LEVEL 1 // by default, print verbose output from each "RTSPClient"


RTSPClient *
openURL(UsageEnvironment& env, char const* rtspURL) {
	RTSPClient* rtspClient =
		ourRTSPClient::createNew(env, rtspURL, RTSP_CLIENT_VERBOSITY_LEVEL, "RTSP Client"/*"rtsp_thread"*/);
	if (rtspClient == NULL) {
		rtsperror("connect failed: %s\n", env.getResultMsg());
		//env << "Failed to create a RTSP client for URL \"" << rtspURL << "\": " << env.getResultMsg() << "\n";
		return NULL;
	}

	++rtspClientCount;

	rtspClient->sendDescribeCommand(continueAfterDESCRIBE); 

	return rtspClient;
}

void
continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString) {
	do {
		UsageEnvironment& env = rtspClient->envir(); // alias
		StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

		if (resultCode != 0) {
			env << *rtspClient << "Failed to get a SDP description: " << resultString << "\n";
			break;
		}

		char* const sdpDescription = resultString;
		env << *rtspClient << "Got a SDP description:\n" << sdpDescription << "\n";

		// Create a media session object from this SDP description:
		scs.session = MediaSession::createNew(env, sdpDescription);
		delete[] sdpDescription; // because we don't need it anymore
		if (scs.session == NULL) {
			env << *rtspClient << "Failed to create a MediaSession object from the SDP description: " << env.getResultMsg() << "\n";
			break;
		} else if (!scs.session->hasSubsessions()) {
			env << *rtspClient << "This session has no media subsessions (i.e., no \"m=\" lines)\n";
			break;
		}

		scs.iter = new MediaSubsessionIterator(*scs.session);
		setupNextSubsession(rtspClient);
		return;
	} while (0);

	// An unrecoverable error occurred with this stream.
	//shutdownStream(rtspClient);
	rtsperror("Connect to %s failed.\n", rtspClient->url());
#ifdef ANDROID
	goBack(rtspParam->jnienv, -1);
#else
	rtspParam->quitLive555 = 1;
#endif
}

void
setupNextSubsession(RTSPClient* rtspClient) {
	UsageEnvironment& env = rtspClient->envir(); // alias
	StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias
	bool rtpOverTCP = false;
#ifdef SAVE_ENC
	if(fout == NULL) {
		fout = fopen(SAVE_ENC, "wb");
	}
#endif
	if(rtspconf->proto == IPPROTO_TCP) {
		rtpOverTCP = true;
	}

	scs.subsession = scs.iter->next();
	do if (scs.subsession != NULL) {
		if (!scs.subsession->initiate()) {
			env << *rtspClient << "Failed to initiate the \"" << *scs.subsession << "\" subsession: " << env.getResultMsg() << "\n";
			setupNextSubsession(rtspClient); // give up on this subsession; go to the next one
		} else {
			if(strcmp("video", scs.subsession->mediumName()) == 0) {
				char vparam[1024];
				const char *pvparam = NULL;
				video_sess_fmt = scs.subsession->rtpPayloadFormat();
				video_codec_name = strdup(scs.subsession->codecName());
				qos_add_source(video_codec_name, scs.subsession->rtpSource());
				scs.subsession->rtpSource()->setAuxilliaryReadHandler(rtp_packet_handler, NULL);
				if(rtp_packet_reordering_threshold > 0)
					scs.subsession->rtpSource()->setPacketReorderingThresholdTime(rtp_packet_reordering_threshold);
				if(port2channel.find(scs.subsession->clientPortNum()) == port2channel.end()) {
					int cid = port2channel.size();
					port2channel[scs.subsession->clientPortNum()] = cid;
#ifdef ANDROID
					if(rtspconf->builtin_video_decoder != 0) {
						video_codec_id = ga_lookup_codec_id(video_codec_name);
						// TODO: retrieve SPS/PPS from sprop-parameter-sets
						if(video_codec_id == AV_CODEC_ID_H264) {
							android_config_h264_sprop(rtspParam, scs.subsession->fmtp_spropparametersets());
						}
					} else {
					////// Work with ffmpeg
#endif
					video_codec_id = ga_lookup_codec_id(video_codec_name);
					if(video_codec_id == AV_CODEC_ID_H264) {
						pvparam = scs.subsession->fmtp_spropparametersets();
					} else if(video_codec_id == AV_CODEC_ID_H265) {
						snprintf(vparam, sizeof(vparam), "%s,%s,%s",
							scs.subsession->fmtp_spropvps()==NULL ? "" : scs.subsession->fmtp_spropvps(),
							scs.subsession->fmtp_spropsps()==NULL ? "" : scs.subsession->fmtp_spropsps(),
							scs.subsession->fmtp_sproppps()==NULL ? "" : scs.subsession->fmtp_sproppps());
						pvparam = vparam;
					} else {
						pvparam = NULL;
					}
					if(init_vdecoder(cid, pvparam/*scs.subsession->fmtp_spropparametersets()*/) < 0) {
						rtsperror("cannot initialize video decoder(%d)\n", cid);
						rtspParam->quitLive555 = 1;
						return;
					}
					rtsperror("video decoder(%d) initialized (client port %d)\n",
						cid, scs.subsession->clientPortNum());
#ifdef ANDROID
					////////////////////////
					}
#endif
				}
			} else if(strcmp("audio", scs.subsession->mediumName()) == 0) {
				const char *mime = NULL;
				audio_sess_fmt = scs.subsession->rtpPayloadFormat();
				audio_codec_name = strdup(scs.subsession->codecName());
				qos_add_source(audio_codec_name, scs.subsession->rtpSource());
				if(rtp_packet_reordering_threshold > 0)
					scs.subsession->rtpSource()->setPacketReorderingThresholdTime(rtp_packet_reordering_threshold);
#ifdef ANDROID
				if((mime = ga_lookup_mime(audio_codec_name)) == NULL) {
					showToast(rtspParam->jnienv, "codec %s not supported", audio_codec_name);
					rtsperror("rtspclient: unsupported audio codec: %s\n", audio_codec_name);
					usleep(300000);
					rtspParam->quitLive555 = 1;
					return;
				}
				audio_codec_id = ga_lookup_codec_id(audio_codec_name);
				if(android_prepare_audio(rtspParam, mime, rtspconf->builtin_audio_decoder != 0) < 0)
					return;
				if(rtspconf->builtin_audio_decoder == 0) {
				//////////////////////////////////////
				rtsperror("init software audio decoder.\n");
#endif
				if(adecoder == NULL) {
					if(init_adecoder() < 0) {
						rtsperror("cannot initialize audio decoder.\n");
						rtspParam->quitLive555 = 1;
						return;
					}
				}
#ifdef ANDROID
				//////////////////////////////////////
				}
#endif
				rtsperror("audio decoder initialized.\n");
			}
			env << *rtspClient << "Initiated the \"" << *scs.subsession
				<< "\" subsession (client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum()+1 << ")\n";

			// Continue setting up this subsession, by sending a RTSP "SETUP" command:
			rtspClient->sendSetupCommand(*scs.subsession, continueAfterSETUP, False, rtpOverTCP ? True : False/*TCP?*/, False, NULL);
		}
		return;
	} while(0);
	//

	// We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
	scs.duration = scs.session->playEndTime() - scs.session->playStartTime();
	rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY);
}

static void
NATHolePunch(RTPSource *rtpsrc, MediaSubsession *subsession) {
	Groupsock *gs = NULL;
	int s;
	struct sockaddr_in sin;
	unsigned char buf[1] = { 0x00 };
#ifdef WIN32
	int sinlen = sizeof(sin);
#else
	socklen_t sinlen = sizeof(sin);
#endif
	if(rtspconf->sin.sin_addr.s_addr == 0
	|| rtspconf->sin.sin_addr.s_addr == INADDR_NONE) {
		rtsperror("NAT hole punching: no server address available.\n");
		return;
	}
	if(rtpsrc == NULL) {
		rtsperror("NAT hole punching: no RTPSource available.\n");
		return;
	}
	if(subsession == NULL) {
		rtsperror("NAT hole punching: no subsession available.\n");
		return;
	}
	gs = rtpsrc->RTPgs();
	if(gs == NULL) {
		rtsperror("NAT hole punching: no Groupsock available.\n");
		return;
	}
	//
	s = gs->socketNum();
	if(getsockname(s, (struct sockaddr*) &sin, &sinlen) < 0) {
		rtsperror("NAT hole punching: getsockname - %s.\n", strerror(errno));
		return;
	}
	rtsperror("NAT hole punching: fd=%d, local-port=%d/%d server-port=%d\n",
		s, ntohs(sin.sin_port), subsession->clientPortNum(), subsession->serverPortNum);
	//
	bzero(&sin, sizeof(sin));
	sin.sin_addr = rtspconf->sin.sin_addr;
	sin.sin_port = htons(subsession->serverPortNum);
	// send 5 packets
	// XXX: use const char * for buf pointer to work with Windows
	sendto(s, (const char *) buf, 1, 0, (struct sockaddr*) &sin, sizeof(sin)); usleep(5000);
	sendto(s, (const char *) buf, 1, 0, (struct sockaddr*) &sin, sizeof(sin)); usleep(5000);
	sendto(s, (const char *) buf, 1, 0, (struct sockaddr*) &sin, sizeof(sin)); usleep(5000);
	sendto(s, (const char *) buf, 1, 0, (struct sockaddr*) &sin, sizeof(sin)); usleep(5000);
	sendto(s, (const char *) buf, 1, 0, (struct sockaddr*) &sin, sizeof(sin)); usleep(5000);
	//
	return;
}

void
continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString) {
	do {
		UsageEnvironment& env = rtspClient->envir(); // alias
		StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

		if (resultCode != 0) {
			env << *rtspClient << "Failed to set up the \"" << *scs.subsession << "\" subsession: " << env.getResultMsg() << "\n";
			break;
		}

		env << *rtspClient << "Set up the \"" << *scs.subsession
			<< "\" subsession (client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum()+1 << ")\n";

		scs.subsession->sink = DummySink::createNew(env, *scs.subsession, rtspClient->url());
		// perhaps use your own custom "MediaSink" subclass instead
		if (scs.subsession->sink == NULL) {
			env << *rtspClient << "Failed to create a data sink for the \"" << *scs.subsession
				<< "\" subsession: " << env.getResultMsg() << "\n";
			break;
		}

		env << *rtspClient << "Created a data sink for the \"" << *scs.subsession << "\" subsession\n";
		scs.subsession->miscPtr = rtspClient; // a hack to let subsession handle functions get the "RTSPClient" from the subsession 
		scs.subsession->sink->startPlaying(*(scs.subsession->readSource()),
				subsessionAfterPlaying, scs.subsession);
		// Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
		if (scs.subsession->rtcpInstance() != NULL) {
			scs.subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, scs.subsession);
		}
		// Set receiver buffer size
		if(scs.subsession->rtpSource()) {
			int newsz;
			newsz = increaseReceiveBufferTo(env,
				scs.subsession->rtpSource()->RTPgs()->socketNum(), RCVBUF_SIZE);
			rtsperror("Receiver buffer increased to %d\n", newsz);
		}
		// NAT hole-punching?
		NATHolePunch(scs.subsession->rtpSource(), scs.subsession);
	} while (0);

	// Set up the next subsession, if any:
	setupNextSubsession(rtspClient);
}

void
continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString) {
	do {
		UsageEnvironment& env = rtspClient->envir(); // alias
		StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

		if (resultCode != 0) {
			env << *rtspClient << "Failed to start playing session: " << resultString << "\n";
			break;
		}

		if (scs.duration > 0) {
			unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
			scs.duration += delaySlop;
			unsigned uSecsToDelay = (unsigned)(scs.duration*1000000);
			scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, rtspClient);
		}

		env << *rtspClient << "Started playing session";
		if (scs.duration > 0) {
			env << " (for up to " << scs.duration << " seconds)";
		}
		env << "...\n";

		qos_start();

		return;
	} while (0);

	// An unrecoverable error occurred with this stream.
	shutdownStream(rtspClient);
}

void
subsessionAfterPlaying(void* clientData) {
	MediaSubsession* subsession = (MediaSubsession*)clientData;
	RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);

	// Begin by closing this subsession's stream:
	Medium::close(subsession->sink);
	subsession->sink = NULL;

	// Next, check whether *all* subsessions' streams have now been closed:
	MediaSession& session = subsession->parentSession();
	MediaSubsessionIterator iter(session);
	while ((subsession = iter.next()) != NULL) {
		if (subsession->sink != NULL) return; // this subsession is still active
	}

	// All subsessions' streams have now been closed, so shutdown the client:
	shutdownStream(rtspClient);
}

void
subsessionByeHandler(void* clientData) {
	MediaSubsession* subsession = (MediaSubsession*)clientData;
	RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;
	UsageEnvironment& env = rtspClient->envir(); // alias

	env << *rtspClient << "Received RTCP \"BYE\" on \"" << *subsession << "\" subsession\n";

	// Now act as if the subsession had closed:
	subsessionAfterPlaying(subsession);
}

void
streamTimerHandler(void* clientData) {
	ourRTSPClient* rtspClient = (ourRTSPClient*)clientData;
	StreamClientState& scs = rtspClient->scs; // alias

	scs.streamTimerTask = NULL;

	// Shut down the stream:
	shutdownStream(rtspClient);
}

void
shutdownStream(RTSPClient* rtspClient, int exitCode) {
	UsageEnvironment& env = rtspClient->envir(); // alias
	StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

	if(rtspClientCount <= 0)
		return;

	// First, check whether any subsessions have still to be closed:
	if (scs.session != NULL) { 
		Boolean someSubsessionsWereActive = False;
		MediaSubsessionIterator iter(*scs.session);
		MediaSubsession* subsession;

		while ((subsession = iter.next()) != NULL) {
			if (subsession->sink != NULL) {
				Medium::close(subsession->sink);
				subsession->sink = NULL;

				if (subsession->rtcpInstance() != NULL) {
					subsession->rtcpInstance()->setByeHandler(NULL, NULL); // in case the server sends a RTCP "BYE" while handling "TEARDOWN"
				}

				someSubsessionsWereActive = True;
			}
		}

		if (someSubsessionsWereActive) {
			// Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
			// Don't bother handling the response to the "TEARDOWN".
			rtspClient->sendTeardownCommand(*scs.session, NULL);
		}
	}

	env << *rtspClient << "Closing the stream.\n";
	Medium::close(rtspClient);
	// Note that this will also cause this stream's "StreamClientState" structure to get reclaimed.

	if (--rtspClientCount == 0) {
		// The final stream has ended, so exit the application now.
		// (Of course, if you're embedding this code into your own application, you might want to comment this out,
		// and replace it with "eventLoopWatchVariable = 1;", so that we leave the LIVE555 event loop, and continue running "main()".)
		//exit(exitCode);
		rtsperror("rtsp thread: no more rtsp clients\n");
		rtspParam->quitLive555 = 1;
	}
}


// Implementation of "ourRTSPClient":

ourRTSPClient*
ourRTSPClient::createNew(UsageEnvironment& env, char const* rtspURL,
	int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum) {
	return new ourRTSPClient(env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
}

ourRTSPClient::ourRTSPClient(UsageEnvironment& env, char const* rtspURL,
	int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum)
	: RTSPClient(env,rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1) {
}

ourRTSPClient::~ourRTSPClient() {
}

// Implementation of "StreamClientState":

StreamClientState::StreamClientState()
	: iter(NULL), session(NULL), subsession(NULL), streamTimerTask(NULL), duration(0.0) {
}

StreamClientState::~StreamClientState() {
	delete iter;
	if (session != NULL) {
		// We also need to delete "session", and unschedule "streamTimerTask" (if set)
		UsageEnvironment& env = session->envir(); // alias

		env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
		Medium::close(session);
	}
}

// Implementation of "DummySink":

// Even though we're not going to be doing anything with the incoming data, we still need to receive it.
// Define the size of the buffer that we'll use:
#define DUMMY_SINK_RECEIVE_BUFFER_SIZE 1048576	//100000

DummySink*
DummySink::createNew(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId) {
	return new DummySink(env, subsession, streamId);
}

DummySink::DummySink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId)
		: MediaSink(env), fSubsession(subsession) {
	fStreamId = strDup(streamId);
	fReceiveBuffer = new u_int8_t[MAX_FRAMING_SIZE+DUMMY_SINK_RECEIVE_BUFFER_SIZE];
	// setup framing if necessary
	// H264 framing code
	if(strcmp("H264", fSubsession.codecName()) == 0
	|| strcmp("H265", fSubsession.codecName()) == 0) {
		video_framing = 4;
		fReceiveBuffer[MAX_FRAMING_SIZE-video_framing+0]
		= fReceiveBuffer[MAX_FRAMING_SIZE-video_framing+1]
		= fReceiveBuffer[MAX_FRAMING_SIZE-video_framing+2] = 0;
		fReceiveBuffer[MAX_FRAMING_SIZE-video_framing+3] = 1;
	}
	return;
}

DummySink::~DummySink() {
	delete[] fReceiveBuffer;
	delete[] fStreamId;
}

void
DummySink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
		struct timeval presentationTime, unsigned durationInMicroseconds) {
	DummySink* sink = (DummySink*)clientData;
	sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

// If you don't want to see debugging output for each received frame, then comment out the following line:
#define DEBUG_PRINT_EACH_RECEIVED_FRAME 1

void
DummySink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
		struct timeval presentationTime, unsigned /*durationInMicroseconds*/) {
#ifndef ANDROID
	extern pthread_mutex_t watchdogMutex;
	extern struct timeval watchdogTimer;
#endif
	if(fSubsession.rtpPayloadFormat() == video_sess_fmt) {
		bool marker = false;
		int channel = port2channel[fSubsession.clientPortNum()];
		int lost = 0, count = 1;
		RTPSource *rtpsrc = fSubsession.rtpSource();
		RTPReceptionStatsDB::Iterator iter(rtpsrc->receptionStatsDB());
		RTPReceptionStats* stats = iter.next(True);
#ifdef ANDROID  // support only single channel
		if(channel > 0) 
			goto dropped;
#endif
		//if(drop_video_frame(channel, presentationTime) > 0) {
		//	if(stats != NULL)
		//		pktloss_monitor_reset(rtpsrc->SSRC());
		//	goto dropped;
		//}
		if(rtpsrc != NULL) {
			marker = rtpsrc->curPacketMarkerBit();
		}
		//
		if(stats != NULL) {
			lost = pktloss_monitor_get(stats->SSRC(), &count, 1/*reset*/);
#if 0
			if(lost > 0) {
				ga_error("rtspclient: frame corrupted? lost=%d; count=%d (packets)\n", lost, count);
			}
#endif
		}
		//
		play_video(channel,
			fReceiveBuffer+MAX_FRAMING_SIZE-video_framing,
			frameSize+video_framing, presentationTime,
			marker);
#ifdef ANDROID
		if(rtspconf->builtin_video_decoder==0
		&& rtspconf->builtin_audio_decoder==0)
			kickWatchdog(rtspParam->jnienv);
#endif
	} else if(fSubsession.rtpPayloadFormat() == audio_sess_fmt) {
		play_audio(fReceiveBuffer+MAX_FRAMING_SIZE-audio_framing,
			frameSize+audio_framing, presentationTime);
	}
#ifndef ANDROID // watchdog is implemented at the Java side
	pthread_mutex_lock(&watchdogMutex);
	gettimeofday(&watchdogTimer, NULL);
	pthread_mutex_unlock(&watchdogMutex);
#endif
dropped:
	// Then continue, to request the next frame of data:
	continuePlaying();
}

Boolean
DummySink::continuePlaying() {
	if (fSource == NULL) return False; // sanity check (should not happen)

	// Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
	fSource->getNextFrame(fReceiveBuffer+MAX_FRAMING_SIZE,
			DUMMY_SINK_RECEIVE_BUFFER_SIZE,
			afterGettingFrame, this,
			onSourceClosure, this);
	return True;
}

