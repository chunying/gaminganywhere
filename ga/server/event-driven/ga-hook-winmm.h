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

#ifndef __GA_HOOK_WINMM_H__
#define __GA_HOOK_WINMM_H__

#include <mmsystem.h>
#include <mmreg.h>

#include "hook-common.h"

#ifdef __cplusplus
extern "C" {
#endif
// winmm: not STDMETHODCALLTYPE?
typedef MMRESULT (*t_waveOutOpen)(LPHWAVEOUT phwo, UINT_PTR uDeviceID, LPWAVEFORMATEX pwfx, DWORD_PTR dwCallback, DWORD_PTR dwCallbackInstance, DWORD fdwOpen);
typedef MMRESULT (*t_waveOutWrite)(HWAVEOUT hwo, LPWAVEHDR pwh, UINT cbwh);
typedef MMRESULT (*t_waveOutClose)(HWAVEOUT hwo);
#ifdef __cplusplus
}
#endif

// prototypes: no need DllExport
MMRESULT hook_waveOutOpen(
		LPHWAVEOUT phwo,
		UINT_PTR uDeviceID,
		LPWAVEFORMATEX pwfx,
		DWORD_PTR dwCallback,
		DWORD_PTR dwCallbackInstance,
		DWORD fdwOpen);
MMRESULT hook_waveOutWrite(HWAVEOUT hwo, LPWAVEHDR pwh, UINT cbwh);
MMRESULT hook_waveOutClose(HWAVEOUT hwo);

int hook_winmm();

#endif	/* __GA_HOOK_WINMM_H__ */

