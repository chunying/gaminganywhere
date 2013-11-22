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
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#ifndef WIN32
#ifndef ANDROID
#include <execinfo.h>
#endif /* !ANDROID */
#include <unistd.h>
#include <sys/time.h>
#ifndef GA_EMCC
#include <sys/syscall.h>
#endif /* GA_EMCC */
#endif /* !WIN32 */
#ifdef ANDROID
#include <android/log.h>
#endif /* ANDROID */

#if !defined(WIN32) && !defined(__APPLE__) && !defined(ANDROID)
#include <X11/Xlib.h>
#endif

#include "ga-common.h"
#include "ga-conf.h"
#ifndef ANDROID_NO_FFMPEG
#include "ga-avcodec.h"
#endif
#include "rtspconf.h"

#ifndef NIPQUAD
#define NIPQUAD(x)	((unsigned char*)&(x))[0],	\
			((unsigned char*)&(x))[1],	\
			((unsigned char*)&(x))[2],	\
			((unsigned char*)&(x))[3]
#endif

static char *ga_logfile = NULL;

long long
tvdiff_us(struct timeval *tv1, struct timeval *tv2) {
	struct timeval delta;
	delta.tv_sec = tv1->tv_sec - tv2->tv_sec;
	delta.tv_usec = tv1->tv_usec - tv2->tv_usec;
	if(delta.tv_usec < 0) {
		delta.tv_sec--;
		delta.tv_usec += 1000000;
	}
	return 1000000LL*delta.tv_sec + delta.tv_usec;
}

long long
ga_usleep(long long interval, struct timeval *ptv) {
	long long delta;
	struct timeval tv;
	if(ptv != NULL) {
		gettimeofday(&tv, NULL);
		delta = tvdiff_us(&tv, ptv);
		if(delta >= interval) {
			usleep(1);
			return -1;
		}
		interval -= delta;
	}
	usleep(interval);
	return 0LL;
}

static void
ga_writelog(struct timeval tv, const char *s) {
	FILE *fp;
	if(ga_logfile == NULL)
		return;
	if((fp = fopen(ga_logfile, "at")) != NULL) {
		fprintf(fp, "[%d] %ld.%06ld %s", getpid(), tv.tv_sec, tv.tv_usec, s);
		fclose(fp);
	}
	return;
}

int
ga_log(const char *fmt, ...) {
	char msg[4096];
	struct timeval tv;
	va_list ap;
	//
	gettimeofday(&tv, NULL);
	va_start(ap, fmt);
#ifdef ANDROID
	__android_log_vprint(ANDROID_LOG_INFO, "ga_log.native", fmt, ap);
#endif
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	//
	ga_writelog(tv, msg);
	//
	return 0;
}

int
ga_error(const char *fmt, ...) {
	char msg[4096];
	struct timeval tv;
	va_list ap;
	gettimeofday(&tv, NULL);
	va_start(ap, fmt);
#ifdef ANDROID
	__android_log_vprint(ANDROID_LOG_INFO, "ga_log.native", fmt, ap);
#endif
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	fprintf(stderr, "# [%d] %ld.%06ld %s", getpid(), tv.tv_sec, tv.tv_usec, msg);
	//
	ga_writelog(tv, msg);
	//
	return -1;
}

int
ga_malloc(int size, void **ptr, int *alignment) {
	if((*ptr = malloc(size+16)) == NULL)
		return -1;
#ifdef __x86_64__
	*alignment = 16 - (((long long) *ptr)&0x0f);
#else
	*alignment = 16 - (((unsigned) *ptr)&0x0f);
#endif
	return 0;
}

long
ga_gettid() {
#ifdef WIN32
	return GetCurrentThreadId();
#elif defined __APPLE__
	return pthread_mach_thread_np(pthread_self());
#elif defined ANDROID
	return gettid();
#elif defined GA_EMCC
	return pthread_self();
#else
	return (pid_t) syscall(SYS_gettid);
#endif
}

static int
winsock_init() {
#ifdef WIN32
	WSADATA wd;
	if(WSAStartup(MAKEWORD(2,2), &wd) != 0)
		return -1;
#endif
	return 0;
}

