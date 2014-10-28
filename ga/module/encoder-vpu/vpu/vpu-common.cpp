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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "vpu-common.h"

#ifdef NO_LIBGA
#define	ga_error	printf
#else
#include "ga-common.h"
#endif

// release all physical memory
static void
vpu_encoder_free_phymem(vpu_context_t *ctx) {
	int i;
	vpu_mem_desc desc;
	//
	if(ctx == NULL)
		return;
	//
	for(i = 0; i < ctx->vpu_phymem.nPhyNum; i++) {
		desc.size	= ctx->vpu_phymem.phyMem_size[i];
		desc.phy_addr	= ctx->vpu_phymem.phyMem_phyAddr[i];
		desc.cpu_addr	= ctx->vpu_phymem.phyMem_cpuAddr[i];
		desc.virt_uaddr	= ctx->vpu_phymem.phyMem_virtAddr[i];
		IOFreeVirtMem(&desc);
		IOFreePhyMem(&desc);
	}
	bzero(&ctx->vpu_phymem, sizeof(ctx->vpu_phymem));
	return;
}

// allocate one physical memory, with alignment
static int
vpu_encoder_alloc_phymem(vpu_context_t *ctx, vpu_mem_desc *desc, int alignment) {
	int ret;
	vpu_mem_desc tmp;
	//
	if(ctx == NULL)
		return -1;
	//
	if(ctx->vpu_phymem.nPhyNum >= VPU_ENC_MAX_NUM_MEM_REQS) {
		ga_error("encoder-vpu: no more physical address slots (max=%d).\n",
			VPU_ENC_MAX_NUM_MEM_REQS);
		return -1;
	}
	//
	bzero(&tmp, sizeof(tmp));
	tmp.size = desc->size + alignment;
	if((ret = IOGetPhyMem(&tmp)) != RETCODE_SUCCESS) {
		ga_error("encoder-vpu: get physical address failed.\n");
		return -1;
	}
	if(tmp.phy_addr == 0) {
		ga_error("encoder-vpu: physical address == 0.\n");
		return -1;
	}
	if((tmp.virt_uaddr = (unsigned long) IOGetVirtMem(&tmp)) == 0) {
		ga_error("encoder-vpu: get virtual address failed.\n");
		IOFreePhyMem(&tmp);
		return -1;
	}
	// everything is fine!
	ctx->vpu_phymem.phyMem_phyAddr[ctx->vpu_phymem.nPhyNum] = tmp.phy_addr;
	ctx->vpu_phymem.phyMem_cpuAddr[ctx->vpu_phymem.nPhyNum] = tmp.cpu_addr;
	ctx->vpu_phymem.phyMem_virtAddr[ctx->vpu_phymem.nPhyNum] = tmp.virt_uaddr;
	ctx->vpu_phymem.phyMem_size[ctx->vpu_phymem.nPhyNum] = tmp.size;
	ctx->vpu_phymem.nPhyNum++;
	//
	desc->phy_addr = vpu_align(tmp.phy_addr, alignment);
	desc->virt_uaddr = vpu_align(tmp.virt_uaddr, alignment);
	//
	return 0;
}

static int
vpu_encoder_alloc_framebuffer1(vpu_context_t *ctx, FrameBuffer *fb, int index, unsigned long *phyaddr, unsigned char **virtaddr) {
	vpu_mem_desc desc;
	static int frameIndex = 0;
	//
	if(ctx == NULL)
		return -1;
	// assume always YUV420P
	bzero(&desc, sizeof(desc));
	// add an additional size
	desc.size = ctx->vpu_phy_framesize + ctx->vpu_phy_width * ctx->vpu_phy_height / 4;
	if(vpu_encoder_alloc_phymem(ctx, &desc, 4) < 0)
		return -1;
	fb->myIndex	= index;
	fb->strideY	= ctx->vpu_phy_width;
	fb->strideC	= ctx->vpu_phy_width / 2;
	fb->bufY	= desc.phy_addr;
	fb->bufCb	= fb->bufY  + ctx->vpu_phy_width * ctx->vpu_phy_height;
	fb->bufCr	= fb->bufCb + ctx->vpu_phy_width * ctx->vpu_phy_height / 4;
	fb->bufMvCol	= fb->bufCr + ctx->vpu_phy_width * ctx->vpu_phy_height / 4;
	if(phyaddr != NULL) {
		*phyaddr = desc.phy_addr;
	}
	if(virtaddr != NULL) {
		*virtaddr = (unsigned char*) desc.virt_uaddr;
	}
	//
	return 0;
}

