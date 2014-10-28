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

#ifndef __VPU_COMMON_H__
#define __VPU_COMMON_H__

int vpu_encoder_init(int width, int height, int fps_n, int fps_d, int bitrate, int gopsize);
int vpu_encoder_deinit();
const unsigned char * vpu_encoder_get_h264_sps(int *size);
const unsigned char * vpu_encoder_get_h264_pps(int *size);
unsigned char * vpu_encoder_encode(unsigned char *frame, int framesize, int *encsize);

#endif /* __VPU_COMMON_H__ */

