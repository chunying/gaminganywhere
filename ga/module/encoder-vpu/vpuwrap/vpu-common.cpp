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

// required package: imx-lib-utb0, libfslvpuwrapper

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
// VPU lib
#ifdef __cplusplus
extern "C" {
#endif
#include <vpu_lib.h>
#include <vpu_io.h>
#include <vpu_wrapper.h>
#ifdef __cplusplus
}
#endif
//

#include "vpu-common.h"

#ifdef NO_LIBGA
#define	ga_error	printf
#else
#endif

#define	VPU_MAX_FRAME_NUM		(10)
#define VPU_ENC_MAX_NUM_MEM_REQS	(30)

#define	vpu_align(ptr, align)		(((unsigned int) ptr+(align)-1)/(align)*(align))

typedef struct EncMemInfo {
	//virtual mem info
	int nVirtNum;
	unsigned int virtMem[VPU_ENC_MAX_NUM_MEM_REQS];
	//phy mem info
	int nPhyNum;
	unsigned int phyMem_virtAddr[VPU_ENC_MAX_NUM_MEM_REQS];
	unsigned int phyMem_phyAddr[VPU_ENC_MAX_NUM_MEM_REQS];
	unsigned int phyMem_cpuAddr[VPU_ENC_MAX_NUM_MEM_REQS];
	unsigned int phyMem_size[VPU_ENC_MAX_NUM_MEM_REQS];	
}	EncMemInfo;

static int		vpu_initialized = 0;
static int		vpu_width = 0;
static int		vpu_height = 0;
static int		vpu_framesize = 0;
static int		vpu_picsizeY = 0;
//
static VpuMemInfo	vpu_meminfo;
static EncMemInfo	enc_meminfo;
static VpuEncOpenParam	vpu_openparam;
static VpuEncInitInfo	vpu_initinfo;
static VpuEncHandle	vpu_handle;
static VpuEncEncParam	vpu_params;
//
static unsigned char	vpu_sps[1024];
static unsigned int	vpu_spslen = 0;
static unsigned char	vpu_pps[1024];
static unsigned int	vpu_ppslen = 0;
//
static VpuFrameBuffer	vpu_framebuf[VPU_MAX_FRAME_NUM];
static int		vpu_frames = 0;		// number of internal vpu frames
//
static unsigned char	*vpu_InputPhy = NULL;
static unsigned char	*vpu_InputVirt = NULL;
static unsigned char	*vpu_OutputPhy = NULL;
static unsigned char	*vpu_OutputVirt = NULL;
static int		vpu_IOSize = 0;

static void
vpu_encoder_free_memories(EncMemInfo *emi) {
	int i;
	VpuEncRetCode ret;
	VpuMemDesc desc;
	// free memory
	for(i = 0; i < emi->nVirtNum; i++) {
		free((void*) emi->virtMem[i]);
	}
	// free physical memory
	for(i = 0; i < emi->nPhyNum; i++) {
		desc.nPhyAddr = emi->phyMem_phyAddr[i];
		desc.nVirtAddr = emi->phyMem_virtAddr[i];
		desc.nCpuAddr = emi->phyMem_cpuAddr[i];
		desc.nSize = emi->phyMem_size[i];
		if((ret = VPU_EncFreeMem(&desc)) != VPU_ENC_RET_SUCCESS) {
			ga_error("encoder-vpu: WARNING - free vpu memory failed, ret = %d\n", ret);
		}
	}
	return;
}

