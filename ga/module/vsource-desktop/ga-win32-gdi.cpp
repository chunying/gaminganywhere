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

#include "ga-common.h"
#include "ga-win32-gdi.h"

#define	BITSPERPIXEL	32

static BITMAPINFO	bmpInfo;
static RECT		screenRect;
static int		frameSize;
static HWND		hDesktopWnd;
static HDC		hDesktopDC;
static HDC		hDesktopCompatibleDC;
static HBITMAP		hDesktopCompatibleBitmap;
static LPVOID		pBits;

int
ga_win32_GDI_init(struct gaImage *image) {
	ga_win32_fill_bitmap_info(
		&bmpInfo,
		GetSystemMetrics(SM_CXSCREEN),
		GetSystemMetrics(SM_CYSCREEN),
		BITSPERPIXEL);
	frameSize = bmpInfo.bmiHeader.biSizeImage;
	ZeroMemory(&screenRect, sizeof(screenRect));
	screenRect.right = bmpInfo.bmiHeader.biWidth-1;
	screenRect.bottom = bmpInfo.bmiHeader.biHeight-1;
	//
	image->width = bmpInfo.bmiHeader.biWidth;
	image->height = bmpInfo.bmiHeader.biHeight;
	image->bytes_per_line = (BITSPERPIXEL>>3) * image->width;
	//
	hDesktopWnd = GetDesktopWindow();
	hDesktopDC = GetDC(hDesktopWnd);
	hDesktopCompatibleDC = CreateCompatibleDC(hDesktopDC);
	hDesktopCompatibleBitmap = CreateDIBSection(hDesktopDC ,&bmpInfo, DIB_RGB_COLORS, &pBits, NULL,0);
	//
	if(hDesktopCompatibleDC==NULL || hDesktopCompatibleBitmap == NULL) {
		ga_error("Unable to Create Desktop Compatible DC/Bitmap\n");
		return -1;
	}
	//
	SelectObject(hDesktopCompatibleDC, hDesktopCompatibleBitmap);
	//
	return 0;
}

void
ga_win32_GDI_deinit() {
	if(hDesktopCompatibleBitmap != NULL) {
		DeleteObject(hDesktopCompatibleBitmap);
		hDesktopCompatibleBitmap = NULL;
		pBits = NULL;
	}
	if(hDesktopCompatibleDC != NULL) {
		DeleteDC(hDesktopCompatibleDC);
		hDesktopCompatibleDC = NULL;
	}
	if(hDesktopDC != NULL) {
		ReleaseDC(hDesktopWnd, hDesktopDC);
		hDesktopDC = NULL;
	}
	return;
}

int
ga_win32_GDI_capture(char *buf, int buflen, struct gaRect *grect) {
	int linesize, height;
	char *source, *dest;
	if(grect==NULL && buflen < frameSize)
		return -1;
	if(grect!=NULL && buflen < grect->size)
		return -1;
	if(hDesktopCompatibleDC==NULL || pBits == NULL)
		return -1;
	BitBlt(hDesktopCompatibleDC, 0, 0,
		screenRect.right+1, screenRect.bottom+1,
		hDesktopDC, 0, 0, SRCCOPY|CAPTUREBLT);

	// XXX: images are stored upside-down
	linesize = (BITSPERPIXEL>>3) * (screenRect.right+1);
	//
	if(grect == NULL) {
		source = ((char*) pBits) + (linesize * screenRect.bottom);
		dest = buf;
		height = screenRect.bottom+1;
		while(height-- > 0) {
			CopyMemory(dest, source, linesize);
			dest += linesize;
			source -= linesize;
		}
	} else {
		source = (char*) pBits;
		source += linesize * (screenRect.bottom - grect->top);
		source += RGBA_SIZE * grect->left;
		dest = buf;
		height = grect->height;
		while(height-- > 0) {
			CopyMemory(dest, source, grect->linesize);
			dest += grect->linesize;
			source -= linesize;
		}
	}

	return grect==NULL ? frameSize : grect->size;
}

