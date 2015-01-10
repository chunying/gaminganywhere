/*
 * Copyright (c) 2013-2015 Chun-Ying Huang
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

#ifndef __GA_DPIPE_H__
#define	__GA_DPIPE_H__

/**
 * @file 
 * dpipe header: pipe for delivering discrete frames
 */

#include <stdio.h>
#include <pthread.h>

#include "ga-common.h"

/**
 * structure for buffering a frame
 */
typedef struct dpipe_buffer_s {
	void *pointer;		/**< pointer to a frame buffer. Aligned to 8-byte address: is equivalent to internal + offset */
	void *internal;		/**< internal pointer to the allocated buffer space. Used with malloc() and free(). */
	int offset;		/**< data pointer offset from internal */
	struct dpipe_buffer_s *next;	/**< pointer to the next dpipe frame buffer */
}	dpipe_buffer_t;

typedef struct dpipe_s {
	int channel_id;		/**< channel id for the dpipe */
	char *name;		/**< name of the dpipe */
	//
	pthread_mutex_t cond_mutex;	/**< pthread mutex for conditional signaling */
	pthread_cond_t cond;		/**< pthread condition */
	//
	pthread_mutex_t io_mutex;	/**< dpipe i/o pool operation mutex */
	dpipe_buffer_t *in;		/**< input pool: pointer to the first frame buffer in input pool (free frames) */
	dpipe_buffer_t *out;		/**< output pool: pointer to the first frame buffer in output pool (occupied frames) */
	dpipe_buffer_t *out_tail;	/**< output pool: pointer to the last frame buffer in output pool (occupied frames) */
	int in_count;			/**< number of unused frame buffers */
	int out_count;			/**< number of occupied frames */
}	dpipe_t;

EXPORT dpipe_t *	dpipe_create(int id, const char *name, int nframe, int maxframesize);
EXPORT dpipe_t *	dpipe_lookup(const char *name);
EXPORT int		dpipe_destroy(dpipe_t *dpipe);
EXPORT dpipe_buffer_t *	dpipe_get(dpipe_t *dpipe);
EXPORT void		dpipe_put(dpipe_t *dpipe, dpipe_buffer_t *buffer);
EXPORT dpipe_buffer_t *	dpipe_load(dpipe_t *dpipe, const struct timespec *abstime);
EXPORT dpipe_buffer_t *	dpipe_load_nowait(dpipe_t *dpipe);
EXPORT void		dpipe_store(dpipe_t *dpipe, dpipe_buffer_t *buffer);

#endif	/* __GA_DPIPE_H__ */
