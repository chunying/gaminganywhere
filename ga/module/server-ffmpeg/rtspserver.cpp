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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#ifndef WIN32
#include <unistd.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <arpa/inet.h>
#endif	/* ifndef WIN32 */

#include "ga-common.h"
#include "ga-avcodec.h"
#include "ga-conf.h"

#include "vsource.h"
#include "asource.h"
#include "encoder-common.h"
#include "rtspconf.h"

#include "rtspserver.h"

#define	RTSP_STREAM_FORMAT	"streamid=%d"
#define	RTSP_STREAM_FORMAT_MAXLEN	64

static struct RTSPConf *rtspconf = NULL;

#ifndef NIPQUAD
#define NIPQUAD(x)	((unsigned char*)&(x))[0],	\
			((unsigned char*)&(x))[1],	\
			((unsigned char*)&(x))[2],	\
			((unsigned char*)&(x))[3]
#endif

void
rtsp_cleanup(RTSPContext *rtsp, int retcode) {
	rtsp->state = SERVER_STATE_TEARDOWN;
#ifdef WIN32
	Sleep(1000);
#else
	sleep(1);
#endif
	return;
}

static int
rtsp_write(RTSPContext *ctx, const void *buf, size_t count) {
	return write(ctx->fd, buf, count);
}

static int
rtsp_printf(RTSPContext *ctx, const char *fmt, ...) {
	va_list ap;
	char buf[8192];
	int buflen;
	//
	va_start(ap, fmt);
	buflen = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	return rtsp_write(ctx, buf, buflen);
}

int
rtsp_write_bindata(RTSPContext *ctx, int streamid, uint8_t *buf, int buflen) {
	int i, pktlen;
	char header[4];
	//
	if(buflen < 4) {
		return buflen;
	}
	// XXX: buffer is the reuslt from avio_open_dyn_buf.
	// Multiple RTP packets can be placed in a single buffer.
	// Format == 4-bytes (big-endian) packet size + packet-data
	i = 0;
	while(i < buflen) {
		pktlen  = (buf[i+0] << 24);
		pktlen += (buf[i+1] << 16);
		pktlen += (buf[i+2] << 8);
		pktlen += (buf[i+3]);
		if(pktlen == 0) {
			i += 4;
			continue;
		}
		//
		header[0] = '$';
		header[1] = (streamid<<1) & 0x0ff;
		header[2] = pktlen>>8;
		header[3] = pktlen & 0x0ff;
		pthread_mutex_lock(&ctx->rtsp_writer_mutex);
		if(rtsp_write(ctx, header, 4) != 4) {
			pthread_mutex_unlock(&ctx->rtsp_writer_mutex);
			return i;
		}
		if(rtsp_write(ctx, &buf[i+4], pktlen) != pktlen) {
			return i;
		}
		pthread_mutex_unlock(&ctx->rtsp_writer_mutex);
		//
		i += (4+pktlen);
	}
	return i;
}

