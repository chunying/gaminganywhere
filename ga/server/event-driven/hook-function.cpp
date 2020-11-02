/*
 * Copyright (c) 2012-2015 Chun-Ying Huang
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
#include <assert.h>

#include "ga-common.h"
#include "vsource.h"
#include "dpipe.h"
#include "rtspconf.h"

#include "ga-hook-common.h"
#include "hook-function.h"

#include <GL/glu.h>

// --- GetRawInputData ---
TGetRawInputData pGetRawInputData = NULL;
HANDLE tmpRawKeyDevice = NULL; 
// --- DirectX 9 ---
TDirect3DCreate9 pD3d = NULL;
TD3D9CreateDevice pD3D9CreateDevice = NULL;
TD3D9GetSwapChain pD3D9GetSwapChain = NULL;
TD3D9DevicePresent pD3D9DevicePresent = NULL;
TSwapChainPresent pSwapChainPresent = NULL;
D3DFORMAT pD3DFORMAT = D3DFMT_UNKNOWN;
// --- DirectX 10 / 10.1 ---
TD3D10CreateDeviceAndSwapChain pD3D10CreateDeviceAndSwapChain = NULL;
TD3D10CreateDeviceAndSwapChain1 pD3D10CreateDeviceAndSwapChain1 = NULL;
// --- DirectX 11 ---
TD3D11CreateDeviceAndSwapChain pD3D11CreateDeviceAndSwapChain = NULL;
// --- DXGI ---
TDXGISwapChainPresent pDXGISwapChainPresent = NULL;
DXGI_FORMAT pDXGI_FORMAT = DXGI_FORMAT_UNKNOWN;
TCreateDXGIFactory pCreateDXGIFactory = NULL;
TDXGICreateSwapChain pDXGICreateSwapChain = NULL;
// ------

//////// internal functions

static IDirect3DSurface9 *resolvedSurface = NULL;
static IDirect3DSurface9 *offscreenSurface = NULL;

static bool
D3D9_screen_capture(IDirect3DDevice9 * pDevice) {
	static int frame_interval;
	static LARGE_INTEGER initialTv, captureTv, freq;
	static int capture_initialized = 0;
	//
	HRESULT hr;
	D3DSURFACE_DESC desc;
	IDirect3DSurface9 *renderSurface, *oldRenderSurface;
	D3DLOCKED_RECT lockedRect;
	int i;
	dpipe_buffer_t *data;
	vsource_frame_t *frame;
	//
	if(vsource_initialized == 0)
		return false;
	//
	renderSurface = oldRenderSurface = NULL;
	//
	hr = pDevice->GetRenderTarget(0, &renderSurface);
	if (FAILED(hr)) {
		if (hr == D3DERR_INVALIDCALL) {
			ga_error("GetRenderTarget failed (INVALIDCALL)\n");
		} else if (hr == D3DERR_NOTFOUND) {
			ga_error("GetRenderTarget failed (D3DERR_NOTFOUND)\n");
		} else {
			ga_error("GetRenderTarget failed. (other)\n");
		}
	}
	if (renderSurface == NULL) {
		ga_error("renderSurface == NULL.\n");
		return false;
	}
	
	renderSurface->GetDesc(&desc);

	if(desc.Width != game_width
	|| desc.Height != game_height) {
		return false;
	}

	if (capture_initialized == 0) {
		frame_interval = 1000000/video_fps; // in the unif of us
		frame_interval++;
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&initialTv);
		capture_initialized = 1;
	} else {
		QueryPerformanceCounter(&captureTv);
	}
	
	// check if the surface of local game enable multisampling,
	// multisampling enabled will avoid locking in the surface
	// if yes, create a no-multisampling surface and copy frame into it
	if (desc.MultiSampleType != D3DMULTISAMPLE_NONE) {
		if(resolvedSurface == NULL) {
			hr = pDevice->CreateRenderTarget(game_width, game_height,
					desc.Format,
					D3DMULTISAMPLE_NONE,
					0,			// non multisampleQuality
					FALSE,			// lockable
					&resolvedSurface, NULL);
			if (FAILED(hr)) {
				ga_error("CreateRenderTarget(resolvedSurface) failed.\n");
				return false;
			}
		}

		hr = pDevice->StretchRect(renderSurface, NULL,
					resolvedSurface, NULL, D3DTEXF_NONE);
		if (FAILED(hr)) {
			ga_error("StretchRect failed.\n");
			return false;
		}
		
		oldRenderSurface = renderSurface;
		renderSurface = resolvedSurface;
	}

	// create offline surface in system memory
	if(offscreenSurface == NULL) {
		hr = pDevice->CreateOffscreenPlainSurface(game_width, game_height, 
				desc.Format,
				D3DPOOL_SYSTEMMEM,
				&offscreenSurface, NULL);
		if (FAILED(hr)) {
			ga_error("Create offscreen surface failed.\n");
			return false;
		}
	}
	
	// copy the render-target data from device memory to system memory
	hr = pDevice->GetRenderTargetData(renderSurface, offscreenSurface);

	if (FAILED(hr)) {
		ga_error("GetRenderTargetData failed.\n");
		if(oldRenderSurface)
			oldRenderSurface->Release();
		else
			renderSurface->Release();
		return false;
	}	
	
	if(oldRenderSurface)
		oldRenderSurface->Release();
	else
		renderSurface->Release();

	// start to lock screen from offline surface
	hr = offscreenSurface->LockRect(&lockedRect, NULL, NULL);
	if (FAILED(hr)) {
		ga_error("LockRect failed.\n");
		return false;
	}

	// copy image 
	do {
		unsigned char *src, *dst;
		data = dpipe_get(g_pipe[0]);
		frame = (vsource_frame_t*) data->pointer;
		frame->pixelformat = PIX_FMT_BGRA;
		frame->realwidth = desc.Width;
		frame->realheight = desc.Height;
		frame->realstride = desc.Width<<2;
		frame->realsize = frame->realwidth * frame->realstride;
		frame->linesize[0] = frame->realstride;//frame->stride;
		//
		src = (unsigned char*) lockedRect.pBits;
		dst = (unsigned char*) frame->imgbuf;
		for (i = 0; i < encoder_height; i++) {
			//memcpy(frame->imgbuf+i*encoder_width*sizeof(DWORD), (BYTE *)lockedRect.pBits+i*lockedRect.Pitch, 1*encoder_width*sizeof(DWORD));
			//CopyMemory(frame->imgbuf, lockedRect.pBits, lockedRect.Pitch * screenRect.bottom);
			CopyMemory(dst, src, frame->realstride/*frame->stride*/);
			src += lockedRect.Pitch;
			dst += frame->realstride;//frame->stride;
		}
		frame->imgpts = pcdiff_us(captureTv, initialTv, freq)/frame_interval;
		gettimeofday(&frame->timestamp, NULL);
	} while(0);

	// duplicate from channel 0 to other channels
	for(i = 1; i < SOURCES; i++) {
		int j;
		dpipe_buffer_t *dupdata;
		vsource_frame_t *dupframe;
		dupdata = dpipe_get(g_pipe[i]);
		dupframe = (vsource_frame_t*) dupdata->pointer;
		//
		vsource_dup_frame(frame, dupframe);
		//
		dpipe_store(g_pipe[i], dupdata);
	}
	dpipe_store(g_pipe[0], data);
	
	offscreenSurface->UnlockRect();
