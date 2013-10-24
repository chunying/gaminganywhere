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
#include <dlfcn.h>

#include "ga-common.h"
#include "ga-hook-lib.h"
#ifdef __APPLE__
#include "mach_hook.h"
#else
#include "elf_hook.h"
#endif

struct hookentry {
	const char *name;
	void *function;
};

#ifdef __APPLE__
#define	LIBRARY_ADDRESS_BY_HANDLE(x)	NULL

static void *
elf_hook(char const *libpath, void const *unused, char const *name, void const *func) {
#if 1
	return NULL;
#else
	void *libaddr, *handle, *first;
	char *ptr;
	Dl_info info;
	FILE *fp;
	char cmd[1024];
	// find one function from libpath
	snprintf(cmd, sizeof(cmd), "nm %s | grep ' T ' | head -n 2", libpath);
	if((fp = popen(cmd, "r")) == NULL)
		return NULL;
	if(fgets(cmd, sizeof(cmd), fp) == NULL) {
		pclose(fp);
		return NULL;
	}
	pclose(fp);
	// move to end
	for(ptr = cmd; *ptr; ptr++)
		;
	// seek for the last ' ' or '\t'
	for(--ptr; ptr >= cmd; ptr--) {
		if(*ptr==0x0d || *ptr==0x0a) {
			*ptr = '\0';
		}
		if(*ptr==' ' || *ptr=='\t') {
			*ptr++ = '\0';
			if(*ptr == '_')
				*ptr++ = '\0';
			break;
		}
	}
	// get the function address
	ga_error("Retrieve base address via function '%s' from %s\n",
		ptr, libpath);
	if((handle = dlopen(libpath, RTLD_LAZY)) == NULL)
		return NULL;
	if((first = dlsym(handle, ptr)) == NULL)
		return NULL;
	//
	if(dladdr((void const *) first, &info) == 0)
		return NULL;
	libaddr = mach_hook_init(libpath, info.dli_fbase);
	if(libaddr == NULL)
		return NULL;
	return (void*) mach_hook(libaddr, name, (mach_substitution) func);
#endif
}
#endif

#define	MAX_HOOKS	1024
static int nhooks = 0;
static struct hookentry hooks[MAX_HOOKS];

static int comp_hookentry(const void *a, const void *b) {
	struct hookentry *ha = (struct hookentry*) a;
	struct hookentry *hb = (struct hookentry*) b;
	return strcmp(ha->name, hb->name);
}

static void *
hook_dlopen(const char *filename, int flag) {
	ga_error("dlopen: open %s\n", filename);
	return dlopen(filename, flag);
}

static void *
hook_dlsym(void *handle, const char *symbol) {
	struct hookentry *ph, h = { symbol, NULL };
	//
	if((ph = (struct hookentry*) bsearch(&h, hooks,
			nhooks, sizeof(struct hookentry),
			comp_hookentry)) != NULL) {
		ga_error("dlsym: %s hooked.\n", symbol);
		return ph->function;
	}
	//
	return dlsym(handle, symbol);
}

void
register_dlsym_hooks(const char *symbol, void *callback) {
	struct hookentry *ph, h = { symbol, NULL };
	// existing?
	if((ph = (struct hookentry*) bsearch(&h, hooks,
			nhooks, sizeof(struct hookentry),
			comp_hookentry)) != NULL) {
		ph->function = callback;
		return;
	}
	// not found
	hooks[nhooks].name = strdup(symbol);
	hooks[nhooks].function = callback;
	nhooks++;
	//
	qsort(hooks, nhooks, sizeof(struct hookentry), comp_hookentry);
	//
	return;
}

int
hook_libdl(const char *libpath) {
	void *handle, *original;
	handle = dlopen(libpath, RTLD_LAZY);
	if(handle == NULL)
		return -1;
	//
	original = elf_hook(libpath,
			LIBRARY_ADDRESS_BY_HANDLE(handle),
			"dlopen", (const void *) hook_dlopen);
	if(original == NULL)
		return -1;
	//
	original = elf_hook(libpath,
			LIBRARY_ADDRESS_BY_HANDLE(handle),
			"dlsym", (const void *) hook_dlsym);
	if(original == NULL)
		return -1;
	//
	return 0;
}

int
hook_lib_generic(const char *libpath, void *handle, const char *name, void *func) {
	void *original;
	if(handle == NULL)
		return -1;
	original = elf_hook(libpath,
			LIBRARY_ADDRESS_BY_HANDLE(handle),
			name, (const void *) func);
	if(original == NULL)
		return -1;
	ga_error("hook library - %s:%s success.\n", libpath, name);
	return 0;
}