void
ga_dump_codecs() {
	int n, count;
	char buf[8192], *ptr;
	AVCodec *c = NULL;
	n = snprintf(buf, sizeof(buf), "Registered codecs: ");
	ptr = &buf[n];
	count = 0;
	for(c = av_codec_next(NULL); c != NULL; c = av_codec_next(c)) {
		n = snprintf(ptr, sizeof(buf)-(ptr-buf), "%s ",
				c->name);
		ptr += n;
		count++;
	}
	snprintf(ptr, sizeof(buf)-(ptr-buf), "(%d)\n", count);
	ga_error(buf);
	return;
}

int
ga_init(const char *config, const char *url) {
	srand(time(0));
	winsock_init();
#ifndef ANDROID_NO_FFMPEG
	av_register_all();
	avcodec_register_all();
	avformat_network_init();
	//ga_dump_codecs();
#endif
	if(config != NULL) {
		if(ga_conf_load(config) < 0) {
			ga_error("GA: cannot load configuration file '%s'\n", config);
			return -1;
		}
	}
	if(url != NULL) {
		if(ga_url_parse(url) < 0) {
			ga_error("GA: invalid URL '%s'\n", url);
			return -1;
		}
	}
	return 0;
}

void
ga_deinit() {
	return;
}

void
ga_openlog() {
	char fn[1024];
	FILE *fp;
	//
	if(ga_conf_readv("logfile", fn, sizeof(fn)) == NULL)
		return;
	if((fp = fopen(fn, "at")) != NULL) {
		fclose(fp);
		ga_logfile = strdup(fn);
	}
	//
	return;
}

void
ga_closelog() {
	if(ga_logfile != NULL) {
		free(ga_logfile);
		ga_logfile = NULL;
	}
	return;
}

long
ga_atoi(const char *str) {
	// XXX: not sure why sometimes windows strtol failed on
	// handling read-only constant strings ...
	char buf[64];
	long val;
	strncpy(buf, str, sizeof(buf));
	val = strtol(buf, NULL, 0);
	return val;
}

struct gaRect *
ga_fillrect(struct gaRect *rect, int left, int top, int right, int bottom) {
	if(rect == NULL)
		return NULL;
#define SWAP(a,b)	do { int tmp = a; a = b; b = tmp; } while(0);
	if(left > right)
		SWAP(left, right);
	if(top > bottom)
		SWAP(top, bottom);
#undef	SWAP
	rect->left = left;
	rect->top = top;
	rect->right = right;
	rect->bottom = bottom;
	//
	rect->width = rect->right - rect->left + 1;
	rect->height = rect->bottom - rect->top + 1;
	rect->linesize = rect->width * RGBA_SIZE;
	rect->size = rect->width * rect->height * RGBA_SIZE;
	//
	if(rect->width <= 0 || rect->height <= 0) {
		ga_error("# invalid rect size (%dx%d)\n", rect->width, rect->height);
		return NULL;
	}
	//
	return rect;
}