#if 1	// XXX: disable until we have found a good place to safely Release()
	if(hook_boost == 0) {
		if(offscreenSurface != NULL) {
			offscreenSurface->Release();
			offscreenSurface = NULL;
		}
		if(resolvedSurface != NULL) {
			resolvedSurface->Release();
			resolvedSurface = NULL;
		}
	}
#endif
		
	return true;
}

static int
D3D9_get_resolution(IDirect3DDevice9 *pDevice) {
	HRESULT hr;
	D3DSURFACE_DESC desc;
	IDirect3DSurface9 *renderSurface;
	static int initialized = 0;
	//
	if(initialized > 0) {
		return 0;
	}
	// get current resolution
	hr = pDevice->GetRenderTarget(0, &renderSurface);
	if(!renderSurface || FAILED(hr))
		return -1;
	renderSurface->GetDesc(&desc);
	renderSurface->Release();
	//
	if(ga_hook_get_resolution(desc.Width, desc.Height) < 0)
		return -1;
	initialized = 1;
	return 0;
}

static int
DXGI_get_resolution(IDXGISwapChain *pSwapChain) {
	DXGI_SWAP_CHAIN_DESC pDESC;
	static int initialized = 0;
	//
	if(initialized > 0) {
		return 0;
	}
	// get current resolution
	pSwapChain->GetDesc(&pDESC);
	//
	if(ga_hook_get_resolution(pDESC.BufferDesc.Width, pDESC.BufferDesc.Height) < 0)
		return -1;
	initialized = 1;
	return 0;
}

//////// hook functions

