/*
 * Copyright (c) 2013-2014 Chun-Ying Huang
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

#ifndef __XCAP_WIN32_D3D_H__
#define __XCAP_WIN32_D3D_H__

#include "ga-common.h"
#include "ga-avcodec.h"

int ga_win32_D3D_init(struct gaImage *image);
void ga_win32_D3D_deinit();
int ga_win32_D3D_capture(char *buf, int buflen, struct gaRect *grect);
//int ga_win32_D3D_capture_YUV(struct SwsContext *swsctx, char *buf, int buflen, int *linesize);

#endif