static int
vpu_encoder_alloc_framebuffer(vpu_context_t *ctx) {
	int i;
	if(ctx == NULL)
		return -1;
	if(ctx->vpu_nframes <= 0 || ctx->vpu_nframes > VPU_ENC_MAX_FRAME) {
		ga_error("encoder-vpu: invalid vpu_nframes (%d).\n", ctx->vpu_nframes);
		return -1;
	}
	for(i = 0; i < ctx->vpu_nframes; i++) {
		if(vpu_encoder_alloc_framebuffer1(ctx, &ctx->vpu_frames[i], i, NULL, NULL) < 0)
			return -1;
	}
	return 0;
}

static int
vpu_encoder_retrieve_h264_param(vpu_context_t *ctx, int type, unsigned char *buf, int buflen) {
	EncHeaderParam hdrparam;
	int ret;
	unsigned char *virt;
	//
	bzero(&hdrparam, sizeof(hdrparam));
	hdrparam.headerType = type;
	ret = vpu_EncGiveCommand(ctx->vpu_handle, ENC_PUT_AVC_HEADER, &hdrparam);
	if(ret != RETCODE_SUCCESS) {
		return -1;
	}
	if(hdrparam.size > buflen) {
		ga_error("encoder-vpu: buffer size is insufficient (%d > %d)\n", hdrparam.size, buf); 
		return -1;
	}
	//
	virt = ctx->vpu_bitstreamVirt + (hdrparam.buf - ctx->vpu_bitstreamPhy);
	// skip framing
	if(virt[0]==0x00 && virt[1]==0x00) {
		if(virt[2]==0x00 && virt[3]==0x01) {
			virt += 4;
			hdrparam.size -= 4;
		} else if(virt[2]==0x01) {
			virt += 3;
			hdrparam.size -= 3;
		}
	}
	// copy
	bcopy(virt, buf, hdrparam.size);
	//
	return hdrparam.size;
}

static int
vpu_encoder_retrieve_h264_spspps(vpu_context_t *ctx) {
	int len;
	//
	if(ctx == NULL)
		return -1;
	//
	if((len = vpu_encoder_retrieve_h264_param(ctx, SPS_RBSP, ctx->vpu_sps, sizeof(ctx->vpu_sps))) < 0) {
		ga_error("encoder-vpu: retrieve SPS failed.\n");
		return -1;
	}
	ctx->vpu_spslen = len;
	//
	if((len = vpu_encoder_retrieve_h264_param(ctx, PPS_RBSP, ctx->vpu_pps, sizeof(ctx->vpu_pps))) < 0) {
		ga_error("encoder-vpu: retrieve PPS failed.\n");
		return -1;
	}
	ctx->vpu_ppslen = len;
	//
	return 0;
}