static int
vpu_encoder_alloc_memories(VpuMemInfo *mi, EncMemInfo *emi) {
	VpuEncRetCode ret;
	int i, size;
	for(i = 0; i < mi->nSubBlockNum; i++) {
		size = mi->MemSubBlock[i].nAlignment + mi->MemSubBlock[i].nSize;
		if(mi->MemSubBlock[i].MemType == VPU_MEM_VIRT) {
			unsigned char *ptr = (unsigned char*) malloc(size);
			if(ptr == NULL) {
				ga_error("encoder-vpu: alloc memory failed.\n");
				goto alloc_failed;
			}
			mi->MemSubBlock[i].pVirtAddr = (unsigned char*) vpu_align(ptr, mi->MemSubBlock[i].nAlignment);
			emi->virtMem[emi->nVirtNum] = (unsigned int) ptr;
			emi->nVirtNum++;
		} else if(mi->MemSubBlock[i].MemType == VPU_MEM_PHY) {
			VpuMemDesc desc;
			desc.nSize = size;
			if((ret = VPU_EncGetMem(&desc)) != VPU_ENC_RET_SUCCESS) {
				ga_error("encoder-vpu: get vpu memory failed, ret = %d\n", ret);
				goto alloc_failed;
			}
			mi->MemSubBlock[i].pVirtAddr = (unsigned char*) vpu_align(desc.nVirtAddr, mi->MemSubBlock[i].nAlignment);
			mi->MemSubBlock[i].pPhyAddr = (unsigned char*) vpu_align(desc.nPhyAddr, mi->MemSubBlock[i].nAlignment);
			emi->phyMem_phyAddr[emi->nPhyNum] = (unsigned int) desc.nPhyAddr;
			emi->phyMem_virtAddr[emi->nPhyNum] = (unsigned int) desc.nVirtAddr;
			emi->phyMem_cpuAddr[emi->nPhyNum] = (unsigned int) desc.nCpuAddr;
			emi->phyMem_size[emi->nPhyNum] = size;
			emi->nPhyNum++;
		} else {
			goto alloc_failed;
		}
	}
	return 0;
alloc_failed:
	vpu_encoder_free_memories(emi);
	return -1;
}

static int
vpu_encoder_alloc_framebuffer(int n, int width, int height, EncMemInfo *emi, int nAlignment) {
	int i;
	VpuEncRetCode ret;
	int phy_width, phy_height;
	int yStride, uvStride;
	int ySize, uvSize, mvSize;
	//
	phy_width = vpu_align(width, 16);
	phy_height = vpu_align(height, 16);
	yStride = phy_width;
	ySize = yStride * phy_height;
	uvStride = yStride / 2;
	uvSize = ySize / 4;
	mvSize = uvSize;
	if(nAlignment > 1) {
		ySize = vpu_align(ySize, nAlignment);
		uvSize = vpu_align(uvSize, nAlignment);
	}
	//
	for(i = 0; i < n; i++) {
		VpuMemDesc desc;
		unsigned char *ptrphy, *ptrvirt;
		desc.nSize = ySize + uvSize * 2 + mvSize + nAlignment;
		if((ret = VPU_EncGetMem(&desc)) != VPU_ENC_RET_SUCCESS) {
			ga_error("encoder-vpu: alloc frame buffer failed, ret = %d\n", ret);
			return -1;
		}
		ptrphy = (unsigned char*) desc.nPhyAddr;
		ptrvirt = (unsigned char*) desc.nVirtAddr;
		if(nAlignment > 1) {
			ptrphy = (unsigned char*) vpu_align(ptrphy, nAlignment);
			ptrvirt = (unsigned char*) vpu_align(ptrvirt, nAlignment);
		}
		bzero(&vpu_framebuf[i], sizeof(vpu_framebuf[i]));
		vpu_framebuf[i].nStrideY = yStride;
		vpu_framebuf[i].nStrideC = uvStride;
		//
		vpu_framebuf[i].pbufY = ptrphy;
		vpu_framebuf[i].pbufCb = ptrphy + ySize;
		vpu_framebuf[i].pbufCr = ptrphy + ySize + uvSize;
		vpu_framebuf[i].pbufMvCol = ptrphy + ySize + uvSize * 2;
		//
		vpu_framebuf[i].pbufVirtY = ptrvirt;
		vpu_framebuf[i].pbufVirtCb = ptrvirt + ySize;
		vpu_framebuf[i].pbufVirtCr = ptrvirt + ySize + uvSize;
		vpu_framebuf[i].pbufVirtMvCol = ptrvirt + ySize + uvSize * 2;
		//
		emi->phyMem_phyAddr[emi->nPhyNum] = desc.nPhyAddr;
		emi->phyMem_virtAddr[emi->nPhyNum] = desc.nVirtAddr;
		emi->phyMem_cpuAddr[emi->nPhyNum] = desc.nCpuAddr;
		emi->phyMem_size[emi->nPhyNum] = desc.nSize;
		emi->nPhyNum++;
	}
	return 0;
}