// Hook function that replaces the Direct3DCreate9() API
DllExport IDirect3D9* WINAPI
hook_d3d(UINT SDKVersion)
{
	static int hooked_d3d9 = 0;
	IDirect3D9 *pDirect3D9 = pD3d(SDKVersion);

	if (hooked_d3d9 > 0)
		return pDirect3D9;

	hooked_d3d9 = 1;

	if (pD3D9CreateDevice == NULL) {
		OutputDebugString("[Direct3DCreate9]");

		uintptr_t* pInterfaceVTable = (uintptr_t*)*(uintptr_t*)pDirect3D9;
		pD3D9CreateDevice = (TD3D9CreateDevice) pInterfaceVTable[16];   // IDirect3D9::CreateDevice()
		ga_hook_function("IDirect3D9::CreateDevice", pD3D9CreateDevice, hook_D3D9CreateDevice);
	}

	return pDirect3D9;
}

// Hook function that replaces the IDirect3D9::CreateDevice() API
DllExport HRESULT __stdcall
hook_D3D9CreateDevice(
		IDirect3DDevice9 * This,
		UINT Adapter,
		D3DDEVTYPE DeviceType,
		HWND hFocusWindow,
		DWORD BehaviorFlags,
		D3DPRESENT_PARAMETERS *pPresentationParameters,
		IDirect3DDevice9 **ppReturnedDeviceInterface
	)
{
	static int createdevice_hooked = 0;

	HRESULT hr = pD3D9CreateDevice(This, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
	
	if(createdevice_hooked > 0)
		return hr;

	if(FAILED(hr))
		return hr;

	if (pD3D9DevicePresent == NULL) {
		uintptr_t* pInterfaceVTable = (uintptr_t*)*(uintptr_t*)*ppReturnedDeviceInterface;

		OutputDebugString("[IDirect3D9::CreateDevice()]");

		// 14: IDirect3DDevice9::GetSwapChain,  17: IDirect3DDevice9::Present
		// 41: IDirect3DDevice9::BeginScene,    42: IDirect3DDevice9::EndScene
		pD3D9GetSwapChain = (TD3D9GetSwapChain)pInterfaceVTable[14];
		pD3D9DevicePresent = (TD3D9DevicePresent)pInterfaceVTable[17];     

		ga_hook_function("IDirect3DDevice9::GetSwapChain", pD3D9GetSwapChain, hook_D3D9GetSwapChain);
		ga_hook_function("IDirect3DDevice9::Present", pD3D9DevicePresent, hook_D3D9DevicePresent);
	}

	createdevice_hooked = 1;

	return hr;
}

// Hook function that replaces the IDirect3dDevice9::GetSwapChain() API
DllExport HRESULT __stdcall hook_D3D9GetSwapChain(
		IDirect3DDevice9 *This,
		UINT iSwapChain,
		IDirect3DSwapChain9 **ppSwapChain
	)
{
	static int getswapchain_hooked = 0;

	HRESULT hr = pD3D9GetSwapChain(This, iSwapChain, ppSwapChain);
	
	if (getswapchain_hooked > 0)
		return hr;

	getswapchain_hooked = 1;

	if (ppSwapChain != NULL && pSwapChainPresent == NULL) {
		OutputDebugString("[IDirect3dDevice9::GetSwapChain]");

		IDirect3DSwapChain9 *pIDirect3DSwapChain9 = *ppSwapChain;
		uintptr_t* pInterfaceVTable = (uintptr_t*)*(uintptr_t*)pIDirect3DSwapChain9;  // IDirect3dSwapChain9
		uintptr_t* ppSwapChainPresent = (uintptr_t*)pInterfaceVTable[3];   // IDirect3DSwapChain9::Present
		pSwapChainPresent = (TSwapChainPresent) ppSwapChainPresent;
		ga_hook_function("IDirect3DSwapChain9::Present", pSwapChainPresent, hook_D3D9SwapChainPresent);
	}

	return hr;
}

// Hook function that replaces the IDirect3dSwapChain9::Present() API
DllExport HRESULT __stdcall hook_D3D9SwapChainPresent(
		IDirect3DSwapChain9 * This,
		CONST RECT* pSourceRect,
		CONST RECT* pDestRect,
		HWND hDestWindowOverride,
		CONST RGNDATA* pDirtyRegion,
		DWORD dwFlags
	)
{
	static int present_hooked = 0;
	IDirect3DDevice9 *pDevice;

	if (present_hooked == 0) {
		OutputDebugString("[IDirect3dSwapChain9::Present()]");
		present_hooked = 1;
	}

	HRESULT hr = pSwapChainPresent(This, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);

	This->GetDevice(&pDevice);

	if(resolution_retrieved == 0) {
		if(D3D9_get_resolution(pDevice) >= 0) {
			resolution_retrieved = 1;
		}
		return hr;
	}

	if (enable_server_rate_control) {
		if(ga_hook_video_rate_control() > 0)
			D3D9_screen_capture(pDevice);
	} else {
		D3D9_screen_capture(pDevice);
	}

	pDevice->Release();
	return hr;
}


// Hook function that replaces the IDirect3dDevice9::Present() API
DllExport HRESULT __stdcall hook_D3D9DevicePresent(
		IDirect3DDevice9 * This,
		CONST RECT* pSourceRect,
		CONST RECT* pDestRect,
		HWND hDestWindowOverride,
		CONST RGNDATA* pDirtyRegion
	)
{
	static int present_hooked = 0;

	if (present_hooked == 0) {
		OutputDebugString("[IDirect3dDevice9::Present()]");
		present_hooked = 1;
	}
	
	HRESULT hr = pD3D9DevicePresent(This, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);	
	
	if(resolution_retrieved == 0) {
		if(D3D9_get_resolution(This) >= 0) {
			resolution_retrieved = 1;
		}
		return hr;
	}
			
	// rate controller
	if (enable_server_rate_control) {
		if(ga_hook_video_rate_control() > 0)
			D3D9_screen_capture(This);
	} else {
		D3D9_screen_capture(This);
	}

	return hr;
}

#if 1
enum DX_VERSION {
	dx_none = 0,
	dx_9,
	dx_10,
	dx_10_1,
	dx_11
};

static enum DX_VERSION dx_version = dx_none;

void
proc_hook_IDXGISwapChain_Present(IDXGISwapChain *ppSwapChain) 
{
	uintptr_t *pInterfaceVTable = (uintptr_t *)*(uintptr_t *)ppSwapChain;   // IDXGISwapChain

	pDXGISwapChainPresent = (TDXGISwapChainPresent)pInterfaceVTable[8];   // IDXGISwapChain::Present()

	ga_hook_function("IDXGISwapChain::Present", pDXGISwapChainPresent, hook_DXGISwapChainPresent);
}


// Hook function that replaces the CreateDXGIFactory() API
DllExport HRESULT __stdcall
hook_CreateDXGIFactory( REFIID riid, void **ppFactory) {
	//
	HRESULT hr = pCreateDXGIFactory(riid, ppFactory);

	if (pDXGICreateSwapChain == NULL && riid == IID_IDXGIFactory && ppFactory != NULL) {
		uintptr_t* pInterfaceVTable = (uintptr_t*)*(uintptr_t*)*ppFactory;
		//OutputDebugString("[CreateDXGIFactory]");
		pDXGICreateSwapChain = (TDXGICreateSwapChain)pInterfaceVTable[10];   // 10: IDXGIFactory::CreateSwapChain  
		ga_hook_function("IDXGIFactory::CreateSwapChain", pDXGICreateSwapChain, hook_DXGICreateSwapChain);
	}

	return hr;
}

// Hook function that replaces the IDXGIFactory::CreateSwapChain() API
DllExport HRESULT __stdcall
hook_DXGICreateSwapChain(
		IDXGIFactory *This,
		IUnknown *pDevice,
		DXGI_SWAP_CHAIN_DESC *pDesc,
		IDXGISwapChain **ppSwapChain
	)
{
	HRESULT hr = pDXGICreateSwapChain(This, pDevice, pDesc, ppSwapChain);
	
	if (pDXGISwapChainPresent == NULL && pDevice != NULL && ppSwapChain != NULL) {
		//OutputDebugString("[IDXGIFactory::CreateSwapChain()]");
		proc_hook_IDXGISwapChain_Present(*ppSwapChain);
	}

	return hr;
}

// Hook function that replaces the D3D10CreateDeviceAndSwapChain() API
DllExport HRESULT __stdcall
hook_D3D10CreateDeviceAndSwapChain(
		IDXGIAdapter *pAdapter,
		D3D10_DRIVER_TYPE DriverType,
		HMODULE Software,
		UINT Flags,
		UINT SDKVersion,
		DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
		IDXGISwapChain **ppSwapChain,
		ID3D10Device **ppDevice
	)
{
	HRESULT hr = pD3D10CreateDeviceAndSwapChain(pAdapter, DriverType, Software, 
			Flags, SDKVersion, pSwapChainDesc, 
			ppSwapChain, ppDevice);

	if (pDXGISwapChainPresent == NULL && pAdapter != NULL && ppSwapChain != NULL && ppDevice != NULL) {
		//OutputDebugString("[D3D10CreateDeviceAndSwapChain]");
		proc_hook_IDXGISwapChain_Present(*ppSwapChain);
	}
	
	return hr;
}

// Hook function that replaces the D3D10CreateDeviceAndSwapChain1() API
DllExport HRESULT __stdcall
hook_D3D10CreateDeviceAndSwapChain1(
		IDXGIAdapter *pAdapter,
		D3D10_DRIVER_TYPE DriverType,
		HMODULE Software,
		UINT Flags,
		D3D10_FEATURE_LEVEL1 HardwareLevel,
		UINT SDKVersion,
		DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
		IDXGISwapChain **ppSwapChain,
		ID3D10Device1 **ppDevice
	)
{
	HRESULT hr = pD3D10CreateDeviceAndSwapChain1(pAdapter, DriverType, Software, 
				Flags, HardwareLevel, SDKVersion, 
				pSwapChainDesc, ppSwapChain, ppDevice);

	if (pDXGISwapChainPresent == NULL && pAdapter != NULL && ppSwapChain != NULL && ppDevice != NULL) {
		//OutputDebugString("[D3D10CreateDeviceAndSwapChain1]");
		proc_hook_IDXGISwapChain_Present(*ppSwapChain);
	}
	return hr;
}

// Hook function that replaces the D3D11CreateDeviceAndSwapChain() API
DllExport HRESULT __stdcall
hook_D3D11CreateDeviceAndSwapChain(
	IDXGIAdapter *pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	const D3D_FEATURE_LEVEL *pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	const DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
	IDXGISwapChain **ppSwapChain,
	ID3D11Device **ppDevice,
	D3D_FEATURE_LEVEL *pFeatureLevel,
	ID3D11DeviceContext **ppImmediateContext
	)
{
	HRESULT hr = pD3D11CreateDeviceAndSwapChain(pAdapter, DriverType, Software, Flags, 
				pFeatureLevels, FeatureLevels, SDKVersion, pSwapChainDesc, 
				ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);

	if (pDXGISwapChainPresent == NULL && pAdapter != NULL && ppSwapChain != NULL && ppDevice != NULL) {
		//OutputDebugString("[D3D11CreateDeviceAndSwapChain]");
		proc_hook_IDXGISwapChain_Present(*ppSwapChain);
	}
	
	return hr;
}

bool
check_dx_device_version(IDXGISwapChain * This, const GUID IID_target) {
	IUnknown *pDevice = NULL;
	HRESULT hr;

	if (dx_version != dx_none)
		return FALSE;  // this device has been checked

	hr = This->GetDevice(IID_target, (void**)&pDevice);
	if (FAILED(hr) || pDevice == NULL) {  // failed to get this device 
		pDevice->Release();
		return FALSE; 
	}

	pDevice->Release();
	return TRUE;
}

// Hook function that replaces the IDXGISwapChain::Present() API
DllExport HRESULT __stdcall
hook_DXGISwapChainPresent(
		IDXGISwapChain * This,
		UINT SyncInterval,
		UINT Flags
	)
{
	static int frame_interval;
	static LARGE_INTEGER initialTv, captureTv, freq;
	static int capture_initialized = 0;
	//
	int i;
	dpipe_buffer_t *data;
	vsource_frame_t *frame;
	//
	DXGI_SWAP_CHAIN_DESC pDESC;
	HRESULT hr = pDXGISwapChainPresent(This, SyncInterval, Flags);	
		
	if(resolution_retrieved == 0) {
		if(DXGI_get_resolution(This) >= 0) {
			resolution_retrieved = 1;
		}
		return hr;
	}
	
	if(vsource_initialized == 0) {
		ga_error("video source not initialized.\n");
		return hr;
	}
	
	This->GetDesc(&pDESC);
	pDXGI_FORMAT = pDESC.BufferDesc.Format;   // extract screen format for sws_scale
	
	if(pDESC.BufferDesc.Width != game_width
	|| pDESC.BufferDesc.Height != game_height) {
		ga_error("game width/height mismatched (%dx%d) != (%dx%d)\n",
			pDESC.BufferDesc.Width, pDESC.BufferDesc.Height,
			game_width, game_height);
		return hr;
	}
	
	//
	if (enable_server_rate_control && ga_hook_video_rate_control() < 0)
		return hr;
	
	if (dx_version == dx_none) {
		//bool check_result = FALSE;
		if (check_dx_device_version(This, IID_ID3D10Device)) {
			dx_version = dx_10;
			ga_error("[DXGISwapChain] DirectX 10\n");
		} else if (check_dx_device_version(This, IID_ID3D10Device1)) {
			dx_version = dx_10_1;
			ga_error("[DXGISwapChain] DirectX 10.1\n");
		} else if (check_dx_device_version(This, IID_ID3D11Device)) {
			dx_version = dx_11;
			ga_error("[DXGISwapChain] DirectX 11\n");
		}
	}
	
	if (capture_initialized == 0) {
		frame_interval = 1000000/video_fps; // in the unif of us
		frame_interval++;
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&initialTv);
		capture_initialized = 1;
	} else {
		QueryPerformanceCounter(&captureTv);
	}

	hr = 0;

	// d3d10 / d3d10.1
	if (dx_version == dx_10 || dx_version == dx_10_1) {
		
		void *ppDevice;	
		ID3D10Device *pDevice;
		//IUnknown *pDevice;

		if (dx_version == dx_10) {
			This->GetDevice(IID_ID3D10Device, &ppDevice);
			pDevice = (ID3D10Device *)ppDevice;
		} else if (dx_version == dx_10_1) {
			This->GetDevice(IID_ID3D10Device1, &ppDevice);
			pDevice = (ID3D10Device1 *)ppDevice;
		} else {
			OutputDebugString("Invalid DirectX version in IDXGISwapChain::Present");
			return hr;
		}

		ID3D10RenderTargetView *pRTV = NULL;
		ID3D10Resource *pSrcResource = NULL;
		pDevice->OMGetRenderTargets(1, &pRTV, NULL);
		pRTV->GetResource(&pSrcResource);

		ID3D10Texture2D* pSrcBuffer = (ID3D10Texture2D *)pSrcResource;
		ID3D10Texture2D* pDstBuffer = NULL;

		D3D10_TEXTURE2D_DESC desc;
		pSrcBuffer->GetDesc(&desc);
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
		desc.Usage = D3D10_USAGE_STAGING;

		hr = pDevice->CreateTexture2D(&desc, NULL, &pDstBuffer);
		if (FAILED(hr)) {
			OutputDebugString("Failed to create texture2D");
			//assert(exp_state == exp_none);
		}

		pDevice->CopyResource(pDstBuffer, pSrcBuffer);

		D3D10_MAPPED_TEXTURE2D mapped_screen;
		hr = pDstBuffer->Map(0, D3D10_MAP_READ, 0, &mapped_screen);
		if (FAILED(hr)) {
			OutputDebugString("Failed to map from DstBuffer");
			//assert(exp_state == exp_none);
		}

		// copy image 
		do {
			unsigned char *src, *dst;
			data = dpipe_get(g_pipe[0]);
			frame = (vsource_frame_t*) data->pointer;
			frame->pixelformat = PIX_FMT_BGRA;
			frame->realwidth = desc.Width;
			frame->realheight = desc.Height;
			frame->realstride = desc.Width<<2;
			frame->realsize = frame->realwidth * frame->realstride;
			frame->linesize[0] = frame->realstride;//frame->stride;
			//
			src = (unsigned char*) mapped_screen.pData;
			dst = (unsigned char*) frame->imgbuf;
			for (i = 0; i < encoder_height; i++) {				
				CopyMemory(dst, src, frame->realstride/*frame->stride*/);
				src += mapped_screen.RowPitch;
				dst += frame->realstride;//frame->stride;
			}
			frame->imgpts = pcdiff_us(captureTv, initialTv, freq)/frame_interval;
			gettimeofday(&frame->timestamp, NULL);
		} while(0);
	
		// duplicate from channel 0 to other channels
		for(i = 1; i < SOURCES; i++) {
			int j;
			dpipe_buffer_t *dupdata;
			vsource_frame_t *dupframe;
			dupdata = dpipe_get(g_pipe[i]);
			dupframe = (vsource_frame_t*) dupdata->pointer;
			//
			vsource_dup_frame(frame, dupframe);
			//
			dpipe_store(g_pipe[i], dupdata);
		}
		dpipe_store(g_pipe[0], data);
		
		pDstBuffer->Unmap(0);

		pDevice->Release();
		pSrcResource->Release();
		pSrcBuffer->Release();
		pRTV->Release();
		pDstBuffer->Release();

	// d11
	} else if (dx_version == dx_11) {
		void *ppDevice;	
		This->GetDevice(IID_ID3D11Device, &ppDevice);
		ID3D11Device *pDevice = (ID3D11Device*) ppDevice;

		This->GetDevice(IID_ID3D11DeviceContext, &ppDevice);
		ID3D11DeviceContext *pDeviceContext = (ID3D11DeviceContext *) ppDevice;
		
		ID3D11RenderTargetView *pRTV = NULL;
		ID3D11Resource *pSrcResource = NULL;
		pDeviceContext->OMGetRenderTargets(1, &pRTV, NULL);
		pRTV->GetResource(&pSrcResource);
	
		ID3D11Texture2D *pSrcBuffer = (ID3D11Texture2D *)pSrcResource;
		ID3D11Texture2D *pDstBuffer = NULL;
		
		D3D11_TEXTURE2D_DESC desc;
		pSrcBuffer->GetDesc(&desc);
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.Usage = D3D11_USAGE_STAGING;

		hr = pDevice->CreateTexture2D(&desc, NULL, &pDstBuffer);
		if (FAILED(hr)) {
			OutputDebugString("Failed to create buffer");
			//assert(exp_state == exp_none);
		}
		pDeviceContext->CopyResource(pDstBuffer, pSrcBuffer);

		D3D11_MAPPED_SUBRESOURCE mapped_screen;
		hr = pDeviceContext->Map(pDstBuffer, 0, D3D11_MAP_READ, 0, &mapped_screen);
		if (FAILED(hr)) {
			OutputDebugString("Failed to map from DeviceContext");
			//assert(exp_state == exp_none);
		}
		
		// copy image 
		do {
			unsigned char *src, *dst;
			data = dpipe_get(g_pipe[0]);
			frame = (vsource_frame_t*) data->pointer;
			frame->pixelformat = PIX_FMT_BGRA;
			frame->realwidth = desc.Width;
			frame->realheight = desc.Height;
			frame->realstride = desc.Width<<2;
			frame->realsize = frame->realwidth * frame->realstride;
			frame->linesize[0] = frame->realstride;//frame->stride;
			//
			src = (unsigned char*) mapped_screen.pData;
			dst = (unsigned char*) frame->imgbuf;
			for (i = 0; i < encoder_height; i++) {				
				CopyMemory(dst, src, frame->realstride/*frame->stride*/);
				src += mapped_screen.RowPitch;
				dst += frame->realstride;//frame->stride;
			}
			frame->imgpts = pcdiff_us(captureTv, initialTv, freq)/frame_interval;
			gettimeofday(&frame->timestamp, NULL);
		} while(0);
	
		// duplicate from channel 0 to other channels
		for(i = 1; i < SOURCES; i++) {
			int j;
			dpipe_buffer_t *dupdata;
			vsource_frame_t *dupframe;
			dupdata = dpipe_get(g_pipe[i]);
			dupframe = (vsource_frame_t*) dupdata->pointer;
			//
			vsource_dup_frame(frame, dupframe);
			//
			dpipe_store(g_pipe[i], dupdata);
		}
		dpipe_store(g_pipe[0], data);

		pDeviceContext->Unmap(pDstBuffer, 0);

		pDevice->Release();
		pDeviceContext->Release();
		pSrcResource->Release();
		pSrcBuffer->Release();
		pRTV->Release();
		pDstBuffer->Release();
	}

	return hr;
}
#endif

