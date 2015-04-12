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

/**
 * @file
 * Interfaces for bridging encoders and sink servers: the implementation.
 */

#include <pthread.h>
#include <map>
#include <list>

#include "vsource.h"
#include "encoder-common.h"

using namespace std;

static pthread_rwlock_t encoder_lock = PTHREAD_RWLOCK_INITIALIZER;
static map<void*, void*> encoder_clients; /**< Count for encoder clients */

static bool threadLaunched = false;	/**< Encoder thread is running? */

// for pts sync between encoders
static pthread_mutex_t syncmutex = PTHREAD_MUTEX_INITIALIZER;
static bool sync_reset = true;
static struct timeval synctv;

// list of encoders
static ga_module_t *vencoder = NULL;	/**< Video encoder instance */
static ga_module_t *aencoder = NULL;	/**< Audio encoder instance */
static ga_module_t *sinkserver = NULL;	/**< Sink server instance */
static void *vencoder_param = NULL;	/**< Vieo encoder parameter */
static void *aencoder_param = NULL;	/**< Audio encoder parameter */

/**
 * Compute the integer presentation timestamp based on elapsed time.
 *
 * @param samplerate [in] Sample rate used by the encoder.
 * @return The integer presentation timestamp.
 *
 * For an audio encoder, \a samplerate is the audio sample rate.
 * For a vieo encoder, \a samplerate is the video frame rate.
 */
int	// XXX: need to be int64_t ?
encoder_pts_sync(int samplerate) {
	struct timeval tv;
	long long us;
	int ret;
	//
	pthread_mutex_lock(&syncmutex);
	if(sync_reset) {
		gettimeofday(&synctv, NULL);
		sync_reset = false; 
		pthread_mutex_unlock(&syncmutex);
		return 0;
	}
	gettimeofday(&tv, NULL);
	us = tvdiff_us(&tv, &synctv);
	pthread_mutex_unlock(&syncmutex);
	ret = (int) (0.000001 * us * samplerate);
	return ret > 0 ? ret : 0;
}

/**
 * Check if the encoder has been launched.
 *
 * @return 0 if encoder is not running or 1 if encdoer is running.
 */
int
encoder_running() {
	return threadLaunched ? 1 : 0;
}

/**
 * Register a video encoder module.
 *
 * @param m [in] Pointer to the video encoder module.
 * @param param [in] Pointer to the video encoder parameter.
 * @return Currently it always returns 0.
 *
 * The encoder module is launched when a client is conneted.
 * The \a param is passed to the encoer module when the module is launched.
 */
int
encoder_register_vencoder(ga_module_t *m, void *param) {
	if(vencoder != NULL) {
		ga_error("encoder: warning - replace video encoder %s with %s\n",
			vencoder->name, m->name);
	}
	vencoder = m;
	vencoder_param = param;
	ga_error("video encoder: %s registered\n", m->name);
	return 0;
}

/**
 * Register an audio encoder module.
 *
 * @param m [in] Pointer to the audio encoder module.
 * @param param [in] Pointer to the audio encoder parameter.
 * @return Currently it always returns 0.
 *
 * The encoder module is launched when a client is conneted.
 * The \a param is passed to the encoer module when the module is launched.
 */
int
encoder_register_aencoder(ga_module_t *m, void *param) {
	if(aencoder != NULL) {
		ga_error("encoder warning - replace audio encoder %s with %s\n",
			aencoder->name, m->name);
	}
	aencoder = m;
	aencoder_param = param;
	ga_error("audio encoder: %s registered\n", m->name);
	return 0;
}

/**
 * Register a sink server module.
 *
 * @param m [in] Pointer to the sink server module.
 * @return 0 on success, or -1 on error.
 *
 * The sink server is used to receive encoded packets.
 * It can then deliver the packets to clients or store the pckets.
 *
 * A sink server MUST have implemented the \a send_packet interface.
 */
