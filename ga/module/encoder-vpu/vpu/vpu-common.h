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

#ifdef __cplusplus
extern "C" {
#endif
#include <vpu_lib.h>
#include <vpu_io.h>
#ifdef __cplusplus
}
#endif

#define	VPU_MEM_ALIGN			(8)
#define	VPU_ENC_MAX_NUM_MEM_REQS	(40)
#define	VPU_ENC_MAX_FRAME		(10)
#define	VPU_ENC_BITSTREAM_SIZE		(3072 * 1024)	// 3MB
#define VPU_ENC_MPEG4_SCRATCH_SIZE	0x080000

#define	vpu_align(ptr, align)		(((unsigned long) ptr+(align)-1)/(align)*(align))

typedef struct PhyMemInfo {
	// phy mem info
	int nPhyNum;
	unsigned long phyMem_virtAddr[VPU_ENC_MAX_NUM_MEM_REQS];
	unsigned long phyMem_phyAddr[VPU_ENC_MAX_NUM_MEM_REQS];
	unsigned long phyMem_cpuAddr[VPU_ENC_MAX_NUM_MEM_REQS];
	unsigned long phyMem_size[VPU_ENC_MAX_NUM_MEM_REQS];	
}	PhyMemInfo;

typedef struct vpu_context_s {
	int		vpu_initialized;
	int		vpu_width;		// real pic dimension
	int		vpu_height;
	int		vpu_framesize;
	int		vpu_phy_width;		// phymem alloc dimension
	int		vpu_phy_height;
	int		vpu_phy_framesize;
	//
	EncHandle	vpu_handle;
	EncOpenParam	vpu_encOP;
	EncInitialInfo	vpu_initialInfo;
	EncParam	vpu_encParam;
	EncOutputInfo	vpu_outputInfo;
	//
	unsigned char	vpu_sps[1024];
	unsigned int	vpu_spslen;
	unsigned char	vpu_pps[1024];
	unsigned int	vpu_ppslen;
	//
	unsigned char	*vpu_bitstreamVirt;
	unsigned long	vpu_bitstreamPhy;
	//
	FrameBuffer	vpu_frames[VPU_ENC_MAX_FRAME];
	int		vpu_nframes;
	//
	FrameBuffer	vpu_InBuffer;
	unsigned char 	*vpu_InBufferVirt;
	unsigned long	vpu_InBufferPhy;
	// for managing physical memories
	PhyMemInfo	vpu_phymem;
	//
}	vpu_context_t;

int vpu_encoder_init(vpu_context_t *ctx, int width, int height, int fps_n, int fps_d, int bitrate, int gopsize);
int vpu_encoder_deinit(vpu_context_t *ctx);
int vpu_encoder_reconfigure(vpu_context_t *ctx, int bitrateKbps, unsigned int framerate);
const unsigned char * vpu_encoder_get_h264_sps(vpu_context_t *ctx, int *size);
const unsigned char * vpu_encoder_get_h264_pps(vpu_context_t *ctx, int *size);
unsigned char * vpu_encoder_encode(vpu_context_t *ctx, unsigned char *frame, int framesize, int *encsize);

#endif /* __VPU_COMMON_H__ */

