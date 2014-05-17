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

#ifndef __XCAP_OSX_H__
#define __XCAP_OSX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "ga-common.h"

int ga_osx_init(struct gaImage *image);
void ga_osx_deinit();
int ga_osx_capture(char *buf, int buflen, struct gaRect *rect);

#ifdef __cplusplus
}
#endif

#endif