static int
vpu_encoder_h264_spspps() {
#if 0
	EncHeaderParam hdrparam;
	int ret;
	//
	bzero(&hdrparam, sizeof(hdrparam));
	hdrparam.headerType = SPS_RBSP;
	ret = vpu_EncGiveCommand(vpu_handle, ENC_PUT_AVC_HEADER, &hdrparam);
	if(hdrparam.size > sizeof(vpu_sps)) {
		ga_error("encoder-vpu: SPS is too long (%d > %d)\n",
			hdrparam.size, sizeof(vpu_sps));
		return -1;
	}
	vpu_spslen = hdrparam.size;
	bcopy(vpu_bitstreamAddr + hdrparam.buf - vpu_bitStreamBuf.phy_addr,
		vpu_sps, hdrparam.size);
	//
	bzero(&hdrparam, sizeof(hdrparam));
	hdrparam.headerType = PPS_RBSP;
	ret = vpu_EncGiveCommand(vpu_handle, ENC_PUT_AVC_HEADER, &hdrparam);
	if(hdrparam.size > sizeof(vpu_pps)) {
		ga_error("encoder-vpu: PPS is too long (%d > %d)\n",
			hdrparam.size, sizeof(vpu_pps));
		return -1;
	}
	vpu_ppslen = hdrparam.size;
	bcopy(vpu_bitstreamAddr + hdrparam.buf - vpu_bitStreamBuf.phy_addr,
		vpu_pps, hdrparam.size);
	//
#endif
	return 0;
}

static void
vpu_encoder_clear() {
	vpu_initialized = 0;
	vpu_width = vpu_height = -1;
	bzero(&vpu_meminfo, sizeof(vpu_meminfo));
	bzero(&enc_meminfo, sizeof(enc_meminfo));
	bzero(&vpu_openparam, sizeof(vpu_openparam));
	bzero(&vpu_initinfo, sizeof(vpu_initinfo));
	bzero(&vpu_handle, sizeof(vpu_handle));
	bzero(&vpu_params, sizeof(vpu_params));
	//
	vpu_spslen = vpu_ppslen = 0;
	//
	return;
}