int
encoder_register_sinkserver(ga_module_t *m) {
	if(m->send_packet == NULL) {
		ga_error("encoder error: sink server %s does not define send_packet interface\n", m->name);
		return -1;
	}
	if(sinkserver != NULL) {
		ga_error("encoder warning: replace sink server %s with %s\n",
			sinkserver->name, m->name);
	}
	sinkserver = m;
	ga_error("sink server: %s registered\n", m->name);
	return 0;
}

/**
 * Get the currently registered video encoder module.
 *
 * @return Pointer to the video encoder module, or NULL if not registered.
 */
ga_module_t *
encoder_get_vencoder() {
	return vencoder;
}

/**
 * Get the currently registered audio encoder module.
 *
 * @return Pointer to the audio encoder module, or NULL if not registered.
 */
ga_module_t *
encoder_get_aencoder() {
	return aencoder;
}

/**
 * Get the currently registered sink server module.
 *
 * @return Pointer to the sink server module, or NULL if not registered.
 */
ga_module_t *
encoder_get_sinkserver() {
	return sinkserver;
}

/**
 * Register an encoder client, and start encoder modules if necessary.
 *
 * @param rtsp [in] Pointer to the encoder client context.
 * @return 0 on success, or quit the program on error.
 *
 * The \a rtsp parameter is used to count the number of connected
 * encoder clients.
 * When the number of encoder clients changes from zero to a larger number,
 * all the encoder modules are started. When the number of encoder clients
 * becomes zero, all the encoder modules are stopped.
 * GamingAnwywere now supports only share-encoder model, so each encoder
 * module only has one instance, no matter how many clients are connected.
 *
 * Note that the number of encoder clients may be not equal to
 * the actual number of clients connected to the game server
 * It depends on how a sink server manages its clients.
 * For example, the \a server-ffmpeg module registered for each connected
 * game clients, but the \a server-live module only registered one.
 */
int
encoder_register_client(void /*RTSPContext*/ *rtsp) {
	pthread_rwlock_wrlock(&encoder_lock);
	if(encoder_clients.size() == 0) {
		// initialize video encoder
		if(vencoder != NULL && vencoder->init != NULL) {
			if(vencoder->init(vencoder_param) < 0) {
				ga_error("video encoder: init failed.\n");
				exit(-1);;
			}
		}
		// initialize audio encoder
		if(aencoder != NULL && aencoder->init != NULL) {
			if(aencoder->init(aencoder_param) < 0) {
				ga_error("audio encoder: init failed.\n");
				exit(-1);
			}
		}
		// must be set before encoder starts!
		threadLaunched = true;
		// start video encoder
		if(vencoder != NULL && vencoder->start != NULL) {
			if(vencoder->start(vencoder_param) < 0) {
				pthread_rwlock_unlock(&encoder_lock);
				ga_error("video encoder: start failed.\n");
				threadLaunched = false;
				exit(-1);
			}
		}
		// start audio encoder
		if(aencoder != NULL && aencoder->start != NULL) {
			if(aencoder->start(aencoder_param) < 0) {
				pthread_rwlock_unlock(&encoder_lock);
				ga_error("audio encoder: start failed.\n");
				threadLaunched = false;
				exit(-1);
			}
		}
	}
	encoder_clients[rtsp] = rtsp;
	ga_error("encoder client registered: total %d clients.\n", encoder_clients.size());
	pthread_rwlock_unlock(&encoder_lock);
	return 0;
}

/**
 * Unregister an encoder client, and stop encoder modules if necessary.
 *
 * @param rtsp [in] Pointer to the encoder client context.
 * @return Currently it always returns 0.
 */
