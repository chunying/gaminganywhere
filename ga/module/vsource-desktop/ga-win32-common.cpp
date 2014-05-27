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

#include "ga-win32-common.h"

int
ga_win32_draw_system_cursor(vsource_frame_t *frame) {
	static int capture_cursor = -1;
	static unsigned char bitmask[8] = {
		0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
	};
	int i, j, ptx, pty;
	int ret = -1;
	//
	if(capture_cursor < 0) {
		if(ga_conf_readbool("capture-cursor", 0) != 0) {
			ga_error("vsource: capture-cursor enabled.\n");
			capture_cursor = 1;
		} else {
			capture_cursor = 0;
		}
	}
	if(capture_cursor == 0)
		return 0;
#ifdef WIN32
	ICONINFO iinfo;
	CURSORINFO cinfo;
	HCURSOR hc;
	BITMAP mask, cursor;
	int msize, csize;
	//
	bzero(&cinfo, sizeof(cinfo));
	cinfo.cbSize = sizeof(cinfo);
	if(GetCursorInfo(&cinfo) == FALSE) {
		ga_error("vsource: GetCursorInfo failed, capture-cursor disabled.\n");
		capture_cursor = 0;
		return -1;
	}
	if(cinfo.flags != CURSOR_SHOWING)
		return 0;
	if((hc = CopyCursor(cinfo.hCursor)) == NULL) {
		ga_error("vsource: CopyCursor failed, err = 0x%08x.\n", GetLastError());
		return -1;
	}
	if(GetIconInfo((HICON) hc, &iinfo) == FALSE) {
		ga_error("vsource: GetIconInfo failed.\n");
		goto quitFreeCursor;
	}
	//
	GetObject(iinfo.hbmMask, sizeof(mask), &mask);
	msize = mask.bmHeight * mask.bmWidthBytes;
	if(iinfo.hbmColor != NULL) {
		GetObject(iinfo.hbmColor, sizeof(cursor), &cursor);
		csize = cursor.bmHeight * cursor.bmWidthBytes;
	}
	if(iinfo.hbmColor == NULL) {	// B/W cursor
		unsigned char mbits[8192];
		unsigned char *mcurr, *ccurr, *fcurr;
		if(mask.bmBitsPixel != 1) {
			ga_error("vsource: unsupported B/W cursor bitsPixel - m:%d%s1\n",
				mask.bmBitsPixel, mask.bmBitsPixel == 1 ? "==" : "!=");
			goto quitFreeIconinfo;
		}
		if(msize > sizeof(mbits)) {
			ga_error("vsource: B/W cursor too loarge, ignored.\n");
			goto quitFreeIconinfo;
		}
		if(mask.bmHeight != mask.bmWidth<<1) {
			ga_error("vsource: Bad B/W cursor size (%dx%d)\n",
				mask.bmWidth, mask.bmHeight);
			goto quitFreeIconinfo;
		}
		GetBitmapBits(iinfo.hbmMask, msize, mbits);
#if 0
		ga_error("vsource: B/W cursor msize=%d point(%d-%d,%d-%d) %dx%d planes=%d bitsPixel=%d widthBytes=%d\n",
			msize,
			cinfo.ptScreenPos.x, iinfo.xHotspot,
			cinfo.ptScreenPos.y, iinfo.yHotspot,
			mask.bmWidth, mask.bmHeight,
			mask.bmPlanes, 
			mask.bmBitsPixel,
			mask.bmWidthBytes);
#endif
		mask.bmHeight = mask.bmHeight>>1;
		for(i = 0; i < mask.bmHeight; i++) {
			pty = cinfo.ptScreenPos.y - iinfo.yHotspot + i;
			if(pty < 0)
				continue;
			if(pty >= frame->realheight)
				break;
			mcurr = mbits + i * mask.bmWidthBytes;
			ccurr = mbits + (mask.bmHeight + i) * mask.bmWidthBytes;
			fcurr = frame->imgbuf + (pty * frame->realstride);
			for(j = 0; j < mask.bmWidth; j++) {
				ptx = cinfo.ptScreenPos.x - iinfo.xHotspot + j;
				if(ptx < 0)
					continue;
				if(ptx >= frame->realwidth)
					break;
				if((mcurr[j>>3] & bitmask[j&0x07]) == 0) {
					if((ccurr[j>>3] & bitmask[j&0x07]) != 0) {
						fcurr[ptx*4+0] = 0xff;//= 0;
						fcurr[ptx*4+1] = 0xff;//= 0;
						fcurr[ptx*4+2] = 0xff;//= 0;
						//fcurr[ptx*4+3] = 0xff;
					}
				} else {
					if((ccurr[j>>3] & bitmask[j&0x07]) != 0) {
						fcurr[ptx*4+0] ^= 0xff;//= 0;
						fcurr[ptx*4+1] ^= 0xff;//= 0;
						fcurr[ptx*4+2] ^= 0xff;//= 0;
						//fcurr[ptx*4+3] = 0xff;
					}
				}
			}
		}
	} else {			// color cursor
		unsigned char mbits[8192];
		unsigned char cbits[262144];
		unsigned char *mcurr, *ccurr, *fcurr;
		// Color
		if(mask.bmBitsPixel != 1 || cursor.bmBitsPixel != 32) {
			ga_error("vsource: unsupported cursor bitsPixel - m:%d%s1, c:%d%s32\n",
				mask.bmBitsPixel, mask.bmBitsPixel == 1 ? "==" : "!=",
				cursor.bmBitsPixel, cursor.bmBitsPixel == 32 ? "==" : "!=");
			goto quitFreeIconinfo;
		}
		if(msize > sizeof(mbits) || csize > sizeof(cbits)) {
			ga_error("vsource: cursor too loarge (> 256x256), ignored.\n");
			goto quitFreeIconinfo;
		}
		GetBitmapBits(iinfo.hbmMask, msize, mbits);
		GetBitmapBits(iinfo.hbmColor, csize, cbits);
#if 0
		ga_error("vsource: cursor msize=%d csize=%d point(%d-%d,%d-%d) %dx%d (%dx%d) planes=%d/%d bitsPixel=%d/%d widthBytes=%d/%d\n",
			msize, csize,
			cinfo.ptScreenPos.x, iinfo.xHotspot,
			cinfo.ptScreenPos.y, iinfo.yHotspot,
			mask.bmWidth, mask.bmHeight,
			cursor.bmWidth, cursor.bmHeight,
			mask.bmPlanes, cursor.bmPlanes,
			mask.bmBitsPixel, cursor.bmBitsPixel,
			mask.bmWidthBytes, cursor.bmWidthBytes);
#endif
		for(i = 0; i < mask.bmHeight; i++) {
			pty = cinfo.ptScreenPos.y - iinfo.yHotspot + i;
			if(pty < 0)
				continue;
			if(pty >= frame->realheight)
				break;
			mcurr = mbits + i * mask.bmWidthBytes;
			ccurr = cbits + i * cursor.bmWidthBytes;
			fcurr = frame->imgbuf + (pty * frame->realstride);
			for(j = 0; j < mask.bmWidth; j++) {
				ptx = cinfo.ptScreenPos.x - iinfo.xHotspot + j;
				if(ptx < 0)
					continue;
				if(ptx >= frame->realwidth)
					break;
				if((mcurr[j>>3] & bitmask[j&0x07]) == 0) {
				fcurr[ptx*4+0] = ccurr[j*4+0];
				fcurr[ptx*4+1] = ccurr[j*4+1];
				fcurr[ptx*4+2] = ccurr[j*4+2];
				//fcurr[ptx*4+3] = ccurr[j*4+3];
				} else {
				fcurr[ptx*4+0] ^= ccurr[j*4+0];
				fcurr[ptx*4+1] ^= ccurr[j*4+1];
				fcurr[ptx*4+2] ^= ccurr[j*4+2];
				//fcurr[ptx*4+3] = ccurr[j*4+3];
				}
			}
		}
	}
	ret = 0;
quitFreeIconinfo:
	if(iinfo.hbmMask != NULL)	DeleteObject(iinfo.hbmMask);
	if(iinfo.hbmColor!= NULL)	DeleteObject(iinfo.hbmColor);
quitFreeCursor:
	DestroyCursor(hc);
#else	/* ! WIN32 */
	ret = 0;
#endif
	return ret;
}