void GetRawInputDevice(bool is_mame_game)
{
	UINT nDevices;
	PRAWINPUTDEVICELIST pRawInputDeviceList;

	char output[256];

	if (GetRawInputDeviceList(NULL, &nDevices, sizeof(RAWINPUTDEVICELIST)) != 0) { 
		ga_error("GetRawInputDeviceList failed.\n");
		exit(-1);
	}

	if ((pRawInputDeviceList = (PRAWINPUTDEVICELIST) malloc(sizeof(RAWINPUTDEVICELIST) * nDevices)) == NULL) {
		ga_error("Memory allocation for RawInputDeviceList failed.\n");
		exit(-1);
	}

	nDevices = GetRawInputDeviceList(pRawInputDeviceList, &nDevices, sizeof(RAWINPUTDEVICELIST));
	if (nDevices <= 0) {
		ga_error("GetRawInputDeviceList failed.\n");
		exit(-1);
	}
	ga_error("[raw input device] number of devices: %d\n", nDevices);

	int device_i = 0; 

//#define SHOW_DEVICE_MSG

	int j;
	RID_DEVICE_INFO rdi;
	rdi.cbSize = sizeof(RID_DEVICE_INFO);
	for (j = 0; j < nDevices; j++) {
		UINT nBufferSize = 256;
		char tBuffer[256] = {0};
		if (GetRawInputDeviceInfo(pRawInputDeviceList[j].hDevice,
							RIDI_DEVICENAME,
							tBuffer,
							&nBufferSize) < 0) {
			ga_error("[device %d] Unable to get device name\n", j);
			continue;
		}
#ifdef SHOW_DEVICE_MSG
		ga_error("[device %d] device name: %s\n", j, tBuffer);
#endif
		//
		
		UINT cbSize = rdi.cbSize;
		if (GetRawInputDeviceInfo(pRawInputDeviceList[j].hDevice, 
							RIDI_DEVICEINFO,
							&rdi,
							&cbSize) < 0) {
			ga_error("[device %d] Unable to get device info\n", j);
			continue;
		}
		if (rdi.dwType == RIM_TYPEMOUSE) {
#ifdef SHOW_DEVICE_MSG
			ga_error("[device %d][Mouse] id: %ld, number of buttons: %ld, sample rate: %ld\n", 
				j, 
				rdi.mouse.dwId, 
				rdi.mouse.dwNumberOfButtons, 
				rdi.mouse.dwSampleRate);
#endif
		} else if (rdi.dwType == RIM_TYPEKEYBOARD) {
#ifdef SHOW_DEVICE_MSG
			ga_error("[device %d][keyboard] mode %ld, number of function keys: %ld, indicators: %ld, keys total: %ld, type: %ld, subtype: %ld\n", 
				j, 
				rdi.keyboard.dwKeyboardMode, 
				rdi.keyboard.dwNumberOfFunctionKeys,
				rdi.keyboard.dwNumberOfIndicators,
				rdi.keyboard.dwNumberOfKeysTotal,
				rdi.keyboard.dwType,
				rdi.keyboard.dwSubType);
#endif
			//char *str_loc; 
			if (strstr(tBuffer, "HID") != NULL) {
				device_i = j;
				tmpRawKeyDevice = pRawInputDeviceList[j].hDevice;
				ga_error("[raw input device] keyboard device is found!\n");
				break;
			}
		} else {
#ifdef SHOW_DEVICE_MSG
			ga_error("[device %d][hid] vender id: %ld, product id: %ld, version no: %ld, usage: %ld, usage page: %ld\n", 
				j, 
				rdi.hid.dwVendorId,
				rdi.hid.dwProductId,
				rdi.hid.dwVersionNumber,
				rdi.hid.usUsage,
				rdi.hid.usUsagePage);
#endif
		}
	}

	//int device_i = 0; 
	if (is_mame_game) {
		ga_error("[RAW INPUT] GetRawInputDevice: MAME device!!");
		//device_i = 3;
	}
	//RAWINPUTDEVICELIST *device = &pRawInputDeviceList[device_i];
	//tmpRawKeyDevice = device->hDevice;
   
	snprintf(output, sizeof(output), "[GetRawInputDevice] keyDevice: %u (%d)", tmpRawKeyDevice, device_i);
	OutputDebugString(output);

	free(pRawInputDeviceList);

	return;
}

