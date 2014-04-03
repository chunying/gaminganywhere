/*
 * Copyright (c) 2013 Chun-Ying Huang
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

#ifndef ANDROID
#include "vsource.h"
#endif
#include "rtspclient.h"

#include "ga-common.h"
#include "ga-avcodec.h"
#include "controller.h"
#include "minih264.h"
#ifdef ANDROID
#include "android-decoders.h"
#endif

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

#define RCVBUF_SIZE		262144

#define	COUNT_FRAME_RATE	600	// every N frames

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

#ifdef COUNT_FRAME_RATE
static int cf_frame[IMAGE_SOURCE_CHANNEL_MAX];
static struct timeval cf_tv0[IMAGE_SOURCE_CHANNEL_MAX];
static struct timeval cf_tv1[IMAGE_SOURCE_CHANNEL_MAX];
static long long cf_interval[IMAGE_SOURCE_CHANNEL_MAX];
#endif

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
static AVCodecContext *vdecoder[IMAGE_SOURCE_CHANNEL_MAX];
static map<unsigned short,int> port2channel;
static AVFrame *vframe[IMAGE_SOURCE_CHANNEL_MAX];
static AVCodecContext *adecoder = NULL;
static AVFrame *aframe = NULL;

static int packet_queue_initialized = 0;
static PacketQueue audioq;

void
packet_queue_init(PacketQueue *q) {
	packet_queue_initialized = 1;
	q->queue.clear();
	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->cond, NULL);
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

int
init_vdecoder(int channel, const char *sprop) {
	AVCodec *codec = NULL; //rtspconf->video_decoder_codec;
	AVCodecContext *ctx;
	AVFrame *frame;
	const char **names = NULL;
	//
	if(channel > IMAGE_SOURCE_CHANNEL_MAX) {
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
	if((frame = avcodec_alloc_frame()) == NULL) {
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
		unsigned char *extra = (unsigned char*) strdup(sprop);
		int extrasize = strlen(sprop);
		extrasize = av_base64_decode(extra, sprop, extrasize);
		if(extrasize > 0) {
			ctx->extradata = extra;
			ctx->extradata_size = extrasize;
			rtsperror("video decoder(%d): sprop configured with '%s', decoded-size=%d\n", channel, sprop, extrasize);
			fprintf(stderr, "SPROP = [");
			for(unsigned char *ptr = extra; extrasize > 0; extrasize--) {
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
	if((aframe = avcodec_alloc_frame()) == NULL) {
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

static void
play_video_priv(int ch/*channel*/, unsigned char *buffer, int bufsize, struct timeval pts) {
	AVPacket avpkt;
	int got_picture, len;
#ifndef ANDROID
	union SDL_Event evt;
#endif
	struct pooldata *data = NULL;
	AVPicture *dstframe = NULL;
	//
	av_init_packet(&avpkt);
	avpkt.size = bufsize;
	avpkt.data = buffer;
	while(avpkt.size > 0) {
		//
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
				rtspParam->format[ch] = (PixelFormat) vframe[ch]->format;
#ifdef ANDROID
				create_overlay(ch, vframe[0]->width, vframe[0]->height, (PixelFormat) vframe[0]->format);
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
			data = rtspParam->pipe[ch]->allocate_data();
			dstframe = (AVPicture*) data->ptr;
			sws_scale(rtspParam->swsctx[ch],
				// source: decoded frame
				vframe[ch]->data, vframe[ch]->linesize,
				0, vframe[ch]->height,
				// destination: texture
				dstframe->data, dstframe->linesize);
			rtspParam->pipe[ch]->store_data(data);
			// request to render it
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
	return;
}

#define	PRIVATE_BUFFER_SIZE	1048576

struct decoder_buffer {
	unsigned int privbuflen;
	unsigned char *privbuf;
	struct timeval lastpts;
};

static void
play_video(int channel, unsigned char *buffer, int bufsize, struct timeval pts, bool marker) {
	static struct decoder_buffer db[IMAGE_SOURCE_CHANNEL_MAX];
	struct decoder_buffer *pdb = &db[channel];
	// buffer initialization
	if(pdb->privbuf == NULL) {
		pdb->privbuf = (unsigned char*) malloc(PRIVATE_BUFFER_SIZE);
		if(pdb->privbuf == NULL) {
			rtsperror("FATAL: cannot allocate private buffer (%d bytes): %s\n", PRIVATE_BUFFER_SIZE, strerror(errno));
			//exit(-1);
			rtspParam->quitLive555 = 1;
			return;
		}
	}
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
			play_video_priv(channel, pdb->privbuf,
				pdb->privbuflen, pdb->lastpts);
		}
		pdb->privbuflen = 0;
	}
	if(pdb->privbuflen + bufsize <= PRIVATE_BUFFER_SIZE) {
		bcopy(buffer, &pdb->privbuf[pdb->privbuflen], bufsize);
		pdb->privbuflen += bufsize;
		pdb->lastpts = pts;
		if(marker && pdb->privbuflen > 0) {
			play_video_priv(channel, pdb->privbuf,
				pdb->privbuflen, pdb->lastpts);
			pdb->privbuflen = 0;
		}
	} else {
		rtsperror("WARNING: video private buffer overflow.\n");
		play_video_priv(channel, pdb->privbuf,
				pdb->privbuflen, pdb->lastpts);
		pdb->privbuflen = 0;
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
		avcodec_get_frame_defaults(aframe);
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
		//fprintf(stderr, "DEBUG: audio pts=%08ld.%06ld\n",
		//	pts.tv_sec, pts.tv_usec);
		packet_queue_put(&audioq, &avpkt);
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
	// XXX: reset everything
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
	if((client = openURL(*env, rtspParam->url)) == NULL) {
		rtsperror("connect to %s failed.\n", rtspParam->url);
		return NULL;
	}
	while(rtspParam->quitLive555 == 0) {
		bs->SingleStep(1000000);
	}
	//
	shutdownStream(client);
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
				video_sess_fmt = scs.subsession->rtpPayloadFormat();
				video_codec_name = strdup(scs.subsession->codecName());
				if(port2channel.find(scs.subsession->clientPortNum()) == port2channel.end()) {
					int cid = port2channel.size();
					port2channel[scs.subsession->clientPortNum()] = cid;
#ifdef ANDROID
					if(rtspconf->builtin_video_decoder != 0) {
						video_codec_id = ga_lookup_codec_id(video_codec_name);
					} else {
					////// Work with ffmpeg
#endif
					if(init_vdecoder(cid, scs.subsession->fmtp_spropparametersets()) < 0) {
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
SetRecvBufSize(RTPSource *rtpsrc) {
	Groupsock *gs = NULL;
	int s;
#ifdef WIN32
	DWORD bufsz;
	int buflen;
#else
	int bufsz;
	socklen_t buflen;
#endif
	//
	if(rtpsrc == NULL) {
		rtsperror("Receiver buffer size: no RTPSource available.\n");
		return;
	}
	//
	gs = rtpsrc->RTPgs();
	if(gs == NULL) {
		rtsperror("Receiver buffer size: no Groupsock available.\n");
		return;
	}
	//
	s = gs->socketNum();
	//
	bufsz = RCVBUF_SIZE;
	if(setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char*) &bufsz, sizeof(bufsz)) != 0) {
		rtsperror("Receiver buffer size: set failed (%d).\n", RCVBUF_SIZE);
		return;
	}
	//
	buflen = sizeof(bufsz);
	if(getsockopt(s, SOL_SOCKET, SO_RCVBUF, (char*) &bufsz, &buflen) != 0) {
		rtsperror("Receiver buffer size: get failed.\n");
		return;
	}
	//
	rtsperror("Receiver buffer size: %d bytes.\n", bufsz);
	return;
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
		SetRecvBufSize(scs.subsession->rtpSource());
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
#define DUMMY_SINK_RECEIVE_BUFFER_SIZE 262144	//100000

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
	if(strcmp("H264", fSubsession.codecName()) == 0) {
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
		RTPSource *rtpsrc = fSubsession.rtpSource();
#ifdef ANDROID  // support only single channel
		if(channel > 0) 
			goto dropped;
#endif
		if(rtpsrc != NULL) {
			marker = rtpsrc->curPacketMarkerBit();
		}
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