int
vpu_encoder_init(vpu_context_t *ctx, int width, int height, int fps_n, int fps_d, int bitrate, int gopsize) {
	int ret;
	vpu_mem_desc desc;
	vpu_versioninfo ver;
	EncExtBufInfo extbufinfo;
	//
	if(ctx == NULL) {
		ga_error("encoder-vpu: context == NULL!\n");
		return -1;
	}
	if(ctx->vpu_initialized != 0)
		return 0;
	bzero(ctx, sizeof(vpu_context_t));
	// assume always YUV420P
	ctx->vpu_width		= width;			// original size
	ctx->vpu_height		= height;
	ctx->vpu_framesize	= width * height * 3 / 2;
	ctx->vpu_phy_width 	= vpu_align(width, 16);		// with alignment
	ctx->vpu_phy_height	= vpu_align(height, 4);
	ctx->vpu_phy_framesize	= ctx->vpu_phy_width * ctx->vpu_phy_height * 3 / 2;
	ga_error("encoder-vpu: real %dx%d [%d bytes] -> phy (%dx%d [%d bytes])\n",
		ctx->vpu_width, ctx->vpu_height, ctx->vpu_framesize,
		ctx->vpu_phy_width, ctx->vpu_phy_height, ctx->vpu_phy_framesize);
	// init + get version
	if((ret = vpu_Init(NULL)) != RETCODE_SUCCESS) {
		ga_error("encoder-vpu: init failed.\n");
		return -1;
	}
	if((ret = vpu_GetVersionInfo(&ver)) != RETCODE_SUCCESS) {
		ga_error("encoder-vpu: cannot get VPU version.\n");
		goto init_failed;
	}
	ga_error("encoder-vpu: firmware %d.%d.%d; libvpu: %d.%d.%d \n",
		ver.fw_major, ver.fw_minor, ver.fw_release,
		ver.lib_major, ver.lib_minor, ver.lib_release);
	// alloc bitstream buffer
	desc.size = VPU_ENC_BITSTREAM_SIZE + VPU_ENC_MPEG4_SCRATCH_SIZE;
	if(vpu_encoder_alloc_phymem(ctx, &desc, 512/*alignment*/) < 0) {
		ga_error("encoder-vpu: alloca bitstream failed.\n");
		goto init_failed;
	}
	ctx->vpu_bitstreamPhy = desc.phy_addr;
	ctx->vpu_bitstreamVirt = (unsigned char*) desc.virt_uaddr;
	// open it!
	ctx->vpu_encOP.bitstreamBuffer = ctx->vpu_bitstreamPhy;
	ctx->vpu_encOP.bitstreamBufferSize = VPU_ENC_BITSTREAM_SIZE;
	ctx->vpu_encOP.bitstreamFormat = STD_AVC;	/* h.264 */
	ctx->vpu_encOP.picWidth = width;
	ctx->vpu_encOP.picHeight = height;
	ctx->vpu_encOP.frameRateInfo = fps_n | ((fps_d-1)<<16);
	ctx->vpu_encOP.bitRate = bitrate;
	ctx->vpu_encOP.gopSize = gopsize;
	ctx->vpu_encOP.rcIntraQp = 25 /* XXX: intra QP: 0 - 51 */;
	ctx->vpu_encOP.intraRefresh = 8;
	ctx->vpu_encOP.avcIntra16x16OnlyModeEnable = 1;
#if 0
	ctx->vpu_encOP.slicemode.sliceMode = 1;	// enabled
	ctx->vpu_encOP.slicemode.sliceSizeMode = 1;	// 0-bits, 1-Macroblocks
	ctx->vpu_encOP.slicemode.sliceSize = (width * height / 256) / 4;	// suppose MB size = 16x16
#else
	ctx->vpu_encOP.slicemode.sliceMode = 0;	// now disabled, but should be enabled
	ctx->vpu_encOP.slicemode.sliceSizeMode = 0;	// now disabled, but should be enabled
	ctx->vpu_encOP.slicemode.sliceSize = 0;	// now disabled, but should be enabled
#endif
	ctx->vpu_encOP.RcIntervalMode = 1;	// Frame level
	ctx->vpu_encOP.MESearchRange = 4;	// minimum: H+/-15; V+/-15
	// for i.MX6
	ctx->vpu_encOP.ringBufferEnable = 0;
	ctx->vpu_encOP.dynamicAllocEnable = 0;
	//
	if((ret = vpu_EncOpen(&ctx->vpu_handle, &ctx->vpu_encOP)) != RETCODE_SUCCESS) {
		ga_error("encoder-vpu: open failed, error = %d\n", ret);
		goto init_failed;
	}
	//
	if((ret = vpu_EncGetInitialInfo(ctx->vpu_handle, &ctx->vpu_initialInfo)) != RETCODE_SUCCESS) {
		ga_error("encoder-vpu: get initial info failed, error = %d\n", ret);
		goto init_failed;
	}
	// +4, preserve last 2 for subsample-A/B frames
	ctx->vpu_nframes = ctx->vpu_initialInfo.minFrameBufferCount + 4;
	//
	ga_error("encoder-vpu: number of vpu frame buffer = %d\n", ctx->vpu_nframes);
	if(ctx->vpu_nframes > 0) {
		if(vpu_encoder_alloc_framebuffer(ctx) < 0) {
			ga_error("encoder-vpu: allocate frame buffer failed.\n");
			goto init_failed;
		}
	}
	//
	bzero(&extbufinfo, sizeof(extbufinfo));
	extbufinfo.scratchBuf.bufferBase = ctx->vpu_bitstreamPhy + VPU_ENC_BITSTREAM_SIZE;
	extbufinfo.scratchBuf.bufferSize = VPU_ENC_MPEG4_SCRATCH_SIZE;
	ret = vpu_EncRegisterFrameBuffer(ctx->vpu_handle, ctx->vpu_frames, ctx->vpu_nframes-2,
			ctx->vpu_phy_width, ctx->vpu_width,
			ctx->vpu_frames[ctx->vpu_nframes-2].bufY,
			ctx->vpu_frames[ctx->vpu_nframes-1].bufY,
			&extbufinfo);
	if(ret != RETCODE_SUCCESS) {
		ga_error("encoder-vpu: register frame buffer failed.\n");
		goto init_failed;
	}
	//
	if(vpu_encoder_retrieve_h264_spspps(ctx) < 0) {
		ga_error("encoder-vpu: get SPS/PPS failed.\n");
		goto init_failed;
	} else {
		ga_error("encoder-vpu: SPS (%d bytes); PPS (%d bytes)\n", 
			ctx->vpu_spslen, ctx->vpu_ppslen);
	}
	// alloc input buffer
	if(vpu_encoder_alloc_framebuffer1(ctx, &ctx->vpu_InBuffer, ctx->vpu_nframes,
				&ctx->vpu_InBufferPhy, &ctx->vpu_InBufferVirt) < 0) {
		ga_error("encoder-vpu: alloc input/output buffer failed.\n");
		goto init_failed;
	}
	ga_error("encoder-vpu: in-buffer  phy=%x, virt=%p\n", ctx->vpu_InBufferPhy, ctx->vpu_InBufferVirt);
	// finally, set vpu_nframes to a correct number
	ctx->vpu_nframes -= 2;
	//
	ctx->vpu_initialized = 1;
	//
	return 0;
init_failed:
	vpu_encoder_deinit(ctx);
	return -1;
}