int
vpu_encoder_init(int width, int height, int fps_n, int fps_d, int bitrate, int gopsize) {
	VpuVersionInfo ver;
	VpuWrapperVersionInfo w_ver;
	VpuEncRetCode ret;
	VpuDecRetCode dret;
	//
	if(vpu_initialized != 0)
		return 0;
	// reset everything
	vpu_encoder_clear();
	// assume always YUV420P
	vpu_width = width;
	vpu_height = height;
	vpu_picsizeY = width * height;
	vpu_framesize = vpu_picsizeY * 3 / 2;
	// init + get version
	if((ret = VPU_EncLoad()) != VPU_ENC_RET_SUCCESS) {
		ga_error("encoder-vpu: load failed, error = %d.\n", ret);
		return -1;
	}
	if((ret = VPU_EncGetVersionInfo(&ver)) != VPU_ENC_RET_SUCCESS) {
		ga_error("encoder-vpu: cannot get version info, error = %d\n", ret);
		goto init_failed;
	}
	if((dret = VPU_EncGetWrapperVersionInfo(&w_ver)) != VPU_DEC_RET_SUCCESS) {
		ga_error("encoder-vpu: cannot get wrapper version version, error = %d.\n", ret);
		goto init_failed;
	}
	ga_error("encoder-vpu: firmware %d.%d.%d; libvpu: %d.%d.%d; wrapper %d.%d.%d(%s) \n",
		ver.nFwMajor, ver.nFwMinor, ver.nFwRelease,
		ver.nLibMajor, ver.nLibMinor, ver.nLibRelease,
		w_ver.nMajor, w_ver.nMinor, w_ver.nRelease,
		w_ver.pBinary ? w_ver.pBinary : "unknown");
	//
	if((ret = VPU_EncQueryMem(&vpu_meminfo)) != VPU_ENC_RET_SUCCESS) {
		ga_error("encoder-vpu: query memory failed, error = %d\n", ret);
		goto init_failed;
	}
	//
	if(vpu_encoder_alloc_memories(&vpu_meminfo, &enc_meminfo) < 0) {
		ga_error("encoder-vpu: alloc memories failed.\n");
		goto init_failed;
	}
	//
	vpu_openparam.nPicWidth = width;
	vpu_openparam.nPicHeight = height;
	vpu_openparam.nFrameRate = fps_n | ((fps_d-1)<<16);
	vpu_openparam.nBitRate = bitrate;
	vpu_openparam.nGOPSize = gopsize;
	vpu_openparam.eFormat = VPU_V_AVC;	/* H.264 */
	vpu_openparam.sMirror = VPU_ENC_MIRDIR_NONE;
	vpu_openparam.eColorFormat = VPU_COLOR_420;
	vpu_openparam.nIntraRefresh = 1;
	vpu_openparam.VpuEncStdParam.avcParam.avc_fmoSliceSaveBufSize = 32;
	//
	if((ret = VPU_EncOpen(&vpu_handle, &vpu_meminfo, &vpu_openparam)) != VPU_ENC_RET_SUCCESS) {
		ga_error("encoder-vpu: open failed.\n");
		goto init_failed;
	}
#if 1
	if((ret = VPU_EncConfig(vpu_handle, VPU_ENC_CONF_NONE, NULL)) != VPU_ENC_RET_SUCCESS) {
		ga_error("encoder-vpu: apply default config(%x), error = %d\n",
			VPU_ENC_CONF_NONE, ret);
		goto init_failed;
	}
#endif
	if((ret = VPU_EncGetInitialInfo(vpu_handle, &vpu_initinfo)) != VPU_ENC_RET_SUCCESS) {
		ga_error("encoder-vpu: get initial info failed, error = %d\n", ret);
		goto init_failed;
	}
	vpu_frames = vpu_initinfo.nMinFrameBufferCount;
	ga_error("encoder-vpu: minimum number of buffer = %d; alignment = %d\n",
		vpu_frames, vpu_initinfo.nAddressAlignment);
	// alloc frame buffers
	if(vpu_encoder_alloc_framebuffer(vpu_frames, width, height, &enc_meminfo, vpu_initinfo.nAddressAlignment) < 0) {
		ga_error("encoder-vpu: alloc frame buffers failed, error = %d\n", ret);
		goto init_failed;
	}
	// register frame buffers
	ret = VPU_EncRegisterFrameBuffer(vpu_handle, vpu_framebuf, vpu_frames, vpu_width);
	if(ret != VPU_ENC_RET_SUCCESS) {
		ga_error("encoder-vpu: register frame buffer failed, error = %d\n", ret);
		goto init_failed;
	}
	// allocate input and output buffer
	do {
		VpuMemInfo mi;
		int size = vpu_framesize + vpu_initinfo.nAddressAlignment * 2;
		bzero(&mi, sizeof(mi));
		mi.nSubBlockNum = 2;
		mi.MemSubBlock[0].MemType = mi.MemSubBlock[1].MemType = VPU_MEM_PHY;
		mi.MemSubBlock[0].nAlignment = mi.MemSubBlock[1].nAlignment = vpu_initinfo.nAddressAlignment;
		mi.MemSubBlock[0].nSize = mi.MemSubBlock[1].nSize = size;
		if(vpu_encoder_alloc_memories(&mi, &enc_meminfo) < 0) {
			ga_error("encoder-vpu: alloc input/output buffer failed.\n");
			goto init_failed;
		}
		vpu_InputPhy = mi.MemSubBlock[0].pPhyAddr;
		vpu_InputVirt = mi.MemSubBlock[0].pVirtAddr;
		vpu_OutputPhy = mi.MemSubBlock[1].pPhyAddr;
		vpu_OutputVirt = mi.MemSubBlock[1].pVirtAddr;
		vpu_IOSize = size;
	} while(0);
	//
	vpu_initialized = 1;
	//
	return 0;
init_failed:
	vpu_encoder_deinit();
	return -1;
}

