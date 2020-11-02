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
#include <stdlib.h>
#include <string.h>

#include "ga-common.h"

#include "pipeline.h"

#include <string>
using namespace std;

//////////////////////////////////////////////////////////////////////////////

static pthread_mutex_t pipelinemutex = PTHREAD_MUTEX_INITIALIZER;
static map<string,pipeline*> pipelinemap;

int
pipeline::do_register(const char *provider, pipeline *pipe) {
	pthread_mutex_lock(&pipelinemutex);
	if(pipelinemap.find(provider) != pipelinemap.end()) {
		pthread_mutex_unlock(&pipelinemutex);
		ga_error("pipeline: duplicated pipeline '%s'\n", provider);
		return -1;
	}
	pipelinemap[provider] = pipe;
	pthread_mutex_unlock(&pipelinemutex);
	pipe->myname = provider;
	ga_error("pipeline: new pipeline '%s' registered.\n", provider);
	return 0;
}

void
pipeline::do_unregister(const char *provider) {
	pthread_mutex_lock(&pipelinemutex);
	pipelinemap.erase(provider);
	pthread_mutex_unlock(&pipelinemutex);
	ga_error("pipeline: pipeline '%s' unregistered.\n", provider);
	return;
}

pipeline *
pipeline::lookup(const char *provider) {
	map<string,pipeline*>::iterator mi;
	pipeline *pipe = NULL;
	pthread_mutex_lock(&pipelinemutex);
	if((mi = pipelinemap.find(provider)) == pipelinemap.end()) {
		pthread_mutex_unlock(&pipelinemutex);
		return NULL;
	}
	pipe = mi->second;
	pthread_mutex_unlock(&pipelinemutex);
	return pipe;
}

//////////////////////////////////////////////////////////////////////////////

pipeline::pipeline(int privdata_size) {
	pthread_mutex_init(&condMutex, NULL);
	pthread_mutex_init(&poolmutex, NULL);
	bufpool = NULL;
	datahead = datatail = NULL;
	privdata = NULL;
	privdata_size = 0;
	if(privdata_size > 0) {
		alloc_privdata(privdata_size);
	}
	return;
}

pipeline::~pipeline() {
	bufpool = datapool_free(bufpool);
	datahead = datapool_free(datahead);
	datatail = NULL;
	if(privdata) {
		free(privdata);
		privdata = NULL;
		privdata_size = 0;
	}
	return;
}

const char *
pipeline::name() {
	return myname.c_str();
}

pooldata_t *
pipeline::datapool_init(int n, int datasize) {
	int i;
	pooldata_t *data;
	//
	if(n <= 0 || datasize <= 0)
		return NULL;
	//
	bufpool = NULL;
	for(i = 0; i < n; i++) {
		if((data = (pooldata_t*) malloc(sizeof(pooldata_t) + datasize)) == NULL) {
			bufpool = datapool_free(bufpool);
			return NULL;
		}
		bzero(data, sizeof(pooldata_t) + datasize);
		data->ptr = ((unsigned char*) data) + sizeof(pooldata_t);
		data->next = bufpool;
		bufpool = data;
	}
	datacount = 0;
	bufcount = n;
	return bufpool;
}

pooldata_t *
pipeline::datapool_free(pooldata_t *head) {
	pooldata_t *next;
	//
	if(head == NULL)
		return NULL;
	//
	do {
		next = head->next;
		free(head);
		head = next;
	} while(head != NULL);
	//
	bufpool = datahead = datatail = NULL;
	datacount = bufcount = 0;
	//
	return NULL;
}

pooldata_t *
pipeline::allocate_data() {
	// allocate a data from buffer pool
	pooldata_t *data = NULL;
	pthread_mutex_lock(&poolmutex);
	if(bufpool == NULL) {
		// no more available free data - force to release the eldest one
		data = load_data_unlocked();
		if(data == NULL) {
			ga_error("data pool: FATAL - unexpected NULL data returned (pipe '%s', data=%d, free=%d).\n",
				this->name(), datacount, bufcount);
			exit(-1);
		}
	} else {
		data = bufpool;
		bufpool = data->next;
		data->next = NULL;
		bufcount--;
	}
	pthread_mutex_unlock(&poolmutex);
	return data;
}

