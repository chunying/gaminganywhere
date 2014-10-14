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
#include <string.h>
#include <stdlib.h>
#include <mfxvideo.h>
#include <windows.h>

#include "allocator.h"
#include "mfx-common.h"

int load_frame_init(const char *filename, int w, int h);
unsigned char * load_frame(int *sz);
void load_frame_close();
unsigned long long load_frame_count();
//
int write_frame_init(const char *filename);
int write_frame(unsigned char *buf, int size);
void write_frame_close();
unsigned long long write_frame_bytes();

static int w, h, fps;

int
mfx_encode(int cid) {
	mfxStatus sts = MFX_ERR_NONE;
	mfxSyncPoint vppsync = NULL;
	mfxSyncPoint encsync = NULL;
	mfxFrameInfo vppdefinfo;
	unsigned long long id = 0;
	unsigned long long fpsunit = 90000 / fps;
	memset(&vppdefinfo, 0, sizeof(vppdefinfo));
	vppdefinfo.PicStruct		= MFX_PICSTRUCT_PROGRESSIVE;
	vppdefinfo.FourCC		= MFX_FOURCC_YV12;	// the only difference
	vppdefinfo.ChromaFormat		= MFX_CHROMAFORMAT_YUV420;
	vppdefinfo.Width		= MFX_ALIGN16(w);
	vppdefinfo.Height		= MFX_ALIGN16(h);
	vppdefinfo.CropW		= w;
	vppdefinfo.CropH		= h;
	vppdefinfo.FrameRateExtN	= fps;
	vppdefinfo.FrameRateExtD	= 1;
	//
	while(sts >= MFX_ERR_NONE || sts == MFX_ERR_MORE_DATA) {
		mfxFrameSurface1 *svppin, *svppout;
		int framesize;
		unsigned char *frame = load_frame(&framesize);
		//
		if(frame == NULL) {
			ga_error("END-OF-FRAME\n");
			break;
		}
		//
		svppin  = frame_pool_get(_vpppool[cid][0], &_vppresponse[cid][0]);
		svppout = frame_pool_get(_vpppool[cid][1], &_vppresponse[cid][1]);
		if(svppin == NULL || svppout == NULL) {
			ga_error("No surface available (%p, %p)\n", svppin, svppout);
			break;
		}
		if(fa_lock(NULL, svppin->Data.MemId, &svppin->Data) != MFX_ERR_NONE) {
			ga_error("Unable to lock VPP frame\n");
			break;
		}
		// fill frame info
		memcpy(&svppin->Info,  &vppdefinfo, sizeof(mfxFrameInfo));
		memcpy(&svppout->Info, &vppdefinfo, sizeof(mfxFrameInfo));
		svppin->Info.FourCC  = MFX_FOURCC_YV12;
		svppout->Info.FourCC = MFX_FOURCC_NV12;
		svppin->Data.TimeStamp = id * fpsunit;
		id++;
		//ga_error("In timestamp = %llu\n", svppin->Data.TimeStamp);
		// copy the frame
		do {
			mfxU8 *dst;
			int i, w2 = w/2, h2 = h/2, p2 = svppin->Data.Pitch/2;
			unsigned char *src = frame;
			// Copy Y
			for(dst = svppin->Data.Y, i = 0; i < h; i++) {
				memcpy(dst, src, w);
				dst += svppin->Data.Pitch;
				src += w;
			}
			// Copy U
			for(dst = svppin->Data.U, i = 0; i < h2; i++) {
				memcpy(dst, src, w2);
				dst += p2;
				src += w2;
			}
			// Copy V
			for(dst = svppin->Data.V, i = 0; i < h2; i++) {
				memcpy(dst, src, w2);
				dst += p2;
				src += w2;
			}
		} while(0);
		//
		if(fa_unlock(NULL, svppin->Data.MemId, &svppin->Data) != MFX_ERR_NONE) {
			ga_error("Unable to unlock VPP frame\n");
			break;
		}
		// do VPP
		sts = mfx_encode_vpp(_session[cid], svppin, svppout, &vppsync);
		// VPP errors?
		if(sts == MFX_ERR_MORE_DATA)	continue;
		if(sts == MFX_ERR_MORE_SURFACE)	continue;
		if(sts != MFX_ERR_NONE) {
			mfx_invalid_status(sts);
			ga_error("video encoder: VPP failed.\n");
			break;
		}
		//
		//MFXVideoCORE_SyncOperation(_session[cid], vppsync, MFX_INFINITE);
		// do ENCODE
		sts = mfx_encode_encode(_session[cid], svppout, &_mfxbs[cid], &encsync);
		//
		if(sts == MFX_ERR_MORE_DATA)	continue;
		if(sts != MFX_ERR_NONE) {
			mfx_invalid_status(sts);
			ga_error("video encoder: encode failed.\n");
			break;
		}
		//
		//ga_error("Out timestamp = d:%lld t:%llu\n", _mfxbs[cid].DecodeTimeStamp, _mfxbs[cid].TimeStamp);
		// save frame
		MFXVideoCORE_SyncOperation(_session[cid], encsync, MFX_INFINITE);
#if 1
		unsigned char *ptr = _mfxbs[cid].Data, *nextptr;
		int naltype, nalsize, nali = 0;
		int off, nextoff;
		// each frame can have only 1 nal ...
		if(ptr != NULL)
			ptr += _mfxbs[cid].DataOffset;
		off = nextoff = 4;
		while(ptr != NULL) {
			// search for startcode 00 00 01 or 00 00 00 01
			for(nextptr = ptr+3;
					nextptr < _mfxbs[cid].Data+_mfxbs[cid].DataOffset+_mfxbs[cid].DataLength-4;
					nextptr++) {
				if(*nextptr == 0 && *(nextptr+1) == 0) {
					if(*(nextptr+2) == 1) {
						// 00 00 01
						nextoff = 3;
						break;
					} else if(*(nextptr+2) == 0 && *(nextptr+3) == 1) {
						// 00 00 00 01
						nextoff = 4;
						break;
					}
				}
			}
			//
			if(nextptr < _mfxbs[cid].Data+_mfxbs[cid].DataOffset+_mfxbs[cid].DataLength-4) {
				nalsize = nextptr - ptr;
				ga_error("XXX: nal[%d] type=%d size=%d\n", nali, *(ptr+off) & 0x1f, nalsize);
				ptr = nextptr;
			} else {
				nalsize = _mfxbs[cid].Data+_mfxbs[cid].DataOffset+_mfxbs[cid].DataLength-ptr;
				ga_error("XXX: nal[%d] type=%d size=%d\n", nali, *(ptr+off) & 0x1f, nalsize);
				ptr = NULL;
			}
			nali++;
			off = nextoff;
		};
#endif
		write_frame(_mfxbs[cid].Data, _mfxbs[cid].DataLength);
		_mfxbs[cid].DataLength = _mfxbs[cid].DataOffset = 0;
	}
	//
	while(sts >= MFX_ERR_NONE) {
		sts = mfx_encode_encode(_session[cid], NULL, &_mfxbs[cid], &encsync);
	}
	//
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

static FILE *fin = NULL;
static unsigned long long framecount = 0;
static int framesize = 0;
static int framealign = 0;
static unsigned char *framebuf = NULL;

int
load_frame_init(const char *filename, int w, int h) {
	load_frame_close();
	if((fin = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "load_frame: open '%s' failed.\n", filename);
		return -1;
	}
	framesize = w * h * 3 / 2;
	framecount = 0;
	if((framebuf = (unsigned char*) malloc(framesize + 32)) == NULL) {
		fprintf(stderr, "load_frame: memory allocation failed.\n");
		load_frame_close();
		return -1;
	}
	return 0;
}

unsigned char *
load_frame(int *sz) {
	int rlen;
	if(fin == NULL || framebuf == NULL)
		return NULL;
	rlen = fread(framebuf + framealign, sizeof(char), framesize, fin);
	if(rlen == 0)
		return NULL;
	if(rlen < framesize) {
		fprintf(stderr, "load_frame: incomplete frame detected.\n");
		return NULL;
	}
	if(sz)
		*sz = rlen;
	framecount++;
	return framebuf + framealign;
}

void
load_frame_close() {
	framesize = 0;
	framecount = 0;
	if(fin)		fclose(fin);
	if(framebuf)	free(framebuf);
	fin = NULL;
	framebuf = NULL;
	return;
}

unsigned long long
load_frame_count() {
	return framecount;
}

//////////////////////////////////////////////////////////////////////////////

static FILE *fout = NULL;
static unsigned long long byteswritten = 0;

int
write_frame_init(const char *filename) {
	if((fout = fopen(filename, "wb")) == NULL)
		return -1;
	byteswritten = 0;
	return 0;
}

int
write_frame(unsigned char *buf, int size) {
	int wlen = 0;
	if(fout == NULL || buf == NULL)
		return -1;
	if(size > 0) {
		wlen = fwrite(buf, sizeof(char), size, fout);
		byteswritten += wlen;
	}
	return wlen;
}

void
write_frame_close() {
	if(fout)
		fclose(fout);
	fout = NULL;
	byteswritten = 0;
	return;
}

unsigned long long
write_frame_bytes() {
	return byteswritten;
}

//////////////////////////////////////////////////////////////////////////////

int
main(int argc, char *argv[]) {
	if(argc < 6) {
		fprintf(stderr, "usage: %s width height framerate infile.raw outfile.264\n", argv[0]);
		return -1;
	}
	//
	w = strtol(argv[1], NULL, 0);
	h = strtol(argv[2], NULL, 0);
	fps = strtol(argv[3], NULL, 0);
	//
	if(load_frame_init(argv[4], w, h) < 0)
		return -1;	
	if(write_frame_init(argv[5]) < 0)
		return -1;
	//
	if(mfx_init_internal(0, w, h, fps, 3000/*Kbps*/, 0/*RGBmode*/) < 0)
		return -1;
	//
	do {
		LARGE_INTEGER t1, t2, freq;
		double elapsed;
		unsigned long long f, w;
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&t1);
		mfx_encode(0);
		QueryPerformanceCounter(&t2);
		elapsed = 1.0 * (t2.QuadPart - t1.QuadPart) / freq.QuadPart;
		f = load_frame_count();
		w = write_frame_bytes();
		printf("%lld frames processed in %.4f second(s) [fps = %.2f]\n",
			f, elapsed, 1.0*f/elapsed);
		printf("%lld bytes written\n", w);
	} while(0);
	//
	mfx_deinit_internal(0);
	load_frame_close();
	write_frame_close();
	//
	return 0;
}