int
vpu_encoder_deinit() {
	if(vpu_handle != 0) {
		VPU_EncClose(vpu_handle);
		vpu_handle = 0;
	}
	//
	vpu_encoder_free_memories(&enc_meminfo);
	//
	vpu_initialized = 0;
	//
	return 0;
}

const unsigned char *
vpu_encoder_get_h264_sps(int *size) {
	if(vpu_initialized == 0)
		return NULL;
	if(vpu_spslen <= 0)
		return NULL;
	*size = vpu_spslen;
	return vpu_sps;
}

const unsigned char *
vpu_encoder_get_h264_pps(int *size) {
	if(vpu_initialized == 0)
		return NULL;
	if(vpu_ppslen <= 0)
		return NULL;
	*size = vpu_ppslen;
	return vpu_pps;
}

unsigned char *
vpu_encoder_encode(unsigned char *frame, int framesize, int *encsize) {
	VpuEncRetCode ret;
	VpuEncEncParam ep;
	//
	if(vpu_initialized == 0)
		return NULL;
	//
	bcopy(frame, vpu_InputVirt, framesize);
	//
	bzero(&ep, sizeof(ep));
	ep.eFormat = VPU_V_AVC;	/* H.264 */
	ep.nPicWidth = vpu_width;
	ep.nPicHeight = vpu_height;
	//ep.nFrameRate = vpu_openparam.nFrameRate;
	ep.nInPhyInput = (unsigned int) vpu_InputPhy;
	ep.nInVirtInput = (unsigned int) vpu_InputVirt;
	ep.nInInputSize = vpu_IOSize;
	ep.nInPhyOutput = (unsigned int) vpu_OutputPhy;
	ep.nInVirtOutput = (unsigned int) vpu_OutputVirt;
	ep.nInOutputBufLen = vpu_IOSize;
	//
	ret = VPU_EncEncodeFrame(vpu_handle, &ep);
	if(ret != VPU_ENC_RET_SUCCESS) {
		ga_error("encoder-vpu: encode failed, error = %d\n", ret);
		return NULL;
	}
	// input used?
	if((ep.eOutRetCode & VPU_ENC_INPUT_USED) == 0) {
		/* XXX: should reuse this frame, but not handeled now... */
		ga_error("encoder-vpu: frame not used?\n");
	}
	// has output?
	if((ep.eOutRetCode & VPU_ENC_OUTPUT_DIS)
	|| (ep.eOutRetCode & VPU_ENC_OUTPUT_SEQHEADER)) {
		*encsize = ep.nOutOutputSize;
	} else {
		*encsize = 0;
	}
	return vpu_OutputVirt;
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
	if(vpu_encoder_init(width, height, fps, 1, 3000, fps) < 0)
		ga_error("encoder-vpu: init failed.\n");
	if(argc > 5 && (fout = fopen(argv[5], "wb")) == NULL) {
		ga_error("open %s failed.\n", argv[5]);
		return -1;
	}
	//
	framecount = 0;
	gettimeofday(&tv1, NULL);
	while(fread(framebuf, 1, framesize, fin) == framesize) {
		unsigned char *encbuf;
		int encsize;
		if((encbuf = vpu_encoder_encode(framebuf, framesize, &encsize)) == NULL) {
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
	vpu_encoder_deinit();
	ga_error("%d frames processed in %.4f sec, fps = %.4f\n",
		framecount, 0.000001 * elapsed,
		1000000.0 * framecount / elapsed);
	return 0;
}
#endif /* ! GA_MODULE */
