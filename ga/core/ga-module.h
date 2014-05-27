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

#ifndef __GA_MODULE__
#define __GA_MODULE__

#include "ga-common.h"

#ifndef WIN32
typedef void * HMODULE;
#endif

enum ga_module_types {
	GA_MODULE_TYPE_NULL = 0,
	GA_MODULE_TYPE_CONTROL,
	GA_MODULE_TYPE_ASOURCE,
	GA_MODULE_TYPE_VSOURCE,
	GA_MODULE_TYPE_FILTER,
	GA_MODULE_TYPE_AENCODER,
	GA_MODULE_TYPE_VENCODER,
	GA_MODULE_TYPE_ADECODER,
	GA_MODULE_TYPE_VDECODER
};

#ifdef __cplusplus
extern "C" {
#endif
//////////////////////////////////////////////
typedef struct ga_module_s {
	HMODULE	handle;
	int type;
	char *name;
	char *mimetype;
	int (*init)(void *arg);
	int (*start)(void *arg);
	//void * (*threadproc)(void *arg);
	int (*stop)(void *arg);
	int (*deinit)(void *arg);
	int (*notify)(void *arg);
	void * (*raw)(void *arg, int *size);
	void * (*option1)(void *arg, int *size);
	void * (*option2)(void *arg, int *size);
	void * (*option3)(void *arg, int *size);
	void * (*option4)(void *arg, int *size);
	void * privdata;
}	ga_module_t;
//////////////////////////////////////////////
#ifdef __cplusplus
}
#endif

#define	MODULE	extern "C"
#if defined WIN32 && defined GA_MODULE
#define	MODULE_EXPORT	__declspec(dllexport)
#else
#define	MODULE_EXPORT
#endif

EXPORT ga_module_t * ga_load_module(const char *modname, const char *prefix);
EXPORT void * ga_module_loadfunc(HMODULE h, const char *prefix, const char *funcname);
EXPORT void ga_unload_module(ga_module_t *m);
EXPORT int ga_init_single_module(const char *name, ga_module_t *m, void *arg);
EXPORT void ga_init_single_module_or_quit(const char *name, ga_module_t *m, void *arg);
EXPORT int ga_run_single_module(const char *name, void * (*threadproc)(void*), void *arg);
EXPORT void ga_run_single_module_or_quit(const char *name, void * (*threadproc)(void*), void *arg);

#ifdef GA_MODULE
// a module must have exported the module_load function
MODULE MODULE_EXPORT ga_module_t * module_load();
#endif

#endif /* __GA_MODULE__ */
