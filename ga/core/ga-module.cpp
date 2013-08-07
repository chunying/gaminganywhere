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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <map>
#include <pthread.h>
#ifndef WIN32
#include <dlfcn.h>
#endif

#include "ga-common.h"
#include "ga-module.h"

using namespace std;

static map<struct ga_module *, struct ga_module *> mlist;

struct ga_module *
ga_load_module(const char *modname, const char *prefix) {
	char fn[1024];
	struct ga_module m, *pm;
#ifdef WIN32
	snprintf(fn, sizeof(fn), "%s.dll", modname);
#elif defined __APPLE__
	snprintf(fn, sizeof(fn), "%s.dylib", modname);
#else
	snprintf(fn, sizeof(fn), "%s.so", modname);
#endif
	if((m.handle = dlopen(fn, RTLD_NOW|RTLD_LOCAL)) == NULL) {
		ga_error("ga_load_module: load module (%s) failed - %s.\n", fn, dlerror());
		return NULL;
	}
	//
	m.init = (int (*)(void*)) ga_module_loadfunc(m.handle, prefix, "init");
	m.threadproc = (void* (*)(void*)) ga_module_loadfunc(m.handle, prefix, "threadproc");;
	m.deinit = (void (*)(void*)) ga_module_loadfunc(m.handle, prefix, "deinit");
	m.notify = (int (*)(void*,int)) ga_module_loadfunc(m.handle, prefix, "notify");
	// nothing exports?
	if(m.init == NULL
	&& m.threadproc == NULL
	&& m.deinit == NULL
	&& m.notify == NULL) {
		ga_error("ga_load_module: [%s] does not export nothing.\n", fn);
		ga_unload_module(&m);
		return NULL;
	}
	//
	if((pm = (struct ga_module*) malloc(sizeof(m))) == NULL) {
		ga_error("ga_load_module: [%s] malloc failed - %s\n", fn, strerror(errno));
		return NULL;
	}
	bcopy(&m, pm, sizeof(m));
	mlist[pm] = pm;
	//
	return pm;
}

void *
ga_module_loadfunc(HMODULE h, const char *prefix, const char *funcname) {
	void *ptr = NULL;
	char fullname[512];
	snprintf(fullname, sizeof(fullname), "%s%s", prefix, funcname);
	if((ptr = dlsym(h, fullname)) == NULL)  {
		ga_error("ga_module_loadfunc: %s - not defined.\n", fullname);
	}
	return ptr;
}

void
ga_unload_module(struct ga_module *m) {
	if(m == NULL)
		return;
	dlclose(m->handle);
	bzero(m, sizeof(struct ga_module));
	return;
}

int
ga_init_single_module(const char *name, struct ga_module *m, void *arg) {
	if(m->init == NULL)
		return 0;
	if(m->init(arg) < 0) {
		ga_error("%s init failed.\n", name);
		return -1;
	}
	return 0;
}

void
ga_init_single_module_or_quit(const char *name, struct ga_module *m, void *arg) {
	if(ga_init_single_module(name, m, arg) < 0)
		exit(-1);
	return;
}

int
ga_run_single_module(const char *name, void * (*threadproc)(void*), void *arg) {
	pthread_t t;
	if(threadproc == NULL)
		return 0;
	if(pthread_create(&t, NULL, threadproc, arg) != 0) {
		ga_error("cannot create %s thread\n", name);
		return -1;
	}
	pthread_detach(t);
	return 0;
}

void
ga_run_single_module_or_quit(const char *name, void * (*threadproc)(void*), void *arg) {
	if(ga_run_single_module(name, threadproc, arg) < 0)
		exit(-1);
	return;
}

