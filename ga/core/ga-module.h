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

#ifdef __cplusplus
extern "C" {
#endif
//////////////////////////////////////////////
struct ga_module {
	HMODULE	handle;
	int (*init)(void *arg);
	void* (*threadproc)(void *arg);
	void (*deinit)(void *arg);
	int (*notify)(void *msg, int msglen);
};
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

EXPORT struct ga_module * ga_load_module(const char *modname, const char *prefix);
EXPORT void * ga_module_loadfunc(HMODULE h, const char *prefix, const char *funcname);
EXPORT void ga_unload_module(struct ga_module *m);
EXPORT int ga_init_single_module(const char *name, struct ga_module *m, void *arg);
EXPORT void ga_init_single_module_or_quit(const char *name, struct ga_module *m, void *arg);
EXPORT int ga_run_single_module(const char *name, void * (*threadproc)(void*), void *arg);
EXPORT void ga_run_single_module_or_quit(const char *name, void * (*threadproc)(void*), void *arg);

#endif /* __GA_MODULE__ */
