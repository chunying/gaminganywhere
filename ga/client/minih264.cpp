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
#include "minih264.h"

static unsigned char *
dupbuf(unsigned char *buf, int len) {
	unsigned char *newbuf = (unsigned char *) malloc(len);
	if(newbuf == NULL)
		return NULL;
	bcopy(buf, newbuf, len);
	return newbuf;
}

struct bufinfo {
	unsigned int byteoff;
	unsigned char bitmask;
	unsigned char *buf;
	int buflen;
};

static void
h264_bufinit(struct bufinfo *bi, unsigned char *buf, int buflen) {
	bzero(bi, sizeof(struct bufinfo));
	bi->bitmask = 0x80;
	bi->buf = buf;
	bi->buflen = buflen;
	return;
}

static unsigned int
h264_buf_u(struct bufinfo *bi, int n) {
	unsigned int val = 0;
	//int oldn = n;
	while(n-- > 0) {
		val <<= 1;
		val |= ((bi->buf[bi->byteoff] & (bi->bitmask)) == 0 ? 0 : 1);
		if(bi->bitmask != 1) {
			bi->bitmask >>= 1;
		} else /* == 1 */{
			bi->byteoff++;
			bi->bitmask = 0x80;
			if(bi->byteoff >= bi->buflen) {
				// XXX: not handled
			}
		}
	}
	//fprintf(stderr, "u(%d) = %u\n", oldn, val);
	return val;
}

static unsigned int
h264_buf_ue(struct bufinfo *bi) {
	unsigned int val = 0;
	unsigned int b;
	int leading = -1;
	//fprintf(stderr, " IN: ue(v)\n");
	do {
		b = h264_buf_u(bi, 1);
		leading++;
	} while(b == 0);
	val = (1<<leading) - 1 + h264_buf_u(bi, leading);
	//fprintf(stderr, "OUT: ue(v) [%d] = %u\n", leading * 2 + 1, val);
	return val;
}

static int
h264_map_se(unsigned int code) {
	int sign = 1;
	if((code & 0x01) == 0)
		sign = -1;
	return sign * ((code+1)>>1);
}

static void
scaling_list(	struct bufinfo *bi,
		int *scalingList,
		int sizeOfScalingList,
		int useDefaultScalingMatrixFlag) {
	int j;
	int lastScale = 8;
	int nextScale = 8;
	for(j = 0; j < sizeOfScalingList; j++) {
		if(nextScale != 0) {
			int delta_scale = h264_map_se(h264_buf_ue(bi));//SE()
			nextScale = (lastScale + delta_scale + 256) & 0xff;
			useDefaultScalingMatrixFlag = (j==0 && nextScale==0);
		}
		scalingList[j] = (nextScale == 0) ? lastScale : nextScale;
		lastScale = scalingList[j];
	}
	return;
}

#define	U(n)	h264_buf_u(&bi, n)
#define	UE()	h264_buf_ue(&bi)
#define	SE()	h264_map_se(h264_buf_ue(&bi))

static int
parse_sps(struct mini_h264_context *ctx, unsigned char *buf, int len) {
	int i, src, rbsplen;
	//
	struct bufinfo bi;
	struct mini_h264_sps *sps = &ctx->sps;
	int scalingList4x4[6][16];
	int scalingList8x8[6][64];
	int useDefaultScalingMatrix4x4Flag[6];
	int useDefaultScalingMatrix8x8Flag[6];
	unsigned char *newbuf = dupbuf(buf, len);
	// copy buf, because we will modify its content
	if(newbuf == NULL)
		return -1;
	buf = newbuf;
	//
	for(src = 0, rbsplen = 0; src < len; src++) {
		if(src+2 < len && buf[src] == 0 && buf[src+1] == 0 && buf[src+2] == 3) {
			buf[rbsplen++] = buf[src++];
			buf[rbsplen++] = buf[src++];
		} else {
			buf[rbsplen++] = buf[src];
		}
	}
	//
	bzero(sps, sizeof(struct mini_h264_sps));
	//ga_log("h264: inlen=%d; rbsplen=%d\n", len, rbsplen);
	h264_bufinit(&bi, buf, rbsplen);
	sps->profile_idc = U(8);
	(void) U(8); /* skip constraint_set_0-5 flag + 2 zero bits */
	sps->level_idc = U(8);
	sps->spsid = UE();
	if(sps->profile_idc == 100
	|| sps->profile_idc == 110
	|| sps->profile_idc == 122
	|| sps->profile_idc == 244 
	|| sps->profile_idc == 44 
	|| sps->profile_idc == 83
	|| sps->profile_idc == 86
	|| sps->profile_idc == 118
	|| sps->profile_idc == 128
	|| sps->profile_idc == 138) {
		sps->chroma_format_idc = UE();
		if(sps->chroma_format_idc == 3)
			sps->separate_colour_plane_flag = U(1);
		sps->bit_depth_luma_minus8 = UE();
		sps->bit_depth_chroma_minus8 = UE();
		sps->qpprime_y_zero_transform_bypass_flag = U(1);
		sps->seq_scaling_matrix_present_flag = U(1);
		if(sps->seq_scaling_matrix_present_flag) {
			for(i = 0; i < ((sps->chroma_format_idc != 3) ? 8 : 12); i++) {
				sps->seq_scaling_list_present_flag[i] = U(1);
				if(sps->seq_scaling_list_present_flag[i]) {
					if(i < 6)
						scaling_list(&bi, scalingList4x4[i], 16, useDefaultScalingMatrix4x4Flag[i]);
					else
						scaling_list(&bi, scalingList8x8[i-6], 64, useDefaultScalingMatrix8x8Flag[i-6]);
				}
			}
		}
	}
	sps->log2_max_frame_num_minus4 = UE();
	sps->pic_order_cnt_type = UE();
	if(sps->pic_order_cnt_type == 0) {
		sps->log2_max_pic_order_cnt_lsb_minus4 = UE();
	} else if(sps->pic_order_cnt_type == 1) {
		sps->delta_pic_order_always_zero_flag = U(1);
		sps->offset_for_non_ref_pic = SE();
		sps->offset_for_top_to_bottom_field = SE();
		sps->num_ref_frames_in_pic_order_cnt_cycle = UE();
		for(unsigned i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; i++) {
			sps->offset_for_ref_frame[i] =SE();
		}
	}
	sps->max_num_ref_frames = UE();
	sps->gaps_in_frame_num_value_allowed_flag = U(1);
	sps->pic_width_in_mbs_minus1 = UE();
	sps->pic_height_in_map_units_minus1 = UE();
	sps->frame_mbs_only_flag = U(1);
	if(sps->frame_mbs_only_flag == 0)
		sps->mb_adaptive_frame_field_flag = U(1);
	sps->direct_8x8_inference_flag = U(1);
	sps->frame_cropping_flag = U(1);
	if(sps->frame_cropping_flag) {
		sps->frame_crop_left_offset = UE();
		sps->frame_crop_right_offset = UE();
		sps->frame_crop_top_offset = UE();
		sps->frame_crop_bottom_offset = UE();
	}
	sps->vui_parameters_present_flag = U(1);
	// XXX: skip vui_parameters
	ctx->width = ((sps->pic_width_in_mbs_minus1 + 1) * 16)
			- sps->frame_crop_right_offset * 2
			- sps->frame_crop_left_offset * 2;
	ctx->height = ((2 - sps->frame_mbs_only_flag)
			* (sps->pic_height_in_map_units_minus1 + 1) * 16)
			- (sps->frame_crop_top_offset * 2)
			- (sps->frame_crop_bottom_offset * 2);
	//
	free(buf);
	//
	return 0;
}

