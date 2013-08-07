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

#ifndef __VCONVERTER_H__
#define __VCONVERTER_H__

#include "ga-avcodec.h"

struct vconvcfg {
	int width;
	int height;
	PixelFormat fmt;
};

EXPORT struct SwsContext * lookup_frame_converter(int w, int h, PixelFormat fmt);
EXPORT struct SwsContext * create_frame_converter(
		int srcw, int srch, PixelFormat srcfmt,
		int dstw, int dsth, PixelFormat dstfmt);

#endif
