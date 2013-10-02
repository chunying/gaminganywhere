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

#ifndef __MINIVP8_H__
#define __MINIVP8_H__

#ifdef __cplusplus
extern "C" {
#endif

#define	TYPE_I_FRAME	1	// 2, 7
#define	TYPE_SI_FRAME	2	// 4, 9
#define	TYPE_B_FRAME	3	// 1, 6
#define	TYPE_P_FRAME	4	// 0, 5
#define	TYPE_SP_FRAME	5	// 3, 8

struct mini_vp8_context {
	// descriptor
	unsigned char extended;
	unsigned char non_reference;
	unsigned char start;
	unsigned char reserv1;
	unsigned short pid;
	unsigned char has_picid;
	unsigned char has_TL0PICIDX;
	unsigned char has_tid;
	unsigned char has_keyidx;
	// payload header
	unsigned char show_frame;
	unsigned char is_keyframe;
	unsigned char ver;
	unsigned char reserv2;
	unsigned int size;
	//unsigned short frametype;
	int width;
	int height;
};

int mini_vp8_parse(struct mini_vp8_context *ctx, unsigned char *buf, int len);

#ifdef __cplusplus
}
#endif

#endif