static void
parse_slice_layer_wo_partition(struct mini_h264_context *ctx, unsigned char *buf, int len) {
	struct bufinfo bi;
	//
	h264_bufinit(&bi, buf, len);
	/*first_mb_in_slice = */UE();
	ctx->slicetype = UE();
	switch(ctx->slicetype) {
	case 0: case 5:
		ctx->frametype = TYPE_P_FRAME;
		break;
	case 1: case 6:
		ctx->frametype = TYPE_B_FRAME;
		break;
	case 2: case 7:
		ctx->frametype = TYPE_I_FRAME;
		break;
	case 3: case 8:
		ctx->frametype = TYPE_SP_FRAME;
		break;
	case 4: case 9:
		ctx->frametype = TYPE_SI_FRAME;
		break;
	}
	return;
}

// assume: buf contains a single nal, except when nal_type = sps
// return 0 on success, >0 when two or more nals are found,  or -1 on fail
int
mini_h264_parse(struct mini_h264_context *ctx, unsigned char *buf, int len) {
	int ret = 0;
	//
	if(len < 5)
		return -1;
	if(buf[0] != 0x00
	|| buf[1] != 0x00
	|| buf[2] != 0x00
	|| buf[3] != 0x01)
		return -1;
	//
	if((buf[4] & 0x80) != 0x00)
		return -1;
	//
	bzero(ctx, sizeof(struct mini_h264_context));
	ctx->nri = (buf[4] & 0x60)>>5;
	ctx->type = buf[4] & 0x1f;
	// coded slide: ref PP.63 of ITU H.264 spec
	if(ctx->type == 1 || ctx->type == 5 || ctx->type == 19) {
		// coded slice for non-IDR/IDR picture
		parse_slice_layer_wo_partition(ctx, &buf[5], len-5);
	} else if(ctx->type > 1 && ctx->type < 5) {
		// coded partitioned slice A, B, C: do nothing
	} else if(ctx->type > 19 && ctx->type <= 21) {
		// coded slice ...
	} else if(ctx->type == 7) {
		// sps
		ret = 0;
		// find next start code to determine nal length
		for(unsigned char *s = &buf[4]; s < buf + len - 4; s++) {
			if(s[0]==0 && s[1]==0 && s[2]==0 && s[3]==1) {
				ret = s - buf;
				break;
			}
		}
		//
		ctx->is_config = 1;
		if((ctx->rawsps = dupbuf(buf, ret > 0 ? ret : len)) == NULL)
			return -1;
		ctx->spslen = ret > 0 ? ret : len;
		//
		if(parse_sps(ctx, ctx->rawsps + 5, ctx->spslen - 5) < 0) {
			free(ctx->rawsps);
			ctx->rawsps = NULL;
			ctx->spslen = 0;
			return -1;
		}
	} else if(ctx->type == 8) {
		// pps
		ret = 0;
		// find next start code to determine nal length
		for(unsigned char *s = &buf[4]; s < buf + len - 4; s++) {
			if(s[0]==0 && s[1]==0 && s[2]==0 && s[3]==1) {
				ret = s - buf;
				break;
			}
		}
		//
		ctx->is_config = 1;
		if((ctx->rawpps = dupbuf(buf, ret > 0 ? ret : len)) == NULL)
			return -1;
		ctx->ppslen = ret > 0 ? ret : len;
	} else {
		ctx->is_config = 1;
	}
	//
	return ret;
}

