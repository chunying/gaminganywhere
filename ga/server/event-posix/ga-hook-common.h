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

#ifndef __GA_HOOK_COMMON_H__
#define __GA_HOOK_COMMON_H__

#include "dpipe.h"
#include "vsource.h"

#define	SOURCES			1

#define	GA_HOOK_INVALID_THREADID	0xffffffff

/** COM Interface vtable pointers */
typedef struct vtable_s {
	void *func[1];
}	vtable_t;

/** COM Interface pointer to vtable */
typedef struct comobj_s {
	vtable_t *vtbl;
}	comobj_t;

extern int vsource_initialized;
extern int resolution_retrieved;
extern int game_width, game_height;
extern int encoder_width, encoder_height;
extern int hook_boost;
extern int no_default_controller;

extern int enable_server_rate_control;
extern int server_token_fill_interval;
extern int server_num_token_to_fill;
extern int server_max_tokens;
extern int video_fps;

extern dpipe_t *g_pipe[SOURCES];

int vsource_init(int width, int height);

int ga_hook_capture_prepared(int width, int height, int check_resolution);
void ga_hook_capture_dupframe(vsource_frame_t *frame);

void *ga_server(void *arg);
int ga_hook_get_resolution(int width, int height);
int ga_hook_video_rate_control();
int ga_hook_init();
#ifndef WIN32
void * ga_hook_lookup(void *handle, const char *name);
void * ga_hook_lookup_or_quit(void *handle, const char *name);
#else
void ga_hook_function(const char *id, void *oldfunc, void *newfunc);
#endif

#endif	/* __GA_HOOK_COMMON_H__ */