void
pipeline::store_data(pooldata_t *data) {
	// store a data into data pool (at the end)
	data->next = NULL;
	pthread_mutex_lock(&poolmutex);
	if(datatail == NULL) {
		// data pool is empty
		datahead = datatail = data;
	} else {
		// data pool is not empty
		datatail->next = data;
		datatail = data;
	}
	datacount++;
	pthread_mutex_unlock(&poolmutex);
	return;
}

pooldata_t *
pipeline::load_data_unlocked() {
	// load a data from data (work) pool
	pooldata_t *data;
	if(datatail == NULL) {
		return NULL;
	}
	data = datahead;
	datahead = data->next;
	data->next = NULL;
	if(datahead == NULL)
		datatail = NULL;
	datacount--;
	return data;
}

pooldata_t *
pipeline::load_data() {
	pooldata_t *data;
	pthread_mutex_lock(&poolmutex);
	data = load_data_unlocked();
	pthread_mutex_unlock(&poolmutex);
	return data;
}

void
pipeline::release_data(pooldata_t *data) {
	// return a data to buffer pool
	pthread_mutex_lock(&poolmutex);
	data->next = bufpool;
	bufpool = data;
	bufcount++;
	pthread_mutex_unlock(&poolmutex);
	return;
}

int
pipeline::data_count() {
	return datacount;
}

int
pipeline::buf_count() {
	return bufcount;
}

void *
pipeline::alloc_privdata(int size) {
	if(privdata == NULL) {
alloc_again:
		if((privdata = malloc(size)) != NULL) {
			privdata_size = size;
		} else {
			privdata_size = 0;
		}
		return privdata;
	} else if(size <= privdata_size) {
		return privdata;
	}
	// privdata != NULL & size > privdata_size
	free(privdata);
	goto alloc_again;
	// never return from here
	return NULL;
}

void *
pipeline::set_privdata(void *ptr, int size) {
	if(size <= privdata_size) {
		bcopy(ptr, privdata, size);
		return privdata;
	}
	return NULL;
}

void *
pipeline::get_privdata() {
	return privdata;
}

int
pipeline::get_privdata_size() {
	return privdata_size;
}

void
pipeline::client_register(long tid, pthread_cond_t *cond) {
	pthread_mutex_lock(&condMutex);
	condmap[tid] = cond;
	pthread_mutex_unlock(&condMutex);
	return;
}

void
pipeline::client_unregister(long tid) {
	pthread_mutex_lock(&condMutex);
	condmap.erase(tid);
	pthread_mutex_unlock(&condMutex);
	return;
}

int
pipeline::wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
	int ret;
	pthread_mutex_lock(mutex);
	ret = pthread_cond_wait(cond, mutex);
	pthread_mutex_unlock(mutex);
	return ret;
}

int 
pipeline::timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) {
	int ret;
	pthread_mutex_lock(mutex);
	ret = pthread_cond_timedwait(cond, mutex, abstime);
	pthread_mutex_unlock(mutex);
	return ret;
}

void
pipeline::notify_all() {
	map<long,pthread_cond_t*>::iterator mi;
	pthread_mutex_lock(&condMutex);
	for(mi = condmap.begin(); mi != condmap.end(); mi++) {
		pthread_cond_signal(mi->second);
	}
	pthread_mutex_unlock(&condMutex);
	return;
}

void
pipeline::notify_one(long tid) {
	map<long,pthread_cond_t*>::iterator mi;
	pthread_mutex_lock(&condMutex);
	if((mi = condmap.find(tid)) != condmap.end()) {
		pthread_cond_signal(mi->second);
	}
	pthread_mutex_unlock(&condMutex);
	return;
}

int
pipeline::client_count() {
	int n;
	pthread_mutex_lock(&condMutex);
	n = (int) condmap.size();
	pthread_mutex_unlock(&condMutex);
	return n;
}

