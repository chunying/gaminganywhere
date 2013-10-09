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

#ifndef __XCAP_COMMON_H__
#define __XCAP_COMMON_H__

#ifndef WIN32
#include <sys/time.h>
#endif

#ifdef ANDROID
#include <android/log.h>
#endif

#if defined WIN32 && defined GA_LIB
#define	EXPORT __declspec(dllexport)
#elif defined WIN32 && ! defined GA_LIB
#define	EXPORT __declspec(dllimport)
#else
#define	EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#ifdef __cplusplus
}
#endif

#include "ga-win32.h"

#define	RGBA_SIZE	4	/* in bytes */

struct gaRect {
	int left, top;
	int right, bottom;
	int width, height;
	int linesize;
	int size;
};

struct gaImage {
	int width;
	int height;
	int bytes_per_line;
};

EXPORT long long tvdiff_us(struct timeval *tv1, struct timeval *tv2);
EXPORT long long ga_usleep(long long interval, struct timeval *ptv);
EXPORT int	ga_log(const char *fmt, ...);
EXPORT int	ga_error(const char *fmt, ...);
//	*ptr+*alignment = start at an aligned address with size s
EXPORT int	ga_malloc(int size, void **ptr, int *alignment);
EXPORT long	ga_gettid();
EXPORT void	ga_dump_codecs();
EXPORT int	ga_init(const char *config, const char *url);
EXPORT void	ga_deinit();
EXPORT void	ga_openlog();
EXPORT void	ga_closelog();
EXPORT long	ga_atoi(const char *str);
EXPORT struct gaRect * ga_fillrect(struct gaRect *rect, int left, int top, int right, int bottom);
EXPORT int	ga_crop_window(struct gaRect *rect, struct gaRect **prect);
EXPORT void	ga_backtrace();
EXPORT void	ga_dummyfunc();

EXPORT const char * ga_lookup_mime(const char *key);
EXPORT const char ** ga_lookup_ffmpeg_decoders(const char *key);
EXPORT enum AVCodecID ga_lookup_codec_id(const char *key);

#endif
