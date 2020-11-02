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
 * video frame converter: implementation
 */

#include <stdio.h>
#include <map>

#include "ga-common.h"
#include "ga-conf.h"

#include "vconverter.h"

using namespace std;

static map<struct vconvcfg, struct SwsContext *> ga_converters;

/**
 * Implement operator< for \a vconvcfg data structure.
 *
 * @param a [in] The left hand side operand.
 * @param b [in] The right hand side operand.
 * @return \a true if \a is smaller than \b,
 *	or \a false if \a is not smaller than \b.
 */
bool operator<(struct vconvcfg a, struct vconvcfg b) {
	if(a.src_width < b.src_width)	return true;
	if(a.src_width > b.src_width)	return false;
	if(a.src_height < b.src_height)	return true;
	if(a.src_height > b.src_height)	return false;
	if(a.src_fmt < b.src_fmt)	return true;
	if(a.src_fmt > b.src_fmt)	return false;
	if(a.dst_width < b.dst_width)	return true;
	if(a.dst_width > b.dst_width)	return false;
	if(a.dst_height < b.dst_height)	return true;
	if(a.dst_height > b.dst_height)	return false;
	if(a.dst_fmt < b.dst_fmt)	return true;
	if(a.dst_fmt > b.dst_fmt)	return false;
	/* all fields are equal */
	return false;
}

/**
 * Look up an existing converter. This is an internal function.
 *
 * @param ccfg [in] Pointer to a prepared \a vconvcfg data structure.
 * @return Pointer to the \a SwsContext structure of the converter,
 *	or NULL if not found.
 */
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

/**
 * Look up an existing converter.
 *
 * @param srcw [in] Video source frame width.
 * @param srch [in] Video source frame height.
 * @param srcfmt [in] Video source frame pixel format.
 * @param dstw [in] Video destination frame width.
 * @param dsth [in] Video destination frame height.
 * @param dstfmt [in] Video destination frame pixel format.
 * @return Pointer to the \a SwsContext structure of the converter,
 *	or NULL if not found.
 */
struct SwsContext *
lookup_frame_converter(int srcw, int srch, AVPixelFormat srcfmt, int dstw, int dsth, AVPixelFormat dstfmt) {
	struct vconvcfg ccfg;
	//
	ccfg.src_width = srcw;
	ccfg.src_height = srch;
	ccfg.src_fmt = srcfmt;
	ccfg.dst_width = dstw;
	ccfg.dst_height = dsth;
	ccfg.dst_fmt = dstfmt;
	//
	return lookup_frame_converter_internal(&ccfg);
}

/**
 * Create a video frame converter.
 *
 * @param srcw [in] Video source frame width.
 * @param srch [in] Video source frame height.
 * @param srcfmt [in] Video source frame pixel format.
 * @param dstw [in] Video destination frame width.
 * @param dsth [in] Video destination frame height.
 * @param dstfmt [in] Video destination frame pixel format.
 * @return Pointer to the \a SwsContext structure of the converter,
 *	or NULL if it fails on creating a converter.
 *
 * This function does not create duplicated converters.
 * An existing converter is returned if it has already been created.
 */
struct SwsContext *
create_frame_converter(int srcw, int srch, AVPixelFormat srcfmt,
		 int dstw, int dsth, AVPixelFormat dstfmt) {
	map<struct vconvcfg, struct SwsContext *>::iterator mi;
	struct vconvcfg ccfg;
	struct SwsContext *ctx;
	//
	ccfg.src_width = srcw;
	ccfg.src_height = srch;
	ccfg.src_fmt = srcfmt;
	ccfg.dst_width = dstw;
	ccfg.dst_height = dsth;
	ccfg.dst_fmt = dstfmt;
	//
	if((ctx = lookup_frame_converter_internal(&ccfg)) != NULL)
		return ctx;
	//
	if((ctx = sws_getContext(srcw, srch, srcfmt,
				 dstw, dsth, dstfmt,
				 SWS_BICUBIC, NULL, NULL, NULL)) == NULL) {
		return NULL;
	}
	ga_converters[ccfg] = ctx;
	ga_error("Frame converter created: from (%d,%d)[%d] -> (%d,%d)[%d]\n",
		(int) srcw, (int) srch, (int) srcfmt,
		(int) dstw, (int) dsth, (int) dstfmt);
	//
	return ctx;
}