#ifdef HOLE_PUNCHING
static int
rtp_open_internal(unsigned short *port) {
#ifdef WIN32
	SOCKET s;
#else
	int s;
#endif
	struct sockaddr_in sin;
#ifdef WIN32
	int sinlen;
#else
	socklen_t sinlen;
#endif
	bzero(&sin, sizeof(sin));
	if((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		return -1;
	sin.sin_family = AF_INET;
	if(bind(s, (struct sockaddr*) &sin, sizeof(sin)) < 0) {
		close(s);
		return -1;
	}
	sinlen = sizeof(sin);
	if(getsockname(s, (struct sockaddr*) &sin, &sinlen) < 0) {
		close(s);
		return -1;
	}
	*port = ntohs(sin.sin_port);
	return s;
}

int
rtp_open_ports(RTSPContext *ctx, int streamid) {
	if(streamid < 0)
		return -1;
	if(streamid+1 > ctx->streamCount) {
		ctx->streamCount = streamid+1;
	}
	streamid *= 2;
	// initialized?
	if(ctx->rtpSocket[streamid] != 0)
		return 0;
	//
	if((ctx->rtpSocket[streamid] = rtp_open_internal(&ctx->rtpLocalPort[streamid])) < 0)
		return -1;
	if((ctx->rtpSocket[streamid+1] = rtp_open_internal(&ctx->rtpLocalPort[streamid+1])) < 0) {
		close(ctx->rtpSocket[streamid]);
		return -1;
	}
	ga_error("RTP: port opened for stream %d, min=%d (fd=%d), max=%d (fd=%d)\n",
		streamid/2,
		(unsigned int) ctx->rtpLocalPort[streamid],
		(int) ctx->rtpSocket[streamid],
		(unsigned int) ctx->rtpLocalPort[streamid+1],
		(int) ctx->rtpSocket[streamid+1]);
	return 0;
}

int
rtp_close_ports(RTSPContext *ctx, int streamid) {
	streamid *= 2;
	if(ctx->rtpSocket[streamid] != 0)
		close(ctx->rtpSocket[streamid]);
	if(ctx->rtpSocket[streamid+1] != 0)
		close(ctx->rtpSocket[streamid+1]);
	ctx->rtpSocket[streamid] = 0;
	ctx->rtpSocket[streamid+1] = 0;
	return 0;
}

int
rtp_write_bindata(RTSPContext *ctx, int streamid, uint8_t *buf, int buflen) {
	int i, pktlen;
	struct sockaddr_in sin;
	if(ctx->rtpSocket[streamid*2] == 0)
		return -1;
	if(buf==NULL)
		return 0;
	if(buflen < 4)
		return buflen;
	bcopy(&ctx->client, &sin, sizeof(sin));
	sin.sin_port = ctx->rtpPeerPort[streamid*2];
	// XXX: buffer is the reuslt from avio_open_dyn_buf.
	// Multiple RTP packets can be placed in a single buffer.
	// Format == 4-bytes (big-endian) packet size + packet-data
	i = 0;
	while(i < buflen) {
		pktlen  = (buf[i+0] << 24);
		pktlen += (buf[i+1] << 16);
		pktlen += (buf[i+2] << 8);
		pktlen += (buf[i+3]);
		if(pktlen == 0) {
			i += 4;
			continue;
		}
#if 0
		ga_error("Pkt: send to %u.%u.%u.%u:%u, size=%d\n",
			NIPQUAD(ctx->client.sin_addr.s_addr),
			ntohs(ctx->rtpPeerPort[streamid*2]),
			buflen);
#endif
		sendto(ctx->rtpSocket[streamid*2], (const char*) &buf[i+4], pktlen, 0,
			(struct sockaddr*) &sin, sizeof(struct sockaddr_in));
		i += (4+pktlen);
	}
	return i;
}
#endif

static int
rtsp_read_internal(RTSPContext *ctx) {
	int rlen;
	if((rlen = read(ctx->fd, 
		ctx->rbuffer + ctx->rbuftail,
		ctx->rbufsize - ctx->rbuftail)) <= 0) {
		return -1;
	}
	ctx->rbuftail += rlen;
	return ctx->rbuftail - ctx->rbufhead;
}

static int
rtsp_read_text(RTSPContext *ctx, char *buf, size_t count) {
	int i;
	size_t textlen;
again:
	for(i = ctx->rbufhead; i < ctx->rbuftail; i++) {
		if(ctx->rbuffer[i] == '\n') {
			textlen = i - ctx->rbufhead + 1;
			if(textlen > count-1) {
				ga_error("Insufficient string buffer length.\n");
				return -1;
			}
			bcopy(ctx->rbuffer + ctx->rbufhead, buf, textlen);
			buf[textlen] = '\0';
			ctx->rbufhead += textlen;
			if(ctx->rbufhead == ctx->rbuftail)
				ctx->rbufhead = ctx->rbuftail = 0;
			return textlen;
		}
	}
	// buffer full?
	if(ctx->rbuftail - ctx->rbufhead == ctx->rbufsize) {
		ga_error("Buffer full: Extremely long text data encountered?\n");
		return -1;
	}
	// did not found '\n', read more
	bcopy(ctx->rbuffer + ctx->rbufhead, ctx->rbuffer, ctx->rbuftail - ctx->rbufhead);
	ctx->rbuftail = ctx->rbuftail - ctx->rbufhead;
	ctx->rbufhead = 0;
	//
	if(rtsp_read_internal(ctx) < 0)
		return -1;
	goto again;
	// unreachable, but to meet compiler's requirement
	return -1;
}

static int
rtsp_read_binary(RTSPContext *ctx, char *buf, size_t count) {
	int reqlength;
	if(ctx->rbuftail - ctx->rbufhead < 4)
		goto readmore;
again:
	reqlength = (unsigned char) ctx->rbuffer[ctx->rbufhead+2];
	reqlength <<= 8;
	reqlength += (unsigned char) ctx->rbuffer[ctx->rbufhead+3];
	// data is ready
	if(4+reqlength <= ctx->rbuftail - ctx->rbufhead) {
		bcopy(ctx->rbuffer + ctx->rbufhead, buf, 4+reqlength);
		ctx->rbufhead += (4+reqlength);
		if(ctx->rbufhead == ctx->rbuftail)
			ctx->rbufhead = ctx->rbuftail = 0;
		return 4+reqlength;
	}
	// second trail?
	if(ctx->rbuftail - ctx->rbufhead == ctx->rbufsize) {
		ga_error("Buffer full: Extremely long binary data encountered?\n");
		return -1;
	}
readmore:
	bcopy(ctx->rbuffer + ctx->rbufhead, ctx->rbuffer, ctx->rbuftail - ctx->rbufhead);
	ctx->rbuftail = ctx->rbuftail - ctx->rbufhead;
	ctx->rbufhead = 0;
	//
	if(rtsp_read_internal(ctx) < 0)
		return -1;
	goto again;
	// unreachable, but to meet compiler's requirement
	return -1;
}

static int
rtsp_getnext(RTSPContext *ctx, char *buf, size_t count) {
	// initialize if necessary
	if(ctx->rbuffer == NULL) {
		ctx->rbufsize = 65536;
		if((ctx->rbuffer = (char*) malloc(ctx->rbufsize)) == NULL) {
			ctx->rbufsize = 0;
			return -1;
		}
		ctx->rbufhead = 0;
		ctx->rbuftail = 0;
	}
	// buffer is empty, force read
	if(ctx->rbuftail == ctx->rbufhead) {
		if(rtsp_read_internal(ctx) < 0)
			return -1;
	}
	// buffer is not empty
	if(ctx->rbuffer[ctx->rbufhead] != '$') {
		// text data
		return rtsp_read_text(ctx, buf, count);
	}
	// binary data
	return rtsp_read_binary(ctx, buf, count);
}

static int
per_client_init(RTSPContext *ctx) {
	int i;
	AVOutputFormat *fmt;
	//
	if((fmt = av_guess_format("rtp", NULL, NULL)) == NULL) {
		ga_error("RTP not supported.\n");
		return -1;
	}
	if((ctx->sdp_fmtctx = avformat_alloc_context()) == NULL) {
		ga_error("create avformat context failed.\n");
		return -1;
	}
	ctx->sdp_fmtctx->oformat = fmt;
	// video stream
	for(i = 0; i < video_source_channels(); i++) {
		if((ctx->sdp_vstream[i] = ga_avformat_new_stream(
			ctx->sdp_fmtctx,
			i, rtspconf->video_encoder_codec)) == NULL) {
			//
			ga_error("cannot create new video stream (%d:%d)\n",
				i, rtspconf->video_encoder_codec->id);
			return -1;
		}
		if((ctx->sdp_vencoder[i] = ga_avcodec_vencoder_init(
			ctx->sdp_vstream[i]->codec,
			rtspconf->video_encoder_codec,
			video_source_out_width(i), video_source_out_height(i),
			rtspconf->video_fps,
			rtspconf->vso)) == NULL) {
			//
			ga_error("cannot init video encoder\n");
			return -1;
		}
	}
	// audio stream
#ifdef ENABLE_AUDIO
	if((ctx->sdp_astream = ga_avformat_new_stream(
			ctx->sdp_fmtctx,
			video_source_channels(),
			rtspconf->audio_encoder_codec)) == NULL) {
		ga_error("cannot create new audio stream (%d)\n",
			rtspconf->audio_encoder_codec->id);
		return -1;
	}
	if((ctx->sdp_aencoder = ga_avcodec_aencoder_init(
			ctx->sdp_astream->codec,
			rtspconf->audio_encoder_codec,
			rtspconf->audio_bitrate,
			rtspconf->audio_samplerate,
			rtspconf->audio_channels,
			rtspconf->audio_codec_format,
			rtspconf->audio_codec_channel_layout)) == NULL) {
		ga_error("cannot init audio encoder\n");
		return -1;
	}
#endif
	if((ctx->mtu = ga_conf_readint("packet-size")) <= 0)
		ctx->mtu = RTSP_TCP_MAX_PACKET_SIZE;
	//
	return 0;
}

static void
close_av(AVFormatContext *fctx, AVStream *st, AVCodecContext *cctx, enum RTSPLowerTransport transport) {
	unsigned i;
	//
	if(cctx) {
		ga_avcodec_close(cctx);
	}
	if(st && st->codec != NULL) {
		if(st->codec != cctx) {
			ga_avcodec_close(st->codec);
		}
		st->codec = NULL;
	}
	if(fctx) {
		for(i = 0; i < fctx->nb_streams; i++) {
			if(cctx != fctx->streams[i]->codec) {
				if(fctx->streams[i]->codec)
					ga_avcodec_close(fctx->streams[i]->codec);
			} else {
				cctx = NULL;
			}
			av_freep(&fctx->streams[i]->codec);
			if(st == fctx->streams[i])
				st = NULL;
			av_freep(&fctx->streams[i]);
		}
#ifdef HOLE_PUNCHING
		// do nothing?
#else
		if(transport==RTSP_LOWER_TRANSPORT_UDP && fctx->pb)
			avio_close(fctx->pb);
#endif
		av_free(fctx);
	}
	if(cctx != NULL)
		av_free(cctx);
	if(st != NULL)
		av_free(st);
	return;
}

static void
per_client_deinit(RTSPContext *ctx) {
	int i;
	for(i = 0; i < video_source_channels()+1; i++) {
		close_av(ctx->fmtctx[i], ctx->stream[i], ctx->encoder[i], ctx->lower_transport[i]);
#ifdef HOLE_PUNCHING
		if(ctx->lower_transport[i] == RTSP_LOWER_TRANSPORT_UDP)
			rtp_close_ports(ctx, i);
#endif
	}
	//close_av(ctx->fmtctx[0], ctx->stream[0], ctx->encoder[0], ctx->lower_transport[0]);
	//close_av(ctx->fmtctx[1], ctx->stream[1], ctx->encoder[1], ctx->lower_transport[1]);
	//
#ifdef HOLE_PUNCHING
	if(ctx->sdp_fmtctx->pb)
		avio_close(ctx->sdp_fmtctx->pb);
#endif
	close_av(ctx->sdp_fmtctx, NULL, NULL, RTSP_LOWER_TRANSPORT_UDP);
	//
	if(ctx->rbuffer) {
		free(ctx->rbuffer);
	}
	ctx->rbufsize = 0;
	ctx->rbufhead = ctx->rbuftail = 0;
	//
	return;
}

static void
rtsp_reply_header(RTSPContext *c, enum RTSPStatusCode error_number) {
	const char *str;
	time_t ti;
	struct tm rtm;
	char buf2[32];

	switch(error_number) {
	case RTSP_STATUS_OK:
		str = "OK";
		break;
	case RTSP_STATUS_METHOD:
		str = "Method Not Allowed";
		break;
	case RTSP_STATUS_BANDWIDTH:
		str = "Not Enough Bandwidth";
		break;
	case RTSP_STATUS_SESSION:
		str = "Session Not Found";
		break;
	case RTSP_STATUS_STATE:
		str = "Method Not Valid in This State";
		break;
	case RTSP_STATUS_AGGREGATE:
		str = "Aggregate operation not allowed";
		break;
	case RTSP_STATUS_ONLY_AGGREGATE:
		str = "Only aggregate operation allowed";
		break;
	case RTSP_STATUS_TRANSPORT:
		str = "Unsupported transport";
		break;
	case RTSP_STATUS_INTERNAL:
		str = "Internal Server Error";
		break;
	case RTSP_STATUS_SERVICE:
		str = "Service Unavailable";
		break;
	case RTSP_STATUS_VERSION:
		str = "RTSP Version not supported";
		break;
	default:
		str = "Unknown Error";
		break;
	}

	rtsp_printf(c, "RTSP/1.0 %d %s\r\n", error_number, str);
	rtsp_printf(c, "CSeq: %d\r\n", c->seq);
	/* output GMT time */
	ti = time(NULL);
#ifdef MSYS
	gmtime_s(&rtm, &ti);
#else
	gmtime_r(&ti, &rtm);
#endif
	strftime(buf2, sizeof(buf2), "%a, %d %b %Y %H:%M:%S", &rtm);
	rtsp_printf(c, "Date: %s GMT\r\n", buf2);
	//
	return;
}

static void
rtsp_reply_error(RTSPContext *c, enum RTSPStatusCode error_number) {
	rtsp_reply_header(c, error_number);
	rtsp_printf(c, "\r\n");
}

static int
prepare_sdp_description(RTSPContext *ctx, char *buf, int bufsize) {
	buf[0] = '\0';
	av_dict_set(&ctx->sdp_fmtctx->metadata, "title", rtspconf->title, 0);
	snprintf(ctx->sdp_fmtctx->filename, sizeof(ctx->sdp_fmtctx->filename), "rtp://0.0.0.0");
	av_sdp_create(&ctx->sdp_fmtctx, 1, buf, bufsize);
	return strlen(buf);
}

static void
rtsp_cmd_describe(RTSPContext *ctx, const char *url) {
	struct sockaddr_in myaddr;
#ifdef WIN32
	int addrlen;
#else
	socklen_t addrlen;
#endif
	char path[4096];
	char content[4096];
	int content_length;
	//
	av_url_split(NULL, 0, NULL, 0, NULL, 0, NULL, path, sizeof(path), url);
	if(strcmp(path, rtspconf->object) != 0) {
		rtsp_reply_error(ctx, RTSP_STATUS_SERVICE);
		return;
	}
	//
	addrlen = sizeof(myaddr);
	getsockname(ctx->fd, (struct sockaddr*) &myaddr, &addrlen);
	content_length = prepare_sdp_description(ctx, content, sizeof(content));
	if(content_length < 0) {
		rtsp_reply_error(ctx, RTSP_STATUS_INTERNAL);
		return;
	}
	// state does not change
	rtsp_reply_header(ctx, RTSP_STATUS_OK);
	rtsp_printf(ctx, "Content-Base: %s/\r\n", url);
	rtsp_printf(ctx, "Content-Type: application/sdp\r\n");
	rtsp_printf(ctx, "Content-Length: %d\r\n", content_length);
	rtsp_printf(ctx, "\r\n");
	rtsp_write(ctx, content, content_length);
	return;
}

static void
rtsp_cmd_options(RTSPContext *c, const char *url) {
	// state does not change
	rtsp_printf(c, "RTSP/1.0 %d %s\r\n", RTSP_STATUS_OK, "OK");
	rtsp_printf(c, "CSeq: %d\r\n", c->seq);
	//rtsp_printf(c, "Public: %s\r\n", "OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE");
	rtsp_printf(c, "Public: %s\r\n", "OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY");
	rtsp_printf(c, "\r\n");
	return;
}

static RTSPTransportField *
find_transport(RTSPMessageHeader *h, enum RTSPLowerTransport lower_transport) {
	RTSPTransportField *th;
	int i;
	for(i = 0; i < h->nb_transports; i++) {
		th = &h->transports[i];
		if (th->lower_transport == lower_transport)
			return th;
	}
	return NULL;
}

static int
rtp_new_av_stream(RTSPContext *ctx, struct sockaddr_in *sin, int streamid, enum AVCodecID codecid) {
	AVOutputFormat *fmt = NULL;
	AVFormatContext *fmtctx = NULL;
	AVStream *stream = NULL;
	AVCodecContext *encoder = NULL;
	uint8_t *dummybuf = NULL;
	//
	if(streamid > VIDEO_SOURCE_CHANNEL_MAX) {
		ga_error("invalid stream index (%d > %d)\n",
			streamid, VIDEO_SOURCE_CHANNEL_MAX);
		return -1;
	}
	if(codecid != rtspconf->video_encoder_codec->id
	&& codecid != rtspconf->audio_encoder_codec->id) {
		ga_error("invalid codec (%d)\n", codecid);
		return -1;
	}
	if(ctx->fmtctx[streamid] != NULL) {
		ga_error("duplicated setup to an existing stream (%d)\n",
			streamid);
		return -1;
	}
	if((fmt = av_guess_format("rtp", NULL, NULL)) == NULL) {
		ga_error("RTP not supported.\n");
		return -1;
	}
	if((fmtctx = avformat_alloc_context()) == NULL) {
		ga_error("create avformat context failed.\n");
		return -1;
	}
	fmtctx->oformat = fmt;
	if(ctx->mtu > 0) {
		if(fmtctx->packet_size > 0) {
			fmtctx->packet_size =
				ctx->mtu < fmtctx->packet_size ? ctx->mtu : fmtctx->packet_size;
		} else {
			fmtctx->packet_size = ctx->mtu;
		}
		ga_error("RTP: packet size set to %d (configured: %d)\n",
			fmtctx->packet_size, ctx->mtu);
	}
#ifdef HOLE_PUNCHING
	if(ffio_open_dyn_packet_buf(&fmtctx->pb, ctx->mtu) < 0) {
		ga_error("cannot open dynamic packet buffer\n");
		return -1;
	}
	ga_error("RTP: Dynamic buffer opened, max_packet_size=%d.\n",
		(int) fmtctx->pb->max_packet_size);
	if(ctx->lower_transport[streamid] == RTSP_LOWER_TRANSPORT_UDP) {
		if(rtp_open_ports(ctx, streamid) < 0) {
			ga_error("RTP: open ports failed - %s\n", strerror(errno));
			return -1;
		}
	}
#else
	if(ctx->lower_transport[streamid] == RTSP_LOWER_TRANSPORT_UDP) {
		snprintf(fmtctx->filename, sizeof(fmtctx->filename),
			"rtp://%s:%d", inet_ntoa(sin->sin_addr), ntohs(sin->sin_port));
		if(avio_open(&fmtctx->pb, fmtctx->filename, AVIO_FLAG_WRITE) < 0) {
			ga_error("cannot open URL: %s\n", fmtctx->filename);
			return -1;
		}
		ga_error("RTP/UDP: URL opened [%d]: %s, max_packet_size=%d\n",
			streamid, fmtctx->filename, fmtctx->pb->max_packet_size);
	} else if(ctx->lower_transport[streamid] == RTSP_LOWER_TRANSPORT_TCP) {
		// XXX: should we use avio_open_dyn_buf(&fmtctx->pb)?
		if(ffio_open_dyn_packet_buf(&fmtctx->pb, ctx->mtu) < 0) {
			ga_error("cannot open dynamic packet buffer\n");
			return -1;
		}
		ga_error("RTP/TCP: Dynamic buffer opened, max_packet_size=%d.\n",
			(int) fmtctx->pb->max_packet_size);
	}
#endif
	fmtctx->pb->seekable = 0;
	//
	if((stream = ga_avformat_new_stream(fmtctx, 0,
			codecid == rtspconf->video_encoder_codec->id ?
				rtspconf->video_encoder_codec : rtspconf->audio_encoder_codec)) == NULL) {
		ga_error("Cannot create new stream (%d)\n", codecid);
		return -1;
	}
	//
	if(codecid == rtspconf->video_encoder_codec->id) {
		encoder = ga_avcodec_vencoder_init(
				stream->codec,
				rtspconf->video_encoder_codec,
				video_source_out_width(streamid),
				video_source_out_height(streamid),
				rtspconf->video_fps,
				rtspconf->vso);
	} else if(codecid == rtspconf->audio_encoder_codec->id) {
		encoder = ga_avcodec_aencoder_init(
				stream->codec,
				rtspconf->audio_encoder_codec,
				rtspconf->audio_bitrate,
				rtspconf->audio_samplerate,
				rtspconf->audio_channels,
				rtspconf->audio_codec_format,
				rtspconf->audio_codec_channel_layout);
	}
	if(encoder == NULL) {
		ga_error("Cannot init encoder\n");
		return -1;
	}
	//
	ctx->encoder[streamid] = encoder;
	ctx->stream[streamid] = stream;
	ctx->fmtctx[streamid] = fmtctx;
	// write header
	if(avformat_write_header(ctx->fmtctx[streamid], NULL) < 0) {
		ga_error("Cannot write stream id %d.\n", streamid);
		return -1;
	}
#ifdef HOLE_PUNCHING
	avio_close_dyn_buf(ctx->fmtctx[streamid]->pb, &dummybuf);
	av_free(dummybuf);
#else
	if(ctx->lower_transport[streamid] == RTSP_LOWER_TRANSPORT_TCP) {
		/*int rlen;
		rlen =*/ avio_close_dyn_buf(ctx->fmtctx[streamid]->pb, &dummybuf);
		av_free(dummybuf);
	}
#endif
	return 0;
}

static void
rtsp_cmd_setup(RTSPContext *ctx, const char *url, RTSPMessageHeader *h) {
	int i;
	RTSPTransportField *th;
	struct sockaddr_in destaddr, myaddr;
#ifdef WIN32
	int destaddrlen, myaddrlen;
#else
	socklen_t destaddrlen, myaddrlen;
#endif
	char path[4096];
	char channelname[VIDEO_SOURCE_CHANNEL_MAX+1][RTSP_STREAM_FORMAT_MAXLEN];
	int baselen = strlen(rtspconf->object);
	int streamid;
	int rtp_port, rtcp_port;
	enum RTSPStatusCode errcode;
	//
	av_url_split(NULL, 0, NULL, 0, NULL, 0, NULL, path, sizeof(path), url);
	for(i = 0; i < VIDEO_SOURCE_CHANNEL_MAX+1; i++) {
		snprintf(channelname[i], RTSP_STREAM_FORMAT_MAXLEN, RTSP_STREAM_FORMAT, i);
	}
	//
	if(strncmp(path, rtspconf->object, baselen) != 0) {
		ga_error("invalid object (path=%s)\n", path);
		rtsp_reply_error(ctx, RTSP_STATUS_AGGREGATE);
		return;
	}
	for(i = 0; i < VIDEO_SOURCE_CHANNEL_MAX+1; i++) {
		if(strcmp(path+baselen+1, channelname[i]) == 0) {
			streamid = i;
			break;
		}
	}
	if(i == VIDEO_SOURCE_CHANNEL_MAX+1) {
		// not found
		ga_error("invalid service (path=%s)\n", path);
		rtsp_reply_error(ctx, RTSP_STATUS_SERVICE);
		return;
	}
	//
	if(ctx->state != SERVER_STATE_IDLE
	&& ctx->state != SERVER_STATE_READY) {
		rtsp_reply_error(ctx, RTSP_STATUS_STATE);
		return;
	}
	// create session id?
	if(ctx->session_id == NULL) {
		if(h->session_id[0] == '\0') {
			snprintf(h->session_id, sizeof(h->session_id), "%04x%04x",
				rand()%0x0ffff, rand()%0x0ffff);
			ctx->session_id = strdup(h->session_id);
			ga_error("New session created (id = %s)\n", ctx->session_id);
		}
	}
	// session id must match -- we have only one session
	if(ctx->session_id == NULL
	|| strcmp(ctx->session_id, h->session_id) != 0) {
		ga_error("Bad session id %s != %s\n", h->session_id, ctx->session_id);
		errcode = RTSP_STATUS_SESSION;
		goto error_setup;
	}
	// find supported transport
	if((th = find_transport(h, RTSP_LOWER_TRANSPORT_UDP)) == NULL) {
		th = find_transport(h, RTSP_LOWER_TRANSPORT_TCP);
	}
	if(th == NULL) {
		ga_error("Cannot find transport\n");
		errcode = RTSP_STATUS_TRANSPORT;
		goto error_setup;
	}
	//
	destaddrlen = sizeof(destaddr);
	bzero(&destaddr, destaddrlen);
	if(getpeername(ctx->fd, (struct sockaddr*) &destaddr, &destaddrlen) < 0) {
		ga_error("Cannot get peername\n");
		errcode = RTSP_STATUS_INTERNAL;
		goto error_setup;
	}
	destaddr.sin_port = htons(th->client_port_min);
	//
	myaddrlen = sizeof(myaddr);
	bzero(&myaddr, myaddrlen);
	if(getsockname(ctx->fd, (struct sockaddr*) &myaddr, &myaddrlen) < 0) {
		ga_error("Cannot get sockname\n");
		errcode = RTSP_STATUS_INTERNAL;
		goto error_setup;
	}
	//
	ctx->lower_transport[streamid] = th->lower_transport;
	if(rtp_new_av_stream(ctx, &destaddr, streamid,
			streamid == video_source_channels()/*rtspconf->audio_id*/ ?
				rtspconf->audio_encoder_codec->id : rtspconf->video_encoder_codec->id) < 0) {
		ga_error("Create AV stream %d failed.\n", streamid);
		errcode = RTSP_STATUS_TRANSPORT;
		goto error_setup;
	}
	//
	ctx->state = SERVER_STATE_READY;
	rtsp_reply_header(ctx, RTSP_STATUS_OK);
	rtsp_printf(ctx, "Session: %s\r\n", ctx->session_id);
	switch(th->lower_transport) {
	case RTSP_LOWER_TRANSPORT_UDP:
#ifdef HOLE_PUNCHING
		rtp_port = ctx->rtpLocalPort[streamid*2];
		rtcp_port = ctx->rtpLocalPort[streamid*2+1];
		ctx->rtpPeerPort[streamid*2] = htons(th->client_port_min);
		ctx->rtpPeerPort[streamid*2+1] = htons(th->client_port_max);
#else
		rtp_port = ff_rtp_get_local_rtp_port((URLContext*) ctx->fmtctx[streamid]->pb->opaque);
		rtcp_port = ff_rtp_get_local_rtcp_port((URLContext*) ctx->fmtctx[streamid]->pb->opaque);
#endif
		ga_error("RTP/UDP: streamid=%d; client=%d-%d; server=%d-%d\n",
			streamid,
			th->client_port_min, th->client_port_max,
			rtp_port, rtcp_port);
		rtsp_printf(ctx, "Transport: RTP/AVP/UDP;unicast;client_port=%d-%d;server_port=%d-%d\r\n",
		       th->client_port_min, th->client_port_max,
		       rtp_port, rtcp_port);
		break;
	case RTSP_LOWER_TRANSPORT_TCP:
		ga_error("RTP/TCP: interleaved=%d-%d\n",
			streamid*2, streamid*2+1);
		rtsp_printf(ctx, "Transport: RTP/AVP/TCP;unicast;interleaved=%d-%d\r\n",
			streamid*2, streamid*2+1, streamid*2);
		break;
	default:
		// should not happen
		break;
	}
	rtsp_printf(ctx, "\r\n");
	return;
error_setup:
	if(ctx->session_id != NULL) {
		free(ctx->session_id);
		ctx->session_id = NULL;
	}
	if(ctx->encoder[streamid] != NULL) {
		ctx->encoder[streamid] = NULL;
	}
	if(ctx->stream[streamid] != NULL) {
		ctx->stream[streamid] = NULL;
	}
	if(ctx->fmtctx[streamid] != NULL) {
		avformat_free_context(ctx->fmtctx[streamid]);
		ctx->fmtctx[streamid] = NULL;
	}
	rtsp_reply_error(ctx, errcode);
	return;
}
static void
rtsp_cmd_play(RTSPContext *ctx, const char *url, RTSPMessageHeader *h) {
	char path[4096];
	//
	av_url_split(NULL, 0, NULL, 0, NULL, 0, NULL, path, sizeof(path), url);
	if(strncmp(path, rtspconf->object, strlen(rtspconf->object)) != 0) {
		rtsp_reply_error(ctx, RTSP_STATUS_SESSION);
		return;
	}
	if(strcmp(ctx->session_id, h->session_id) != 0) {
		rtsp_reply_error(ctx, RTSP_STATUS_SESSION);
		return;
	}
	//
	if(ctx->state != SERVER_STATE_READY
	&& ctx->state != SERVER_STATE_PAUSE) {
		rtsp_reply_error(ctx, RTSP_STATUS_STATE);
		return;
	}
	// 2014-05-20: support only shared-encoder model
	if(ff_server_register_client(ctx) < 0) {
		ga_error("cannot register encoder client.\n");
		rtsp_reply_error(ctx, RTSP_STATUS_INTERNAL);
		return;
	}
	//
	ctx->state = SERVER_STATE_PLAYING;
	rtsp_reply_header(ctx, RTSP_STATUS_OK);
	rtsp_printf(ctx, "Session: %s\r\n", ctx->session_id);
	rtsp_printf(ctx, "\r\n");
	return;
}

static void
rtsp_cmd_pause(RTSPContext *ctx, const char *url, RTSPMessageHeader *h) {
	char path[4096];
	//
	av_url_split(NULL, 0, NULL, 0, NULL, 0, NULL, path, sizeof(path), url);
	if(strncmp(path, rtspconf->object, strlen(rtspconf->object)) != 0) {
		rtsp_reply_error(ctx, RTSP_STATUS_SESSION);
		return;
	}
	if(strcmp(ctx->session_id, h->session_id) != 0) {
		rtsp_reply_error(ctx, RTSP_STATUS_SESSION);
		return;
	}
	//
	if(ctx->state != SERVER_STATE_PLAYING) {
		rtsp_reply_error(ctx, RTSP_STATUS_STATE);
		return;
	}
	//
	ctx->state = SERVER_STATE_PAUSE;
	rtsp_reply_header(ctx, RTSP_STATUS_OK);
	rtsp_printf(ctx, "Session: %s\r\n", ctx->session_id);
	rtsp_printf(ctx, "\r\n");
	return;
}

static void
rtsp_cmd_teardown(RTSPContext *ctx, const char *url, RTSPMessageHeader *h, int bruteforce) {
	char path[4096];
	//
	av_url_split(NULL, 0, NULL, 0, NULL, 0, NULL, path, sizeof(path), url);
	if(strncmp(path, rtspconf->object, strlen(rtspconf->object)) != 0) {
		rtsp_reply_error(ctx, RTSP_STATUS_SESSION);
		return;
	}
	if(strcmp(ctx->session_id, h->session_id) != 0) {
		rtsp_reply_error(ctx, RTSP_STATUS_SESSION);
		return;
	}
	//
	ctx->state = SERVER_STATE_TEARDOWN;
	if(bruteforce != 0)
		return;
	// XXX: well, gently response
	rtsp_reply_header(ctx, RTSP_STATUS_OK);
	rtsp_printf(ctx, "Session: %s\r\n", ctx->session_id);
	rtsp_printf(ctx, "\r\n");
	return;
}

struct RTCPHeader {
	unsigned char vps;	// version, padding, RC/SC
#define	RTCP_Version(hdr)	(((hdr)->vps) >> 6)
#define	RTCP_Padding(hdr)	((((hdr)->vps) >> 5) & 0x01)
#define	RTCP_RC(hdr)		(((hdr)->vps) & 0x1f)
#define	RTCP_SC(hdr)		RTCP_RC(hdr)
	unsigned char pt;
	unsigned short length;
}
#ifdef WIN32
;
#else
__attribute__ ((__packed__));
#endif

static int
handle_rtcp(RTSPContext *ctx, const char *buf, size_t buflen) {
#if 0
	int reqlength;
	struct RTCPHeader *rtcp;
	char msg[64] = "", *ptr = msg;
	//
	reqlength = (unsigned char) buf[2];
	reqlength <<= 8;
	reqlength += (unsigned char) buf[3];
	rtcp = (struct RTCPHeader*) (buf+4);
	//
	ga_error("TCP feedback for stream %d received (%d bytes): ver=%d; sc=%d; pt=%d; length=%d\n",
		buf[1], reqlength,
		RTCP_Version(rtcp), RTCP_SC(rtcp), rtcp->pt, ntohs(rtcp->length));
	for(int i = 0; i < 16; i++, ptr += 3) {
		snprintf(ptr, sizeof(msg)-(ptr-msg),
			"%2.2x ", (unsigned char) buf[i]);
	}
	ga_error("HEX: %s\n", msg);
#endif
	return 0;
}

static void
skip_spaces(const char **pp) {
	const char *p;
	p = *pp;
	while (*p == ' ' || *p == '\t')
		p++;
	*pp = p;
}

static void
get_word(char *buf, int buf_size, const char **pp) {
	const char *p;
	char *q;

	p = *pp;
	skip_spaces(&p);
	q = buf;
	while (!isspace(*p) && *p != '\0') {
		if ((q - buf) < buf_size - 1)
			*q++ = *p;
		p++;
	}
	if (buf_size > 0)
		*q = '\0';
	*pp = p;
}

void*
rtspserver(void *arg) {
#ifdef WIN32
	SOCKET s = *((SOCKET*) arg);
	int sinlen = sizeof(struct sockaddr_in);
#else
	int s = *((int*) arg);
	socklen_t sinlen = sizeof(struct sockaddr_in);
#endif
	const char *p;
	char buf[8192];
	char cmd[32], url[1024], protocol[32];
	int rlen;
	struct sockaddr_in sin;
	RTSPContext ctx;
	RTSPMessageHeader header1, *header = &header1;
	//int thread_ret;
	// image info
	//int iwidth = video_source_maxwidth(0);
	//int iheight = video_source_maxheight(0);
	//
	rtspconf = rtspconf_global();
	sinlen = sizeof(sin);
	getpeername(s, (struct sockaddr*) &sin, &sinlen);
	//
	bzero(&ctx, sizeof(ctx));
	if(per_client_init(&ctx) < 0) {
		ga_error("server initialization failed.\n");
		return NULL;
	}
	bcopy(&sin, &ctx.client, sizeof(ctx.client));
	ctx.state = SERVER_STATE_IDLE;
	// XXX: hasVideo is used to sync audio/video
	// This value is increased by 1 for each captured frame until it is gerater than zero
	// when this value is greater than zero, audio encoding then starts ...
	//ctx.hasVideo = -(rtspconf->video_fps>>1);	// for slow encoders?
	ctx.hasVideo = 0;	// with 'zerolatency'
	pthread_mutex_init(&ctx.rtsp_writer_mutex, NULL);
	//
	ga_error("[tid %ld] client connected from %s:%d\n",
		ga_gettid(),
		inet_ntoa(sin.sin_addr), htons(sin.sin_port));
	//
	ctx.fd = s;
	//
	do {
		int i, fdmax, active;
		fd_set rfds;
		struct timeval to;
		FD_ZERO(&rfds);
		FD_SET(ctx.fd, &rfds);
		fdmax = ctx.fd;
#ifdef HOLE_PUNCHING
		for(i = 0; i < 2*ctx.streamCount; i++) {
			FD_SET(ctx.rtpSocket[i], &rfds);
			if(ctx.rtpSocket[i] > fdmax)
				fdmax = ctx.rtpSocket[i];
		}
#endif
		to.tv_sec = 0;
		to.tv_usec = 500000;
		if((active = select(fdmax+1, &rfds, NULL, NULL, &to)) < 0) {
			ga_error("select() failed: %s\n", strerror(errno));
			goto quit;
		}
		if(active == 0) {
			// try again!
			continue;
		}
#ifdef HOLE_PUNCHING
		for(i = 0; i < 2*ctx.streamCount; i++) {
			struct sockaddr_in xsin;
#ifdef WIN32
			int xsinlen = sizeof(xsin);
#else
			socklen_t xsinlen = sizeof(xsin);
#endif
			if(FD_ISSET(ctx.rtpSocket[i], &rfds) == 0)
				continue;
			recvfrom(ctx.rtpSocket[i], buf, sizeof(buf), 0,
				(struct sockaddr*) &xsin, &xsinlen);
			if(ctx.rtpPortChecked[i] != 0)
				continue;
			// XXX: port should not flip-flop, so check only once
			if(xsin.sin_addr.s_addr != ctx.client.sin_addr.s_addr) {
				ga_error("RTP: client address mismatched? %u.%u.%u.%u != %u.%u.%u.%u\n",
					NIPQUAD(ctx.client.sin_addr.s_addr),
					NIPQUAD(xsin.sin_addr.s_addr));
				continue;
			}
			if(xsin.sin_port != ctx.rtpPeerPort[i]) {
				ga_error("RTP: client port reconfigured: %u -> %u\n",
					(unsigned int) ntohs(ctx.rtpPeerPort[i]),
					(unsigned int) ntohs(xsin.sin_port));
				ctx.rtpPeerPort[i] = xsin.sin_port;
			} else {
				ga_error("RTP: client is not under an NAT, port %d confirmed\n",
					(int) ntohs(ctx.rtpPeerPort[i]));
			}
			ctx.rtpPortChecked[i] = 1;
		}
		// is RTSP connection?
		if(FD_ISSET(ctx.fd, &rfds) == 0)
			continue;
#endif
		// read commands
		if((rlen = rtsp_getnext(&ctx, buf, sizeof(buf))) < 0) {
			goto quit;
		}
		// Interleaved binary data?
		if(buf[0] == '$') {
			handle_rtcp(&ctx, buf, rlen);
			continue;
		}
		// REQUEST line
		ga_error("%s", buf);
		p = buf;
		get_word(cmd, sizeof(cmd), &p);
		get_word(url, sizeof(url), &p);
		get_word(protocol, sizeof(protocol), &p);
		// check protocol
		if(strcmp(protocol, "RTSP/1.0") != 0) {
			rtsp_reply_error(&ctx, RTSP_STATUS_VERSION);
			goto quit;
		}
		// read headers
		bzero(header, sizeof(*header));
		do {
			int myseq = -1;
			char mysession[sizeof(header->session_id)] = "";
			if((rlen = rtsp_getnext(&ctx, buf, sizeof(buf))) < 0)
				goto quit;
			if(buf[0]=='\n' || (buf[0]=='\r' && buf[1]=='\n'))
				break;
#if 0
			ga_error("HEADER: %s", buf);
#endif
			// Special handling to CSeq & Session header
			// ff_rtsp_parse_line cannot handle CSeq & Session properly on Windows
			// any more?
			if(strncasecmp("CSeq: ", buf, 6) == 0) {
				myseq = strtol(buf+6, NULL, 10);
			}
			if(strncasecmp("Session: ", buf, 9) == 0) {
				strcpy(mysession, buf+9);
			}
			//
			ff_rtsp_parse_line(header, buf, NULL, NULL);
			//
			if(myseq > 0 && header->seq <= 0) {
				ga_error("WARNING: CSeq fixes applied (%d->%d).\n",
					header->seq, myseq);
				header->seq = myseq;
			}
			if(mysession[0] != '\0' && header->session_id[0]=='\0') {
				unsigned i;
				for(i = 0; i < sizeof(header->session_id)-1; i++) {
					if(mysession[i] == '\0'
					|| isspace(mysession[i])
					|| mysession[i] == ';')
						break;
					header->session_id[i] = mysession[i];
				}
				header->session_id[i+1] = '\0';
				ga_error("WARNING: Session fixes applied (%s)\n",
					header->session_id);
			}
		} while(1);
		// special handle to session_id
		if(header->session_id != NULL) {
			char *p = header->session_id;
			while(*p != '\0') {
				if(*p == '\r' || *p == '\n') {
					*p = '\0';
					break;
				}
				p++;
			}
		}
		// handle commands
		ctx.seq = header->seq;
		if (!strcmp(cmd, "DESCRIBE"))
			rtsp_cmd_describe(&ctx, url);
		else if (!strcmp(cmd, "OPTIONS"))
			rtsp_cmd_options(&ctx, url);
		else if (!strcmp(cmd, "SETUP"))
			rtsp_cmd_setup(&ctx, url, header);
		else if (!strcmp(cmd, "PLAY"))
			rtsp_cmd_play(&ctx, url, header);
		else if (!strcmp(cmd, "PAUSE"))
			rtsp_cmd_pause(&ctx, url, header);
		else if (!strcmp(cmd, "TEARDOWN"))
			rtsp_cmd_teardown(&ctx, url, header, 1);
		else
			rtsp_reply_error(&ctx, RTSP_STATUS_METHOD);
		if(ctx.state == SERVER_STATE_TEARDOWN) {
			break;
		}
	} while(1);
quit:
	ctx.state = SERVER_STATE_TEARDOWN;
	//
	close(ctx.fd);
	// 2014-05-20: support only share-encoder model
	ff_server_unregister_client(&ctx);
	//
	per_client_deinit(&ctx);
	//ga_error("RTSP client thread terminated (%d/%d clients left).\n",
	//	video_source_client_count(), audio_source_client_count());
	ga_error("RTSP client thread terminated.\n");
	//
	return NULL;
}