int
encoder_unregister_client(void /*RTSPContext*/ *rtsp) {
	pthread_rwlock_wrlock(&encoder_lock);
	encoder_clients.erase(rtsp);
	ga_error("encoder client unregistered: %d clients left.\n", encoder_clients.size());
	if(encoder_clients.size() == 0) {
		threadLaunched = false;
		ga_error("encoder: no more clients, quitting ...\n");
		if(vencoder != NULL && vencoder->stop != NULL)
			vencoder->stop(vencoder_param);
		if(vencoder != NULL && vencoder->deinit != NULL)
			vencoder->deinit(vencoder_param);
#ifdef ENABLE_AUDIO
		if(aencoder != NULL && aencoder->stop != NULL)
			aencoder->stop(aencoder_param);
		if(aencoder != NULL && aencoder->deinit != NULL)
			aencoder->deinit(aencoder_param);
#endif
		// reset packet queue
		encoder_pktqueue_reset();
		// reset sync pts
		pthread_mutex_lock(&syncmutex);
		sync_reset = true;
		pthread_mutex_unlock(&syncmutex);
	}
	pthread_rwlock_unlock(&encoder_lock);
	return 0;
}

/**
 * Send a packet to a sink server.
 *
 * @param prefix [in] Name to identify the sender. Can be any valid string.
 * @param channelId [in] Channel id.
 * @param pkt [in] The packet to be delivery.
 * @param encoderPts [in] Encoder presentation timestamp in an integer.
 * @param ptv [in] Encoder presentation timestamp in \a timeval structure.
 * @return 0 on success, or -1 on error.
 *
 * \a channelId is used to identify whether this packet is an audio packet or
 * a video packet. A video packet usually uses a channel id ranges
 * from 0 to \a N-1, where \a N is the number of video tracks (usually 1).
 * A audio packet usually uses a channel id of \a N.
 */
int
encoder_send_packet(const char *prefix, int channelId, AVPacket *pkt, int64_t encoderPts, struct timeval *ptv) {
	if(sinkserver) {
		return sinkserver->send_packet(prefix, channelId, pkt, encoderPts, ptv);
	}
	ga_error("encoder: no sink server registered.\n");
	return -1;
}

// encoder packet queue functions - for async packet delivery
static int pktqueue_initqsize = -1;
static int pktqueue_initchannels = -1;
static encoder_packet_queue_t pktqueue[VIDEO_SOURCE_CHANNEL_MAX+1];
static list<encoder_packet_t> pktlist[VIDEO_SOURCE_CHANNEL_MAX+1];
static map<qcallback_t,qcallback_t>queue_cb[VIDEO_SOURCE_CHANNEL_MAX+1];

/**
 * Initialize an encoder packet queue.
 *
 * @param channels [in] Number of channels.
 * @param qsize [in] Size of each queue in bytes.
 * @return 0 on success, or quit the program on error.
 *
 * This function creates a packet queue of size \a qsize for each channel.
 * This functoin should be called only once.
 * If you have multiple channels, specify the number in the \a channels 
 * parameter.
 */
int
encoder_pktqueue_init(int channels, int qsize) {
	int i;
	for(i = 0; i < channels; i++) {
		if(pktqueue[i].buf != NULL)
			free(pktqueue[i].buf);
		//
		bzero(&pktqueue[i], sizeof(encoder_packet_queue_t));
		pthread_mutex_init(&pktqueue[i].mutex, NULL);
		if((pktqueue[i].buf = (char *) malloc(qsize)) == NULL) {
			ga_error("encoder: initialized packet queue#%d failed (%d bytes)\n",
				i, qsize);
			exit(-1);
		}
		pktqueue[i].bufsize = qsize;
		pktqueue[i].datasize = 0;
		pktqueue[i].head = 0;
		pktqueue[i].tail = 0;
		pktlist[i].clear();
	}
	pktqueue_initqsize = qsize;
	pktqueue_initchannels = channels;
	ga_error("encoder: packet queue initialized (%dx%d bytes)\n", channels, qsize);
	return 0;
}

/**
 * Empty packets stored in all packet queues.
 */
