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
 * GamingAnywhere's common functions for Windows 
 *
 * This includes Windows specific functions and
 * common UNIX function implementations for Windows.
 */

#include "ga-common.h"

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS) || defined(__WATCOMC__)
#define DELTA_EPOCH_IN_USEC	11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_USEC	11644473600000000ULL
#endif

typedef unsigned __int64 u_int64_t;

/**
 * Convert Windows FILETIME to UNIX timestamp. This is an internal function.
 *
 * @param ft [in] Pointer to a FILETIME.
 * @return UNIX timestamp time in microsecond unit.
 */
static u_int64_t
filetime_to_unix_epoch(const FILETIME *ft) {
	u_int64_t res = (u_int64_t) ft->dwHighDateTime << 32;
	res |= ft->dwLowDateTime;
	res /= 10;                   /* from 100 nano-sec periods to usec */
	res -= DELTA_EPOCH_IN_USEC;  /* from Win epoch to Unix epoch */
	return (res);
}

/**
 * gettimeofday() implementation
 *
 * @param tv [in] \a timeval to store the timestamp.
 * @param tz [in] timezone: unused.
 */
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

/**
 * usleep() function: sleep in microsecond scale.
 *
 * @param waitTime [in] time to sleep (in microseconds).
 * @return Always return 0.
 */
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

/**
 * read() function to read from a socket.
 *
 * @param fd [in] The SOCKET identifier.
 * @param buf [in] Buffer to receive data.
 * @param count [in] Size limit of the \a buf.
 * @return Number of bytes received, see MSDN recv() function.
 */
int
read(SOCKET fd, void *buf, int count) {
	return recv(fd, (char *) buf, count, 0);
}

/**
 * write() function to write to a socket.
 *
 * @param fd [in] The SOCKET identifier.
 * @param buf [in] Buffer to be sent.
 * @param count [in] Number of bytes in the \a buf.
 * @return Number of bytes sent, see MSDN send() function.
 */
int
write(SOCKET fd, const void *buf, int count) {
	return send(fd, (const char*) buf, count, 0);
}

/**
 * close() function to close a socket.
 *
 * @param fd [in] The SOCKET identifier.
 * @return Zero on success, otherwise see MSDN closesocket() function.
 */
int
close(SOCKET fd) {
	return closesocket(fd);
}

/**
 * dlerror() to report error message of dl* functions
 *
 * Not supported on Windows.
 */
char *
dlerror() {
	static char notsupported[] = "dlerror() on Windows is not supported.";
	return notsupported;
}

/**
 * Fill BITMAPINFO data structure
 *
 * @param pinfo [in,out] The BITMAPINFO structure to be filled.
 * @param w [in] The width of the bitmap image.
 * @param h [in] The height of the bitmap image.
 * @param bitsPerPixel [in] The bits-per-pixel of the bitmap image.
 */
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

/**
 * Compute time differece based on Windows performance counter.
 *
 * @param t1 [in] The first counter.
 * @param t2 [in] The second counter.
 * @param freq [in] The performance frequency obtained by \em QueryPerformanceFrequency().
 * @return Time differece in microsecond unit, e.g., \a t1 - \a t2.
 */
long long
pcdiff_us(LARGE_INTEGER t1, LARGE_INTEGER t2, LARGE_INTEGER freq) {
	return 1000000LL * (t1.QuadPart - t2.QuadPart) / freq.QuadPart;
}

typedef enum GA_PROCESS_DPI_AWARENESS {
	GA_PROCESS_DPI_UNAWARE = 0,
	GA_PROCESS_SYSTEM_DPI_AWARE = 1,
	GA_PROCESS_PER_MONITOR_DPI_AWARE = 2
} GA_PROCESS_DPI_AWARENESS;
typedef BOOL (WINAPI * setProcessDpiAware_t)(void);
typedef HRESULT (WINAPI * setProcessDpiAwareness_t)(GA_PROCESS_DPI_AWARENESS);

/**
 * Platform dependent call to SetProcessDpiAware(PROCESS_PER_MONITOR_DPI_AWARE)
 */
EXPORT
int
ga_set_process_dpi_aware() {
	HMODULE shcore, user32;
	setProcessDpiAware_t     aw = NULL;
	setProcessDpiAwareness_t awness = NULL;
	int ret = 0;
	if((shcore = LoadLibraryA("shcore.dll")))
		awness = (setProcessDpiAwareness_t) GetProcAddress(shcore, "SetProcessDpiAwareness");
	if((user32 = LoadLibraryA("user32.dll")))
		aw = (setProcessDpiAware_t) GetProcAddress(user32, "SetProcessDPIAware");
	if(awness) {
		ret = (int) (awness(GA_PROCESS_PER_MONITOR_DPI_AWARE) == S_OK);
	} else if(aw) {
		ret = (int) (aw() != 0);
	}
	if(user32) FreeLibrary(user32);
	if(shcore) FreeLibrary(shcore);
	return ret;
}


