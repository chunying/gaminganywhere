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

#ifndef __GA_HOOK_PULSE_H__
#define __GA_HOOK_PULSE_H__

#include <pulse/simple.h>
#include <pulse/stream.h>
#include <pulse/error.h>

#ifdef __cplusplus
extern "C" {
#endif
// simple
typedef pa_simple* (*t_pa_simple_new)(const char * server,
		const char * name,
		pa_stream_direction_t dir,
		const char * dev,
		const char * stream_name,
		const pa_sample_spec * ss,
		const pa_channel_map * map,
		const pa_buffer_attr * attr,
		int * error);
typedef void (*t_pa_simple_free)(pa_simple * s);
typedef int (*t_pa_simple_write)(pa_simple * s,
		const void * data,
		size_t bytes,
		int * error);
// async
typedef pa_stream* (*t_pa_stream_new)(pa_context * c,
		const char * name,
		const pa_sample_spec * ss,
		const pa_channel_map * map);
typedef pa_stream* (*t_pa_stream_new_extended)(pa_context * c,
		const char * name,
		pa_format_info *const * formats,
		unsigned int n_formats,
		pa_proplist * p);
typedef pa_stream* (*t_pa_stream_new_with_proplist)(pa_context * c,
		const char * name,
		const pa_sample_spec * ss,
		const pa_channel_map * map,
		pa_proplist * p);
typedef int (*t_pa_stream_connect_playback)(pa_stream * s,
		const char * dev,
		const pa_buffer_attr * attr,
		pa_stream_flags_t flags,
		const pa_cvolume * volume,
		pa_stream * sync_stream);
typedef void (*t_pa_stream_set_write_callback)(pa_stream * p,
		pa_stream_request_cb_t cb,
		void * userdata);
typedef int (*t_pa_stream_write)(pa_stream * p,
		const void * data,
		size_t nbytes,
		pa_free_cb_t free_cb,
		int64_t offset,
		pa_seek_mode_t seek);
#ifdef __cplusplus
}
#endif


// simple
extern t_pa_simple_new		old_pa_simple_new;
extern t_pa_simple_free		old_pa_simple_free;
extern t_pa_simple_write	old_pa_simple_write;
// async
extern t_pa_stream_new			old_pa_stream_new;
extern t_pa_stream_new_extended		old_pa_stream_new_extended;
extern t_pa_stream_new_with_proplist	old_pa_stream_new_with_proplist;
extern t_pa_stream_connect_playback	old_pa_stream_connect_playback;
extern t_pa_stream_set_write_callback	old_pa_stream_set_write_callback;
extern t_pa_stream_write		old_pa_stream_write;

// simple

pa_simple* hook_pa_simple_new(const char * server,
		const char * name,
		pa_stream_direction_t dir,
		const char * dev,
		const char * stream_name,
		const pa_sample_spec * ss,
		const pa_channel_map * map,
		const pa_buffer_attr * attr,
		int * error);
int hook_pa_simple_write(pa_simple * s,
		const void * data,
		size_t bytes,
		int * error);
void hook_pa_simple_free(pa_simple * s);

// async

pa_stream* hook_pa_stream_new(pa_context * c,
		const char * name,
		const pa_sample_spec * ss,
		const pa_channel_map * map);
pa_stream* hook_pa_stream_new_extended(pa_context * c,
		const char * name,
		pa_format_info *const * formats,
		unsigned int n_formats,
		pa_proplist * p);
pa_stream* hook_pa_stream_new_with_proplist(pa_context * c,
		const char * name,
		const pa_sample_spec * ss,
		const pa_channel_map * map,
		pa_proplist * p);
int hook_pa_stream_connect_playback(pa_stream * s,
		const char * dev,
		const pa_buffer_attr * attr,
		pa_stream_flags_t flags,
		const pa_cvolume * volume,
		pa_stream * sync_stream);
void hook_pa_stream_set_write_callback(pa_stream * p,
		pa_stream_request_cb_t cb,
		void * userdata);
int hook_pa_stream_write(pa_stream * p,
		const void * data,
		size_t nbytes,
		pa_free_cb_t free_cb,
		int64_t offset,
		pa_seek_mode_t seek);
#endif