int
vpu_encoder_deinit(vpu_context_t *ctx) {
	int ret;
	//
	if(ctx == NULL)
		return 0;
	//
	if(ctx->vpu_handle > 0) {
		ret = vpu_EncClose(ctx->vpu_handle);
		if(ret == RETCODE_FRAME_NOT_COMPLETE) {
			vpu_EncGetOutputInfo(ctx->vpu_handle, &ctx->vpu_outputInfo);
			ret = vpu_EncClose(ctx->vpu_handle);
			if(ret < 0) {
				ga_error("encoder-vpu: close vpu failed, error = %d\n", ret);
			}
		}
	}
	//
	vpu_encoder_free_phymem(ctx);
	//
	vpu_UnInit();
	bzero(ctx, sizeof(vpu_context_t));
	//
	return 0;
}

int
vpu_encoder_reconfigure(vpu_context_t *ctx, int bitrateKbps, unsigned int framerate) {
	int ret;
	if(bitrateKbps > 0) {
		ret = vpu_EncGiveCommand(ctx->vpu_handle, ENC_SET_BITRATE, &bitrateKbps);
		if(ret != RETCODE_SUCCESS)
			return -1;
	}
	if(framerate != 0) {
		ret = vpu_EncGiveCommand(ctx->vpu_handle, ENC_SET_FRAME_RATE, &framerate);
		if(ret != RETCODE_SUCCESS)
			return -1;
	}
	return 0;
}

const unsigned char *
vpu_encoder_get_h264_sps(vpu_context_t *ctx, int *size) {
	if(ctx == NULL || ctx->vpu_initialized == 0)
		return NULL;
	if(ctx->vpu_spslen <= 0)
		return NULL;
	*size = ctx->vpu_spslen;
	return ctx->vpu_sps;
}

const unsigned char *
vpu_encoder_get_h264_pps(vpu_context_t *ctx, int *size) {
	if(ctx == NULL || ctx->vpu_initialized == 0)
		return NULL;
	if(ctx->vpu_ppslen <= 0)
		return NULL;
	*size = ctx->vpu_ppslen;
	return ctx->vpu_pps;
}

