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
#include "ga-win32-d3d.h"

#include <d3d9.h>
#include <d3dx9tex.h>

#define	BITSPERPIXEL	32

#define	D3D_WINDOW_MODE	true	// not in full screen mode

static IDirect3D9*		g_pD3D;
static IDirect3DDevice9*	g_pd3dDevice;
static IDirect3DSurface9*	g_pSurface;
static BITMAPINFO		bmpInfo;
static RECT			screenRect;
static int			frameSize;
static HWND			captureHwnd;

static void
MakeBitmapInfo(BITMAPINFO *pInfo, int w, int h, int bitsPerPixel) {
	ZeroMemory(pInfo, sizeof(BITMAPINFO));
	pInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pInfo->bmiHeader.biBitCount = bitsPerPixel;
	pInfo->bmiHeader.biCompression = BI_RGB;
	pInfo->bmiHeader.biWidth = w;
	pInfo->bmiHeader.biHeight = h;
	pInfo->bmiHeader.biPlanes = 1; // must be 1
	pInfo->bmiHeader.biSizeImage = pInfo->bmiHeader.biHeight
		* pInfo->bmiHeader.biWidth
		* pInfo->bmiHeader.biBitCount/8;
	return;
}

int
ga_win32_D3D_init(struct gaImage *image) {
	D3DDISPLAYMODE		ddm;
	D3DPRESENT_PARAMETERS	d3dpp;
	//
	captureHwnd = GetDesktopWindow();
	//
	ZeroMemory(&screenRect, sizeof(screenRect));
	ga_win32_fill_bitmap_info(
		&bmpInfo,
		GetSystemMetrics(SM_CXSCREEN),
		GetSystemMetrics(SM_CYSCREEN), 
		BITSPERPIXEL);
	frameSize = bmpInfo.bmiHeader.biSizeImage;
	//
	image->width = bmpInfo.bmiHeader.biWidth;
	image->height = bmpInfo.bmiHeader.biHeight;
	image->bytes_per_line = (BITSPERPIXEL>>3) * image->width;
	//
	if((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION))==NULL)
	{
		ga_error("Unable to Create Direct3D\n");
		return -1;
	}

	if(FAILED(g_pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT,&ddm)))
	{
		ga_error("Unable to Get Adapter Display Mode\n");
		goto initErrorQuit;
	}

	ZeroMemory(&d3dpp, sizeof(D3DPRESENT_PARAMETERS));

	d3dpp.Windowed = D3D_WINDOW_MODE;
	d3dpp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
	d3dpp.BackBufferFormat = ddm.Format;
	d3dpp.BackBufferHeight = screenRect.bottom = ddm.Height;
	d3dpp.BackBufferWidth = screenRect.right = ddm.Width;
	d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
	d3dpp.SwapEffect = D3DSWAPEFFECT_COPY; //DISCARD;
	d3dpp.hDeviceWindow = captureHwnd /*NULL*/ /*hWnd*/;
	d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
	d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;

	MakeBitmapInfo(&bmpInfo, ddm.Width, ddm.Height, BITSPERPIXEL);
	frameSize = bmpInfo.bmiHeader.biSizeImage;

	if(FAILED(g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, captureHwnd /*NULL*/ /*hWnd*/,
		D3DCREATE_SOFTWARE_VERTEXPROCESSING ,&d3dpp, &g_pd3dDevice)))
	{
		ga_error("Unable to Create Device\n");
		goto initErrorQuit;
	}
	if(FAILED(g_pd3dDevice->CreateOffscreenPlainSurface(ddm.Width, ddm.Height,
		D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM/*D3DPOOL_SCRATCH*/, &g_pSurface, NULL)))
	{
		ga_error("Unable to Create Surface\n");
		goto initErrorQuit;
	}

	return 0;
	//
initErrorQuit:
	ga_win32_D3D_deinit();
	return -1;
}

void
ga_win32_D3D_deinit() {
	if(g_pSurface) {
		g_pSurface->Release();
		g_pSurface = NULL;
	}
	if(g_pd3dDevice) {
		g_pd3dDevice->Release();
		g_pd3dDevice = NULL;
	}
	if(g_pD3D) {
		g_pD3D->Release();
		g_pD3D = NULL;
	}
	return;
}

int
ga_win32_D3D_capture(char *buf, int buflen, struct gaRect *grect) {
	D3DLOCKED_RECT lockedRect;
	if(grect == NULL && buflen < frameSize)
		return -1;
	if(grect != NULL && buflen < grect->size)
		return -1;
	if(g_pd3dDevice == NULL || g_pSurface == NULL)
		return -1;
	// get front buffer
	g_pd3dDevice->GetFrontBufferData(0, g_pSurface);
	// lock
	if(FAILED(g_pSurface->LockRect(&lockedRect, &screenRect,
		D3DLOCK_NO_DIRTY_UPDATE|D3DLOCK_NOSYSLOCK|D3DLOCK_READONLY)))
	{
		ga_error("Unable to Lock Front Buffer Surface\n");
		return -1;
	}
	// copy
	if(grect == NULL) {
		CopyMemory(buf, lockedRect.pBits, lockedRect.Pitch * screenRect.bottom);
	} else {
		int i;
		char *src, *dst;
		src = (char *) lockedRect.pBits;
		src += lockedRect.Pitch * grect->top;
		src += RGBA_SIZE * grect->left;
		dst = (char*) buf;
		//
		for(i = 0; i < grect->height; i++) {
			CopyMemory(dst, src, grect->linesize);
			src += lockedRect.Pitch;
			dst += grect->linesize;
		}
	}
	// Unlock
	g_pSurface->UnlockRect();

	return grect == NULL ? frameSize : grect->size;
}

