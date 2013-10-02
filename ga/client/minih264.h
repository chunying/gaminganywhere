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

#ifndef __MINIH264_H__
#define __MINIH264_H__

#ifdef __cplusplus
extern "C" {
#endif

struct mini_h264_sps {
	unsigned char profile_idc;
	unsigned char constraint_set_flag[5];
	unsigned char reserved_zero_2bits;
	unsigned char level_idc;
	unsigned int spsid;
	unsigned int chroma_format_idc;
	unsigned char separate_colour_plane_flag;
	unsigned int bit_depth_luma_minus8;
	unsigned int bit_depth_chroma_minus8;
	unsigned char qpprime_y_zero_transform_bypass_flag;
	unsigned char seq_scaling_matrix_present_flag;
	unsigned char seq_scaling_list_present_flag[12];
	// XXX: scaling list
	unsigned int log2_max_frame_num_minus4;
	unsigned int pic_order_cnt_type;
	unsigned int log2_max_pic_order_cnt_lsb_minus4;
	unsigned char delta_pic_order_always_zero_flag;
	int offset_for_non_ref_pic;
	int offset_for_top_to_bottom_field;
	unsigned int num_ref_frames_in_pic_order_cnt_cycle;
	int offset_for_ref_frame[64];	// XXX: what's the max value?
	unsigned int max_num_ref_frames;
	unsigned char gaps_in_frame_num_value_allowed_flag;
	unsigned int pic_width_in_mbs_minus1;
	unsigned int pic_height_in_map_units_minus1;
	unsigned char frame_mbs_only_flag;
	unsigned char mb_adaptive_frame_field_flag;
	unsigned char direct_8x8_inference_flag;
	unsigned char frame_cropping_flag;
	unsigned int frame_crop_left_offset;
	unsigned int frame_crop_right_offset;
	unsigned int frame_crop_top_offset;
	unsigned int frame_crop_bottom_offset;
	unsigned char vui_parameters_present_flag;
	// XXX: no vui parameters
};

#define	TYPE_I_FRAME	1	// 2, 7
#define	TYPE_SI_FRAME	2	// 4, 9
#define	TYPE_B_FRAME	3	// 1, 6
#define	TYPE_P_FRAME	4	// 0, 5
#define	TYPE_SP_FRAME	5	// 3, 8

struct mini_h264_context {
	unsigned short nri;
	unsigned short type;
	unsigned short slicetype;
	unsigned short frametype;
	struct mini_h264_sps sps;
	unsigned char *rawsps;
	unsigned int spslen;
	unsigned char *rawpps;
	unsigned int ppslen;
	int width;
	int height;
	int is_config;
};

int mini_h264_parse(struct mini_h264_context *ctx, unsigned char *buf, int len);

#ifdef __cplusplus
}
#endif

#endif
