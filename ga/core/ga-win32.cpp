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


#include "ga-common.h"

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS) || defined(__WATCOMC__)
#define DELTA_EPOCH_IN_USEC	11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_USEC	11644473600000000ULL
#endif

typedef unsigned __int64 u_int64_t;

static u_int64_t
filetime_to_unix_epoch(const FILETIME *ft) {
	u_int64_t res = (u_int64_t) ft->dwHighDateTime << 32;
	res |= ft->dwLowDateTime;
	res /= 10;                   /* from 100 nano-sec periods to usec */
	res -= DELTA_EPOCH_IN_USEC;  /* from Win epoch to Unix epoch */
	return (res);
}

int
gettimeofday(struct timeval *tv, void *tz) {
	FILETIME  ft;
	u_int64_t tim;
	if (!tv) {
		//errno = EINVAL;
		return (-1);
	}
	GetSystemTimeAsFileTime(&ft);
	tim = filetime_to_unix_epoch (&ft);
	tv->tv_sec  = (long) (tim / 1000000L);
	tv->tv_usec = (long) (tim % 1000000L);
	return (0);
}

long long tvdiff_us(struct timeval *tv1, struct timeval *tv2);

int
usleep(long long waitTime) {
#if 0
	LARGE_INTEGER t1, t2, freq;
#else
	struct timeval t1, t2;
#endif
	long long ms, elapsed;
	if(waitTime <= 0)
		return 0;
#if 0
	QueryPerformanceCounter(&t1);
	QueryPerformanceFrequency(&freq);
	if(freq.QuadPart == 0) {
		// not supported
		Sleep(waitTime/1000);
		return 0;
	}
#else
	gettimeofday(&t1, NULL);
#endif
	// Sleep() may be fine
	ms = waitTime / 1000;
	waitTime %= 1000;
	if(ms > 0) {
		Sleep(ms);
	}
	// Sleep for the rest
	if(waitTime > 0) do {
#if 0
		QueryPerformanceCounter(&t2);

		elapsed = 1000000.0 * (t2.QuadPart - t1.QuadPart) / freq.QuadPart;
#else
		gettimeofday(&t2, NULL);
		elapsed = tvdiff_us(&t2, &t1);
#endif
	} while(elapsed < waitTime);
	//
	return 0;
}

int
read(SOCKET fd, void *buf, int count) {
	return recv(fd, (char *) buf, count, 0);
}

int
write(SOCKET fd, const void *buf, int count) {
	return send(fd, (const char*) buf, count, 0);
}

int
close(SOCKET fd) {
	return closesocket(fd);
}

char *
dlerror() {
	static char notsupported[] = "dlerror() on Windows is not supported.";
	return notsupported;
}

void
ga_win32_fill_bitmap_info(BITMAPINFO *pinfo, int w, int h, int bitsPerPixel) {
	ZeroMemory(pinfo, sizeof(BITMAPINFO));
	pinfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pinfo->bmiHeader.biBitCount = bitsPerPixel;
	pinfo->bmiHeader.biCompression = BI_RGB;
	pinfo->bmiHeader.biWidth = w;
	pinfo->bmiHeader.biHeight = h;
	pinfo->bmiHeader.biPlanes = 1; // must be 1
	pinfo->bmiHeader.biSizeImage = pinfo->bmiHeader.biHeight
					* pinfo->bmiHeader.biWidth
					* pinfo->bmiHeader.biBitCount/8;
	return;
}

long long	/* return microsecond */
pcdiff_us(LARGE_INTEGER t1, LARGE_INTEGER t2, LARGE_INTEGER freq) {
	return 1000000LL * (t1.QuadPart - t2.QuadPart) / freq.QuadPart;
}

