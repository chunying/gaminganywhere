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
#ifndef WIN32
#include <strings.h>
#endif

#include "ga-common.h"
#include "minivp8.h"

unsigned char *
parse_descriptor(struct mini_vp8_context *ctx, unsigned char *ptr) {
	// parse descriptor (1-5 bytes)
	ctx->extended = (*ptr) & 0x80;
	ctx->non_reference = (*ptr) & 0x20;
	ctx->start = (*ptr) & 0x10;
	ctx->pid = (*ptr) & 0x07;
	ptr++;
	//
	if(ctx->extended != 0) {
		ctx->has_picid = (*ptr) & 0x80;
		ctx->has_TL0PICIDX = (*ptr) & 0x40;
		ctx->has_tid = (*ptr) & 0x20;
		ctx->has_keyidx = (*ptr) & 0x10;
		ptr++;
	}
	if(ctx->has_picid)
		ptr++;
	if(ctx->has_TL0PICIDX)
		ptr++;
	if(ctx->has_tid || ctx->has_keyidx)
		ptr++;
	return ptr;
}

unsigned char *
parse_payload_header(struct mini_vp8_context *ctx, unsigned char *ptr) {
	// parse payload header, 3 bytes
	unsigned int size0, size1, size2;
	ctx->show_frame = (*ptr) & 0x10;
	ctx->ver = ((*ptr) & 0x0e)>>1;
	ctx->is_keyframe = ((*ptr) & 0x01)^0x01;
	//
	size0 = ((*ptr) & 0xe0)>>5;
	ptr++;
	//
	size1 = *ptr++;
	size2 = *ptr++;
	ctx->size = ((unsigned int) size0) + 8U * size1 + 2048U * size2;
	//
	return ptr;
}

// assume: buf contains a single nal
// return 0 on success, or -1 on fail
int
mini_vp8_parse(struct mini_vp8_context *ctx, unsigned char *buf, int len) {
	//
	unsigned char *ptr = buf;
	//
	bzero(ctx, sizeof(struct mini_vp8_context));
	ptr = parse_descriptor(ctx, buf);
	ptr = parse_payload_header(ctx, buf);
	if(ctx->is_keyframe) {
		unsigned char local_key;
		unsigned char local_ver;
		unsigned char local_show;
		local_key = (*ptr) & 0x80;
		local_ver = ((*ptr) & 0x70)>>4;
		local_show = (*ptr) & 0x08;
		ptr += 3;
		// local_key should be 1 as well
		ctx->width = (*((unsigned short*) ptr)) & 0x3fff;
		ptr += 2;
		ctx->height= (*((unsigned short*) ptr)) & 0x3fff;
		ptr += 2;
	}
	return 0;
}

