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

#ifndef __PIPELINE_H__
#define __PIPELINE_H__

#include <pthread.h>
#include <map>
#include <string>

typedef struct pooldata_s {
	void *ptr;
	struct pooldata_s *next;
}	pooldata_t;

class EXPORT pipeline {
private:
	std::string myname;
	// management of listeners
	pthread_mutex_t condMutex;
	std::map<long,pthread_cond_t*> condmap;
	// buffer pool queue
	pthread_mutex_t poolmutex;
	pooldata_t *bufpool;		// unused free pool
	pooldata_t *datahead, *datatail;	// occupied data
	pooldata_t * datapool_free(pooldata_t *head);
	int datacount, bufcount;
	pooldata_t * load_data_unlocked(); // load one data from work pool w/o lock
	// private data
	void *privdata;
	int privdata_size;
public:
	// static functions
	static int do_register(const char *provider, pipeline *pipe);
	static void do_unregister(const char *provider);
	static pipeline * lookup(const char *provider);
	// constructor & deconstructor
	pipeline(int privdata_size = 0);
	~pipeline();
	const char * name();
	// buffer pool
	pooldata_t * datapool_init(int n, int datasize);
	pooldata_t * allocate_data();	  // allocate one free data from free pool
	void store_data(pooldata_t *data);	  // store one data into work pool
	pooldata_t * load_data();		  // load one data from work pool
	void release_data(pooldata_t *data); // release one data into free pool
	int data_count();
	int buf_count();
	// private data
	void * alloc_privdata(int size);
	void * set_privdata(void *ptr, int size);
	void * get_privdata();
	int get_privdata_size();
	// work with clients
	void client_register(long tid, pthread_cond_t *cond);
	void client_unregister(long tid);
	int wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
	int timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime);
	void notify_all();
	void notify_one(long tid);
	int client_count();
};
#endif /* __PIPELINE_H__ */
