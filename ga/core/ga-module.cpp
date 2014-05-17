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

static map<ga_module_t *, HMODULE> mlist;

ga_module_t *
ga_load_module(const char *modname, const char *prefix) {
	char fn[1024];
	ga_module_t *m;
	HMODULE handle;
	ga_module_t * (*do_module_load)();
#ifdef WIN32
	snprintf(fn, sizeof(fn), "%s.dll", modname);
#elif defined __APPLE__
	snprintf(fn, sizeof(fn), "%s.dylib", modname);
#else
	snprintf(fn, sizeof(fn), "%s.so", modname);
#endif
	//
	if((handle = dlopen(fn, RTLD_NOW|RTLD_LOCAL)) == NULL) {
		ga_error("ga_load_module: load module (%s) failed - %s.\n", fn, dlerror());
		return NULL;
	}
	//if((loadfunc = ga_module_loadfunc(handle, prefix, "load")) == NULL) {
	if((do_module_load = (ga_module_t * (*)()) dlsym(handle, "module_load")) == NULL) {
		ga_error("ga_load_module: [%s] is not a valid module.\n", fn);
		dlclose(handle);
		return NULL;
	}
	if((m = do_module_load()) != NULL) {
		mlist[m] = handle;
	}
	//
	return m;
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
ga_unload_module(ga_module_t *m) {
	map<ga_module_t *, HMODULE>::iterator mi;
	if(m == NULL)
		return;
	if((mi = mlist.find(m)) == mlist.end())
		return;
	dlclose(mi->second);
	mlist.erase(mi);
	return;
}

int
ga_init_single_module(const char *name, ga_module_t *m, void *arg) {
	if(m->init == NULL)
		return 0;
	if(m->init(arg) < 0) {
		ga_error("%s init failed.\n", name);
		return -1;
	}
	return 0;
}

void
ga_init_single_module_or_quit(const char *name, ga_module_t *m, void *arg) {
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