#ifdef WIN32
int
ga_crop_window(struct gaRect *rect, struct gaRect **prect) {
	char wndname[1024], wndclass[1024];
	char *pname;
	char *pclass;
	int dw, dh, find_wnd_arg = 0;
	HWND hWnd;
	RECT client;
	POINT lt, rb;
	//
	if(rect == NULL || prect == NULL)
		return -1;
	//
	pname = ga_conf_readv("find-window-name", wndname, sizeof(wndname));
	pclass = ga_conf_readv("find-window-class", wndclass, sizeof(wndclass));
	//
	if(pname != NULL && *pname != '\0')
		find_wnd_arg++;
	if(pclass != NULL && *pclass != '\0')
		find_wnd_arg++;
	if(find_wnd_arg <= 0) {
		*prect = NULL;
		return 0;
	}
	//
	if((hWnd = FindWindow(pclass, pname)) == NULL) {
		ga_error("FindWindow failed for '%s/%s'\n",
			pclass ? pclass : "",
			pname ? pname : "");
		return -1;
	}
	//
	GetWindowText(hWnd, wndname, sizeof(wndname));
	dw = GetSystemMetrics(SM_CXSCREEN);
	dh = GetSystemMetrics(SM_CYSCREEN);
	//
	ga_error("Found window (0x%08x) :%s%s%s%s\n", hWnd,
		pclass ? " class=" : "",
		pclass ? pclass : "",
		pname ? " name=" : "",
		pname ? pname : "");
	//
	if(SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE|SWP_SHOWWINDOW) == 0) {
		ga_error("SetWindowPos failed.\n");
		return -1;
	}
	if(GetClientRect(hWnd, &client) == 0) {
		ga_error("GetClientRect failed.\n");
		return -1;
	}
	if(SetForegroundWindow(hWnd) == 0) {
		ga_error("SetForegroundWindow failed.\n");
	}
	//
	lt.x = client.left;
	lt.y = client.top;
	rb.x = client.right-1;
	rb.y = client.bottom-1;
	//
	if(ClientToScreen(hWnd, &lt) == 0
	|| ClientToScreen(hWnd, &rb) == 0) {
		ga_error("Map from client coordinate to screen coordinate failed.\n");
		return -1;
	}
	//
	rect->left = lt.x;
	rect->top = lt.y;
	rect->right = rb.x;
	rect->bottom = rb.y;
	// size check: multiples of 2?
	if((rect->right - rect->left + 1) % 2 != 0)
		rect->left--;
	if((rect->bottom - rect->top + 1) % 2 != 0)
		rect->top--;
	//
	if(rect->left < 0 || rect->top < 0 || rect->right >= dw || rect->bottom >= dh) {
		ga_error("Invalid window: (%d,%d)-(%d,%d) w=%d h=%d (screen dimension = %dx%d).\n",
			rect->left, rect->top, rect->right, rect->bottom,
			rect->right - rect->left + 1,
			rect->bottom - rect->top + 1);
		return -1;
	}
	//
	*prect = rect;
	return 1;
}
#elif defined(__APPLE__) || defined(ANDROID)
int
ga_crop_window(struct gaRect *rect, struct gaRect **prect) {
	// XXX: implement find window for Apple
	*prect = NULL;
	return 0;
}
#else /* X11 */
Window
FindWindowX(Display *dpy, Window top, const char *name) {
	Window *children, dummy;
	unsigned int i, nchildren;
	Window w = 0;
	char *window_name;

	if(XFetchName(dpy, top, &window_name) && !strcmp(window_name, name)) {
		return(top);
	}

	if(!XQueryTree(dpy, top, &dummy, &dummy, &children, &nchildren))
		return(0);

	for(i = 0; i < nchildren; i++) {
		w = FindWindowX(dpy, children[i], name);
		if (w) {
			break;
		}
	}

	if (children)
		XFree ((char *)children);

	return(w);
}

int
GetClientRectX(Display *dpy, int screen, Window window, struct gaRect *rect) {
	XWindowAttributes win_attributes;
	int rx, ry;
	Window tmpwin;
	//
	if(rect == NULL)
		return -1;
	if(!XGetWindowAttributes(dpy, window, &win_attributes))
		return -1;
	XTranslateCoordinates(dpy, window, win_attributes.root,
		-win_attributes.border_width,
		-win_attributes.border_width,
		&rx, &ry, &tmpwin);
	rect->left = rx;
	rect->top = ry;
	rect->right = rx + win_attributes.width - 1;
	rect->bottom = ry + win_attributes.height - 1;
	return 0;
}

