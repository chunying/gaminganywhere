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
#include <map>

#include "ga-common.h"
#include "ga-conf.h"

#include "vconverter.h"

using namespace std;

static map<struct vconvcfg, struct SwsContext *> ga_converters;

bool operator<(struct vconvcfg a, struct vconvcfg b) {
	if(a.width < b.width)	return true;
	if(a.width > b.width)	return false;
	if(a.height < b.height)	return true;
	if(a.height > b.height)	return false;
	if(a.fmt < b.fmt)	return true;
	if(a.fmt > b.fmt)	return false;
	return false;
}

static struct SwsContext *
lookup_frame_converter_internal(struct vconvcfg *ccfg) {
	map<struct vconvcfg, struct SwsContext *>::iterator mi;
	//
	if((mi = ga_converters.find(*ccfg)) != ga_converters.end()) {
		return mi->second;
	}
	//
	return NULL;
}

struct SwsContext *
lookup_frame_converter(int w, int h, PixelFormat fmt) {
	struct vconvcfg ccfg;
	//
	ccfg.width = w;
	ccfg.height = h;
	ccfg.fmt = fmt;
	//
	return lookup_frame_converter_internal(&ccfg);
}

struct SwsContext *
create_frame_converter(int srcw, int srch, PixelFormat srcfmt,
		 int dstw, int dsth, PixelFormat dstfmt) {
	map<struct vconvcfg, struct SwsContext *>::iterator mi;
	struct vconvcfg ccfg;
	struct SwsContext *ctx;
	//
	ccfg.width = srcw;
	ccfg.height = srch;
	ccfg.fmt = srcfmt;
	//
	if((ctx = lookup_frame_converter_internal(&ccfg)) != NULL)
		return ctx;
	//
	if((ctx = ga_swscale_init(srcfmt, srcw, srch, dstw, dsth)) == NULL)
		return NULL;
	ga_converters[ccfg] = ctx;
	ga_error("Frame converter created: from (%d,%d)[%d] -> (%d,%d)[%d]\n",
		(int) srcw, (int) srch, (int) srcfmt,
		(int) dstw, (int) dsth, (int) dstfmt);
	//
	return ctx;
}