int
encoder_pktqueue_reset() {
	int i;
	if(pktqueue_initchannels <= 0)
		return -1;
	for(i = 0; i < pktqueue_initchannels; i++) {
		encoder_pktqueue_reset_channel(i);
	}
	return 0;
}

/**
 * Empty packets stored in a single packet queue.
 *
 * @param channelId [in] Chennel id.
 */
int
encoder_pktqueue_reset_channel(int channelId) {
	pthread_mutex_lock(&pktqueue[channelId].mutex);
	pktlist[channelId].clear();
	pktqueue[channelId].head = pktqueue[channelId].tail = 0;
	pktqueue[channelId].datasize = 0;
	pktqueue[channelId].bufsize = pktqueue_initqsize;
	pthread_mutex_unlock(&pktqueue[channelId].mutex);
	return 0;
}

/**
 * Return the occupied size of a packet queue for a given channel.
 *
 * @param channelId [in] The channel id to be read.
 * @return The occupied size in bytes.
 */
int
encoder_pktqueue_size(int channelId) {
	return pktqueue[channelId].datasize;
}

/**
 * Add a packet into a packet queue.
 *
 * @param channelId [in] The channel id.
 * @param pkt [in] The packet to be stored.
 * @param encoderPts [in] The presentation timestamp in an integer.
 * @param ptv [in] The presentation timestamp in a \timeval structure.
 * @return 0 on success, or -1 on error.
 *
 * The content of \a pkt is copied into the queue buffer, so it can be released
 * after returing from the function.
 */
int
encoder_pktqueue_append(int channelId, AVPacket *pkt, int64_t encoderPts, struct timeval *ptv) {
	encoder_packet_queue_t *q = &pktqueue[channelId];
	encoder_packet_t qp;
	map<qcallback_t,qcallback_t>::iterator mi;
	int padding = 0;
	pthread_mutex_lock(&q->mutex);
size_check:
	// size checking
	if(q->datasize + pkt->size > q->bufsize) {
		pthread_mutex_unlock(&q->mutex);
		ga_error("encoder: packet queue #%d full, packet dropped (%d+%d)\n",
			channelId, q->datasize, pkt->size);
		return -1;
	}
	// end-of-buffer space is not sufficient
	if(q->bufsize - q->tail < pkt->size) {
		if(pktlist[channelId].size() == 0) {
			q->datasize = q->tail = q->head = 0;
		} else {
			padding = q->bufsize - q->tail;
			pktlist[channelId].back().padding = padding;
			q->datasize += padding;
			q->tail = 0;
		}
		goto size_check;
	}
	bcopy(pkt->data, q->buf + q->tail, pkt->size);
	//
	qp.data = q->buf + q->tail;
	qp.size = pkt->size;
	qp.pts_int64 = pkt->pts;
	if(ptv != NULL) {
		qp.pts_tv = *ptv;
	} else {
		gettimeofday(&qp.pts_tv, NULL);
	}
	//qp.pos = q->tail;
	qp.padding = 0;
	//
	q->tail += pkt->size;
	q->datasize += pkt->size;
	pktlist[channelId].push_back(qp);
	//
	if(q->tail == q->bufsize)
		q->tail = 0;
	//
	pthread_mutex_unlock(&q->mutex);
	// notify client
	for(mi = queue_cb[channelId].begin(); mi != queue_cb[channelId].end(); mi++) {
		mi->second(channelId);
	}
	//
	return 0;
}

/**
 * Read the first packet from the packet queue.
 *
 * @param channelId [in] The channel id.
 * @param pkt [out] The pointer to stored a retrieved packet.
 * @return Pointer equal to \a pkt->data, or NULL or error.
 *
 * This funcion ONLY reads the first packet.
 * It DOES NOT remove the packet from the queue.
 */
