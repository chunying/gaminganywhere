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

#ifndef __XCAP_WIN32_H__
#define __XCAP_WIN32_H__

#ifdef WIN32
/////////////////////////////////////////////////////////////////////////

#include <winsock2.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <process.h>	// _getpid
#include <winerror.h>	// FAILED() macro

#define	VK_PRETENDED_LCONTROL	0x98	// L_CONTROL key: 162
#define	VK_PRETENDED_LALT	0x99	// L_ALT key: 164

#ifdef USE_GA_WIN32_MACRO

// bzero
#ifndef bzero
#define	bzero(m,n)		ZeroMemory(m, n)
#endif
// bcopy
#ifndef bcopy
#define	bcopy(s,d,n)		CopyMemory(d, s, n)
#endif
// strncasecmp
#ifndef strncasecmp
#define	strncasecmp(a,b,n)	_strnicmp(a,b,n)
#endif
// strcasecmp
#ifndef strcasecmp
#define	strcasecmp(a,b)		_stricmp(a,b)
#endif
// strdup
#ifndef strdup
#define	strdup(s)		_strdup(s)
#endif
// strtok_r
#ifndef strtok_r
#define strtok_r(s,d,v)		strtok_s(s,d,v)
#endif
// snprintf
#ifndef snprintf
#define	snprintf(b,n,f,...)	_snprintf_s(b,n,_TRUNCATE,f,__VA_ARGS__)
#endif
// vsnprintf
#ifndef vsnprintf
#define	vsnprintf(b,n,f,ap)	vsnprintf_s(b,n,_TRUNCATE,f,ap)
#endif
// strncpy
#ifndef strncpy
#define	strncpy(d,s,n)		strncpy_s(d,n,s,_TRUNCATE)
#endif
// getpid
#ifndef getpid
#define	getpid			_getpid
#endif
// gmtimr_r
#ifndef gmtime_r
#define	gmtime_r(pt,ptm)	gmtime_s(ptm,pt)
#endif
// dlopen
#ifndef dlopen
#define	dlopen(f,opt)		LoadLibrary(f)
#endif
// dlsym
#ifndef dlsym
#define	dlsym(h,name)		GetProcAddress(h,name)
#endif
// dlclose
#ifndef dlclose
#define	dlclose(h)		FreeLibrary(h)
#endif

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif

struct WIN32IMAGE {
	int width;
	int height;
	int bytes_per_line;
};

EXPORT int gettimeofday(struct timeval *tv, void *tz);
EXPORT int usleep(long long waitTime);
EXPORT int read(SOCKET fd, void *buf, int count);
EXPORT int write(SOCKET fd, const void *buf, int count);
EXPORT int close(SOCKET fd);

EXPORT char *dlerror();

EXPORT void ga_win32_fill_bitmap_info(BITMAPINFO *pinfo, int w, int h, int bitsPerPixel);
EXPORT long long pcdiff_us(LARGE_INTEGER t1, LARGE_INTEGER t2, LARGE_INTEGER freq);

#endif	/* USE_GA_WIN32_MACRO */
/////////////////////////////////////////////////////////////////////////
#endif	/* WIN32 */

#endif
