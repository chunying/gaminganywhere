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

#ifndef __GA_HOOK_COREAUDIO_H__
#define __GA_HOOK_COREAUDIO_H__

#include <mmdeviceapi.h>
#include <audioclient.h>

#include "hook-common.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef HRESULT (STDMETHODCALLTYPE *t_CoCreateInstance)(
	REFCLSID clsid,
	LPUNKNOWN punknown,
	DWORD dwClsContext,
	REFIID iid,
	LPVOID *ppv);
typedef HRESULT (STDMETHODCALLTYPE *t_EnumAudioEndpoints)( 
	IMMDeviceEnumerator *thiz,
	EDataFlow dataFlow,
	DWORD dwStateMask,
	IMMDeviceCollection **ppDevices);
typedef HRESULT (STDMETHODCALLTYPE *t_GetDefaultAudioEndpoint)(
	IMMDeviceEnumerator *thiz,
	EDataFlow dataFlow,
	ERole role,
	IMMDevice **ppDevice);
typedef HRESULT (STDMETHODCALLTYPE *t_GetDevice)( 
	IMMDeviceEnumerator *thiz,
	LPCWSTR pwstrId,
	IMMDevice **ppDevice);
typedef HRESULT (STDMETHODCALLTYPE * t_Activate)(
	IMMDeviceActivator *thiz,
	REFIID iid,
	DWORD dwClsCtx,
	PROPVARIANT *pActivationParams,
	void **ppInterface);
typedef HRESULT (STDMETHODCALLTYPE *t_Item)( 
	IMMDeviceCollection *thiz,
	UINT nDevice,
	IMMDevice **ppDevice);
typedef HRESULT (STDMETHODCALLTYPE *t_GetService)(
	IAudioClient *thiz,
	REFIID riid,
	void **ppv);
typedef HRESULT (STDMETHODCALLTYPE *t_GetBuffer)( 
	IAudioRenderClient *thiz,
	UINT32 NumFramesRequested,
	BYTE **ppData);
typedef HRESULT (STDMETHODCALLTYPE *t_ReleaseBuffer)( 
	IAudioRenderClient *thiz,
	UINT32 NumFramesWritten,
	DWORD dwFlags);
typedef HRESULT (STDMETHODCALLTYPE *t_GetMixFormat)( 
	IAudioClient *thiz,
	WAVEFORMATEX **ppDeviceFormat);
#ifdef __cplusplus
}
#endif

// prototypes
DllExport HRESULT __stdcall hook_CoCreateInstance(
		REFCLSID clsid,
		LPUNKNOWN punknown,
		DWORD dwClsContext,
		REFIID iid,
		LPVOID *ppv); 
DllExport HRESULT __stdcall hook_GetDevice ( 
		IMMDeviceEnumerator *thiz,
		LPCWSTR pwstrId,
		IMMDevice **ppDevice);
DllExport HRESULT __stdcall hook_Activate(
		IMMDeviceActivator *thiz,
		REFIID iid,
		DWORD dwClsCtx,
		PROPVARIANT *pActivationParams,
		void **ppInterface);
DllExport HRESULT __stdcall hook_GetDefaultAudioEndpoint(
		IMMDeviceEnumerator *thiz,
		EDataFlow dataFlow,
		ERole role,
		IMMDevice **ppDevice);
DllExport HRESULT __stdcall hook_EnumAudioEndpoints( 
		IMMDeviceEnumerator *thiz,
		EDataFlow dataFlow,
		DWORD dwStateMask,
		IMMDeviceCollection **ppDevices);
DllExport HRESULT __stdcall hook_Item( 
		IMMDeviceCollection *thiz,
		UINT nDevice,
		IMMDevice **ppDevice);
DllExport HRESULT __stdcall hook_GetService(
		IAudioClient *thiz,
		REFIID iid,
		void **ppv);
DllExport HRESULT __stdcall hook_GetBuffer( 
		IAudioRenderClient *thiz,
		UINT32 NumFramesRequested,
		BYTE **ppData);
DllExport HRESULT __stdcall hook_ReleaseBuffer( 
		IAudioRenderClient *thiz,
		UINT32 NumFramesWritten,
		DWORD dwFlags);
DllExport HRESULT __stdcall hook_GetMixFormat( 
		IAudioClient *thiz,
		WAVEFORMATEX **ppDeviceFormat);

int hook_coreaudio();

#endif

