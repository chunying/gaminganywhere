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

/**
 * @file
 * header: common GA functions and macros
 */

#ifndef __GA_COMMON_H__
#define __GA_COMMON_H__

#ifndef WIN32
#include <sys/time.h>
#endif

#ifdef ANDROID
#include <android/log.h>
#endif

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if defined WIN32 && defined GA_LIB
/** Functions exported from DLL's */
#define	EXPORT __declspec(dllexport)
#elif defined WIN32 && ! defined GA_LIB
/** Functions imported from DLL's */
#define	EXPORT __declspec(dllimport)
#else
/** Not used in UNIX-like systems, but required for compatible with WIN32 libraries */
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

/** Enable audio subsystem? */
#define	ENABLE_AUDIO

/** Unit size size for RGBA pixels, in bytes */
#define	RGBA_SIZE	4

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
EXPORT int	ga_malloc(int size, void **ptr, int *alignment);
EXPORT int	ga_alignment(void *ptr, int alignto);
EXPORT long	ga_gettid();
EXPORT void	ga_dump_codecs();
EXPORT int	ga_init(const char *config, const char *url);
EXPORT void	ga_deinit();
EXPORT void	ga_openlog();
EXPORT void	ga_closelog();
// save file feature
EXPORT FILE *	ga_save_init(const char *filename);
EXPORT FILE *	ga_save_init_txt(const char *filename);
EXPORT int	ga_save_data(FILE *fp, unsigned char *buffer, int size);
EXPORT int	ga_save_printf(FILE *fp, const char *fmt, ...);
EXPORT int	ga_save_yuv420p(FILE *fp, int w, int h, unsigned char *planes[], int linesize[]);
EXPORT int	ga_save_rgb4(FILE *fp, int w, int h, unsigned char *planes, int linesize);
EXPORT int	ga_save_close(FILE *fp);
// aggregated output feature
EXPORT void	ga_aggregated_reset();
EXPORT void	ga_aggregated_print(int key, int limit, int value);
// encoders or decoders would require this
EXPORT unsigned char * ga_find_startcode(unsigned char *buf, unsigned char *end, int *startcode_len);
//
EXPORT long	ga_atoi(const char *str);
EXPORT struct gaRect * ga_fillrect(struct gaRect *rect, int left, int top, int right, int bottom);
EXPORT int	ga_crop_window(struct gaRect *rect, struct gaRect **prect);
EXPORT void	ga_backtrace();
EXPORT void	ga_dummyfunc();

EXPORT const char * ga_lookup_mime(const char *key);
EXPORT const char ** ga_lookup_ffmpeg_decoders(const char *key);
EXPORT enum AVCodecID ga_lookup_codec_id(const char *key);

EXPORT void	pthread_cancel_init();
#ifdef ANDROID
#include <pthread.h>
EXPORT int	pthread_cancel(pthread_t thread);
#endif

#endif
