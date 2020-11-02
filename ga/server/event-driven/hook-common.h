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

#ifndef __HOOK_COMMON_H__
#define __HOOK_COMMON_H__

#include <stdio.h>
#include <windows.h>

#include <d3d9.h>
#include <d3dx9.h>
#include <d3d10_1.h>
#include <d3d11.h>
#include <d3dx9tex.h>
#include <dxerr.h>

#define DllImport   __declspec( dllimport )
#define DllExport   __declspec( dllexport )

typedef HRESULT (STDMETHODCALLTYPE *TGetRawInputData) (
	HRAWINPUT hRawInput,
	UINT uiCommand,
	LPVOID pData,
	PUINT pcbSize,
	UINT cbSizeHeader
);

// --- Common Release ---
typedef ULONG (STDMETHODCALLTYPE *TRelease) (IUnknown *comobj);
// --- DirectX 9 ---
typedef IDirect3D9* (STDMETHODCALLTYPE *TDirect3DCreate9)(
	UINT SDKVersion
);

typedef  HRESULT (STDMETHODCALLTYPE *TD3D9CreateDevice)( 
	IDirect3DDevice9 * This,
	UINT Adapter,
	D3DDEVTYPE DeviceType,
	HWND hFocusWindow,
	DWORD BehaviorFlags,
	D3DPRESENT_PARAMETERS *pPresentationParameters,
	IDirect3DDevice9 **ppReturnedDeviceInterface
);

typedef  HRESULT (STDMETHODCALLTYPE *TD3D9DevicePresent)( 
	IDirect3DDevice9 * This,
	CONST RECT* pSourceRect,
	CONST RECT* pDestRect,
	HWND hDestWindowOverride,
	CONST RGNDATA* pDirtyRegion
);

typedef  HRESULT (STDMETHODCALLTYPE *TSwapChainPresent)( 
	IDirect3DSwapChain9 * This,
	CONST RECT* pSourceRect,
	CONST RECT* pDestRect,
	HWND hDestWindowOverride,
	CONST RGNDATA* pDirtyRegion,
	DWORD dwFlags
);

typedef HRESULT (STDMETHODCALLTYPE *TD3D9GetSwapChain)(
	IDirect3DDevice9 *This,
	UINT iSwapChain,
	IDirect3DSwapChain9 **ppSwapChain
);
// ------

#if 1
// --- DXGI ---
typedef  HRESULT (STDMETHODCALLTYPE *TDXGISwapChainPresent)( 
	IDXGISwapChain * This,
	UINT SyncInterval,
	UINT Flags
);

typedef HRESULT (STDMETHODCALLTYPE *TCreateDXGIFactory) (
	REFIID riid,
	void **ppFactory
);

typedef HRESULT (STDMETHODCALLTYPE *TDXGICreateSwapChain) (
	IDXGIFactory *This,
	IUnknown *pDevice,
	DXGI_SWAP_CHAIN_DESC *pDesc,
	IDXGISwapChain **ppSwapChain
);
// ------

// --- DirectX 10 / 10.1 ---
typedef HRESULT (STDMETHODCALLTYPE *TD3D10CreateDeviceAndSwapChain)(
	IDXGIAdapter *pAdapter,
	D3D10_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	UINT SDKVersion,
	DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
	IDXGISwapChain **ppSwapChain,
	ID3D10Device **ppDevice
);

typedef HRESULT (STDMETHODCALLTYPE *TD3D10CreateDeviceAndSwapChain1)(
	IDXGIAdapter *pAdapter,
	D3D10_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	D3D10_FEATURE_LEVEL1 HardwareLevel,
	UINT SDKVersion,
	DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
	IDXGISwapChain **ppSwapChain,
	ID3D10Device1 **ppDevice
);
// ------

// --- DirectX 11 ---
typedef HRESULT (STDMETHODCALLTYPE *TD3D11CreateDeviceAndSwapChain) (
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
);
// ------
#endif

#endif