unsigned char *
vpu_encoder_encode(vpu_context_t *ctx, unsigned char *frame, int framesize, int *encsize) {
	int ret, retries;
	EncParam param;
	FrameBuffer fb;
	//
	if(ctx == NULL || ctx->vpu_initialized == 0)
		return NULL;
	// copy frame: assume always YUV420P
	if(framesize != ctx->vpu_framesize) {
		ga_error("encoder-video: invalid frame size %d (!=%d)\n",
			framesize, ctx->vpu_framesize);
		return NULL;
	}
	//
	bcopy(frame, ctx->vpu_InBufferVirt, framesize);
	//
	bzero(&param, sizeof(param));
	bzero(&fb, sizeof(fb));
	fb.myIndex = ctx->vpu_nframes + 1;	// very important on i.MX6
	// frame is a compact YUV 420P
	fb.strideY = ctx->vpu_width;
	fb.strideC = ctx->vpu_width>>1;
	fb.bufY = ctx->vpu_InBuffer.bufY;
	fb.bufCb = ctx->vpu_InBuffer.bufY + ctx->vpu_width * ctx->vpu_height;
	fb.bufCr = ctx->vpu_InBuffer.bufCb + ctx->vpu_width * ctx->vpu_height / 4;;
	fb.bufMvCol = 1;	// no mv needed
	param.sourceFrame = &fb;
	//
	if((ret = vpu_EncStartOneFrame(ctx->vpu_handle, &param)) != RETCODE_SUCCESS) {
		ga_error("encoder-vpu: encode failed, error = %d\n", ret);
		return NULL;
	}
	// timeout after 2 seconds
	retries = 4;
	while(vpu_WaitForInt(500) != RETCODE_SUCCESS) {
		// infinite loop?
		if(--retries <= 0) {
			ga_error("encoder-vpu: encoder timed-out.\n");
			return NULL;
		}
	}
	// get encoded bits
	bzero(&ctx->vpu_outputInfo, sizeof(ctx->vpu_outputInfo));
	ret = vpu_EncGetOutputInfo(ctx->vpu_handle, &ctx->vpu_outputInfo);
	if(ret != RETCODE_SUCCESS) {
		ga_error("encoder-vpu: get output failed, error = %d\n", ret);
		return NULL;
	}
	//
	*encsize = ctx->vpu_outputInfo.bitstreamSize;
	return ctx->vpu_bitstreamVirt + (ctx->vpu_outputInfo.bitstreamBuffer - ctx->vpu_bitstreamPhy);
}

#ifndef GA_MODULE

int
main(int argc, char *argv[]) {
	FILE *fin = NULL;
	FILE *fout = NULL;
	int width, height, fps, framesize, framecount;
	unsigned char *framebuf;
	struct timeval tv1, tv2;
	long long elapsed;
	vpu_context_t vpu;
	//
	bzero(&vpu, sizeof(vpu));
	//
	if(argc < 5) {
		ga_error("usage: %s input.yuv width height fps [output.264]\n", argv[0]);
		return -1;
	}
	//
	width = strtol(argv[2], NULL, 0);
	height = strtol(argv[3], NULL, 0);
	fps = strtol(argv[4], NULL, 0);
	if((fin = fopen(argv[1], "rb")) == NULL) {
		ga_error("open %s failed.\n", argv[0]);
		return -1;
	}
	framesize = width * height * 3 / 2;
	//
	if((framebuf = (unsigned char*) malloc(framesize)) == NULL) {
		ga_error("alloc frame buffer failed.\n");
		return -1;
	}
	//
	ga_error("input: %s, %dx%d, size=%d\n", argv[1], width, height, framesize);
	//
	if(vpu_encoder_init(&vpu, width, height, fps, 1, 3000, fps) < 0)
		ga_error("encoder-vpu: init failed.\n");
	if(argc > 5 && (fout = fopen(argv[5], "wb")) == NULL) {
		ga_error("open %s failed.\n", argv[5]);
		return -1;
	}
	// write sps/pps
	if(fout) {
		const unsigned char *raw;
		int rawsize;
		if((raw = vpu_encoder_get_h264_sps(&vpu, &rawsize)) != NULL)
			fwrite(raw, 1, rawsize, fout);
		if((raw = vpu_encoder_get_h264_pps(&vpu, &rawsize)) != NULL)
			fwrite(raw, 1, rawsize, fout);
	}
	//
	framecount = 0;
	gettimeofday(&tv1, NULL);
	while(fread(framebuf, 1, framesize, fin) == framesize) {
		unsigned char *encbuf;
		int encsize;
		if((encbuf = vpu_encoder_encode(&vpu, framebuf, framesize, &encsize)) == NULL) {
			break;
		}
		if(fout)
			fwrite(encbuf, 1, encsize, fout);
		framecount++;
	}
	gettimeofday(&tv2, NULL);
	elapsed = (tv2.tv_sec - tv1.tv_sec)*1000000 + (tv2.tv_usec - tv1.tv_usec);
	// finalize
	if(fin)		fclose(fin);
	if(fout)	fclose(fout);
	vpu_encoder_deinit(&vpu);
	ga_error("%d frames processed in %.4f sec, fps = %.4f\n",
		framecount, 0.000001 * elapsed,
		1000000.0 * framecount / elapsed);
	return 0;
}
#endif /* ! GA_MODULE */