// Hook function that replaces the GetRawInputData API
DllExport HRESULT __stdcall hook_GetRawInputData(
		HRAWINPUT hRawInput,
		UINT uiCommand,
		LPVOID pData,
		PUINT pcbSize,
		UINT cbSizeHeader
	)
{
	HRESULT hr;
	hr = pGetRawInputData(hRawInput, uiCommand, pData, pcbSize, cbSizeHeader);

	if (uiCommand == RID_INPUT && pData != NULL) {
		RAWINPUT *input = (RAWINPUT *)pData;

		// keyboard key
		if (input->data.keyboard.VKey != NULL) { 
			if (input->header.hDevice == 0) { // SendInput
				input->header.hDevice = tmpRawKeyDevice;
				//
				// left 37, up 38, right 39, down 40
				// [hook message from Sendinput] down: 0 -> 2, up: 1 -> 3
				// if sendinput send Rcontrol, Ralt
				// rawinput will get vk_control(17), down:2, up:3
				// if sendinput send Lcontrol, Lalt
				// rawinput will get vk_control(17), down:0, up:1
				if (input->data.keyboard.VKey == 17 ||
					input->data.keyboard.VKey == 18 ||
					input->data.keyboard.VKey == 37 ||
					input->data.keyboard.VKey == 38 ||
					input->data.keyboard.VKey == 39 ||
					input->data.keyboard.VKey == 40
				   ) {
					if (input->data.keyboard.Flags == 0)
						input->data.keyboard.Flags = 2;
					else if (input->data.keyboard.Flags == 1)
						input->data.keyboard.Flags = 3;
				} else if (input->data.keyboard.VKey == VK_PRETENDED_LCONTROL) {
					input->data.keyboard.VKey = 17;
				} else if (input->data.keyboard.VKey == VK_PRETENDED_LALT) {
					input->data.keyboard.VKey = 18;
				}
			}

		}
	}
	return hr;
}
/////////////////////////////////////////////////////////////////////