int
ga_crop_window(struct gaRect *rect, struct gaRect **prect) {
	char display[16], wndname[1024];
	char *pdisplay, *pname;
	Display *d = NULL;
	int dw, dh, screen = 0;
	Window w = 0;
	//
	if(rect == NULL || prect == NULL)
		return -1;
	//
	if((pdisplay = ga_conf_readv("display", display, sizeof(display))) == NULL) {
		*prect = NULL;
		return 0;
	}
	if((pname = ga_conf_readv("find-window-name", wndname, sizeof(wndname))) == NULL) {
		*prect = NULL;
		return 0;
	}
	//
	if((d = XOpenDisplay(pdisplay)) == NULL) {
		ga_error("ga_crop_window: cannot open display %s\n", display);
		return -1;
	}
	screen = XDefaultScreen(d);
	dw = DisplayWidth(d, screen);
	dh = DisplayHeight(d, screen);
	if((w = FindWindowX(d, RootWindow(d, screen), pname)) == 0) {
		ga_error("FindWindowX failed for %s/%s\n", pdisplay, pname);
		XCloseDisplay(d);
		return -1;
	}
	if(GetClientRectX(d, screen, w, rect) < 0) {
		ga_error("GetClientRectX failed for %s/%s\n", pdisplay, pname);
		XCloseDisplay(d);
		return -1;
	}
	XRaiseWindow(d, w);
	XSetInputFocus(d, w, RevertToNone, CurrentTime);
	XCloseDisplay(d);
	// size check: multiples of 2?
	if((rect->right - rect->left + 1) % 2 != 0)
		rect->left--;
	if((rect->bottom - rect->top + 1) % 2 != 0)
		rect->top--;
	// window is all visible?
	if(rect->left < 0 || rect->top < 0 || rect->right >= dw || rect->bottom >= dh) {
		ga_error("Invalid window: (%d,%d)-(%d,%d) w=%d h=%d (screen dimension = %dx%d).\n",
			rect->left, rect->top, rect->right, rect->bottom,
			rect->right - rect->left + 1,
			rect->bottom - rect->top + 1,
			dw, dh);
		return -1;
	}
	//
	*prect = rect;
	return 1;
}
#endif

void
ga_backtrace() {
#if defined(WIN32) || defined(ANDROID)
	return;
#else
	int j, nptrs;
#define SIZE 100
	void *buffer[SIZE];
	char **strings;

	nptrs = backtrace(buffer, SIZE);
	printf("-- backtrace() returned %d addresses -----------\n", nptrs);

	strings = backtrace_symbols(buffer, nptrs);
	if (strings == NULL) {
		perror("backtrace_symbols");
		exit(-1);
	}

	for (j = 0; j < nptrs; j++)
		printf("%s\n", strings[j]);

	free(strings);
	printf("------------------------------------------------\n");
#endif	/* WIN32 */
}

void
ga_dummyfunc() {
#ifndef ANDROID_NO_FFMPEG
	// place some required functions - for link purpose
	swr_alloc_set_opts(NULL, 0, (AVSampleFormat) 0, 0, 0, (AVSampleFormat) 0, 0, 0, NULL);
#endif
	return;
}

struct ga_codec_entry {
	const char *key;
	enum AVCodecID id;
	const char *mime;
	const char *ffmpeg_decoders[4];
};

struct ga_codec_entry ga_codec_table[] = {
	{ "H264", AV_CODEC_ID_H264, "video/avc", { "h264", NULL } },
	{ "VP8", AV_CODEC_ID_VP8, "video/x-vnd.on2.vp8", { "libvpx", NULL } },
	{ "MPA", AV_CODEC_ID_MP3, "audio/mpeg", { "mp3", NULL } },
	{ "OPUS", AV_CODEC_ID_OPUS, "audio/opus", { "libopus", NULL } },
	{ NULL, AV_CODEC_ID_NONE, NULL, { NULL } } /* END */
};

static ga_codec_entry *
ga_lookup_core(const char *key) {
	int i = 0;
	while(i >= 0 && ga_codec_table[i].key != NULL) {
		if(strcasecmp(ga_codec_table[i].key, key) == 0)
			return &ga_codec_table[i];
		i++;
	}
	return NULL;
}

const char *
ga_lookup_mime(const char *key) {
	struct ga_codec_entry * e = ga_lookup_core(key);
	if(e==NULL || e->mime==NULL) {
		ga_error("ga_lookup[%s]: mime not found\n", key);
		return NULL;
	}
	return e->mime;
}

const char **
ga_lookup_ffmpeg_decoders(const char *key) {
	struct ga_codec_entry * e = ga_lookup_core(key);
	if(e==NULL || e->ffmpeg_decoders==NULL) {
		ga_error("ga_lookup[%s]: ffmpeg decoders not found\n", key);
		return NULL;
	}
	return e->ffmpeg_decoders;
}

enum AVCodecID
ga_lookup_codec_id(const char *key) {
	struct ga_codec_entry * e = ga_lookup_core(key);
	if(e==NULL) {
		ga_error("ga_lookup[%s]: codec id not found\n", key);
		return AV_CODEC_ID_NONE;
	}
	return e->id;
}