char *
encoder_pktqueue_front(int channelId, encoder_packet_t *pkt) {
	encoder_packet_queue_t *q = &pktqueue[channelId];
	pthread_mutex_lock(&q->mutex);
	if(pktlist[channelId].size() == 0) {
		pthread_mutex_unlock(&q->mutex);
		return NULL;
	}
	*pkt = pktlist[channelId].front();
	pthread_mutex_unlock(&q->mutex);
	return pkt->data;
}

/**
 * Split the first packet in the packet queue into two packets.
 *
 * @param channelId [in] The channel id.
 * @param offset [in] The point to split the packet data.
 *
 * This function is used when you do not have sufficient buffer to handle
 * an entire packet, so you can split the first packet in the queue into
 * two independent packets.
 *
 * Suppose a sink server reads the first packet of size \a M and only
 * \a N bytes can be * processed, it has to compute \a pkt->data+N and pass
 * the value as the \a offset parameter to this functoin.
 * When this function returns, the first packet in the queue would hold
 * exact \a N bytes and the rest \a (M-N) bytes would be helded
 * in the second packet.
 */
void
encoder_pktqueue_split_packet(int channelId, char *offset) {
	encoder_packet_queue_t *q = &pktqueue[channelId];
	encoder_packet_t *pkt, newpkt;
	pthread_mutex_lock(&q->mutex);
	// has packet?
	if(pktlist[channelId].size() == 0)
		goto quit_split_packet;
	pkt = &pktlist[channelId].front();
	// offset must be in the middle
	if(offset <= pkt->data || offset >= pkt->data + pkt->size)
		goto quit_split_packet;
	// split the packet: the new one
	newpkt = *pkt;
	newpkt.size = offset - pkt->data;
	newpkt.padding = 0;
	//
	pkt->data = offset;
	pkt->size -= newpkt.size;
	//
	pktlist[channelId].push_front(newpkt);
	//
	pthread_mutex_unlock(&q->mutex);
	return;
quit_split_packet:
	pthread_mutex_unlock(&q->mutex);
	return;
}

/**
 * Remove the first packet from the queue.
 *
 * @parm channelId [in] The channel id.
 */
void
encoder_pktqueue_pop_front(int channelId) {
	encoder_packet_queue_t *q = &pktqueue[channelId];
	encoder_packet_t qp;
	pthread_mutex_lock(&q->mutex);
	if(pktlist[channelId].size() == 0) {
		pthread_mutex_unlock(&q->mutex);
		return;
	}
	qp = pktlist[channelId].front();
	pktlist[channelId].pop_front();
	// update the packet queue
	q->head += qp.size;
	q->head += qp.padding;
	q->datasize -= qp.size;
	q->datasize -= qp.padding;
	if(q->head == q->bufsize) {
		q->head = 0;
	}
	if(q->head == q->tail) {
		q->head = q->tail = 0;
	}
	//
	pthread_mutex_unlock(&q->mutex);
	return;
}

/**
 * Register a callback function for a packet queue.
 *
 * @param channelId [in] The channel id.
 * @param cb [in] Pointer to the callback function.
 * @return This function alywas return 0.
 *
 * The callback function \a cb is called when a packet is appended into the
 * queue. The callback function must be in the form of:\n
 * \a void \a callback_function(int \a channelId);
 *
 * Note that a packet queue can have multiple callback functions, and
 * all of them are called on packet appending.
 */
int
encoder_pktqueue_register_callback(int channelId, qcallback_t cb) {
	queue_cb[channelId][cb] = cb;
	ga_error("encoder: pktqueue #%d callback registered (%p)\n", channelId, cb);
	return 0;
}

/**
 * Remove a callback function from a packet queue.
 *
 * @param channelId [in] The channel id.
 * @param cb [in] The callback function to be removed.
 * @return This functon always returns 0.
 */
int
encoder_pktqueue_unregister_callback(int channelId, qcallback_t cb) {
	queue_cb[channelId].erase(cb);
	return 0;
}

