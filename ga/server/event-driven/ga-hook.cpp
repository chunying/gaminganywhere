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
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#include "ga-common.h"
#include "ga-conf.h"
#include "ga-module.h"
#include "rtspconf.h"
#include "controller.h"
#include "encoder-common.h"
#include "ctrl-sdl.h"

#include "hook-function.h"
#include "ga-hook-common.h"
#include "ga-hook-sdl.h"
#include "ga-hook-sdlaudio.h"
#include "ga-hook-sdl2.h"
#include "ga-hook-sdl2audio.h"
#include "ga-hook-coreaudio.h"
#include "ga-hook-gl.h"

#include "easyhook.h"

static HMODULE hInst = NULL;
static int hookid = 0;

#define	load_hook_function(mod, type, ptr, func)	\
		if((ptr = (type) GetProcAddress(mod, func)) == NULL) { \
			ga_error("GetProcAddress(%s) failed.\n", func); \
			return -1; \
		}

#define	hook_function()

static int
hook_input() {
	HMODULE hMod;
	// --- GetRawInputData ---
	GetRawInputDevice(false);
	//ga_error("Start to hook GetRawInputData ...");
	if((hMod = LoadLibrary("user32.dll")) == NULL) {
		ga_error("Load user32.dll failed.\n");
		return -1;
	}
	//
	pGetRawInputData = (TGetRawInputData)
		GetProcAddress(hMod, "GetRawInputData");
	//
	if(pGetRawInputData == NULL) {
		ga_error("Unable to hook GetRawInputData (NULL returned).\n");
		return -1;
	}
	//
	ga_hook_function("GetRawInputData",
		pGetRawInputData,
		hook_GetRawInputData);
	return 0;
}

static int
hook_audio(const char *type) {
	if(type == NULL)
		return 0;
	if(*type == '\0')
		return 0;
	if(strcmp(type, "coreaudio") == 0)
		return hook_coreaudio();
	return 0;
}

int
hook_SDL_UpperBlit(SDL12_Surface *src, SDL12_Rect *srcrect, SDL12_Surface *dst, SDL12_Rect *dstrect) {
	return hook_SDL_BlitSurface(src, srcrect, dst, dstrect);
}

static int
hook_sdl12(const char *hook_type, const char *hook_method) {
	HMODULE hMod;
	char audio_type[64] = "";
	//const char *sdlpath = getenv("LIBSDL_SO");
	char real_sdlpath[256];
	const char *sdlpath = NULL;
	const char def_sdlpath[] = "SDL.dll";
	if(getenv("LIBSDL_SO") != NULL) {
		strncpy(real_sdlpath, getenv("LIBSDL_SO"), sizeof(real_sdlpath));
		sdlpath = real_sdlpath;
	} else if(ga_conf_readv("hook-sdl-path", real_sdlpath, sizeof(real_sdlpath)) != NULL) {
		sdlpath = real_sdlpath;
	} else {
		sdlpath = def_sdlpath;
	}
	//
	if((hMod = GetModuleHandle(sdlpath)) == NULL) {
		if((hMod = LoadLibrary(sdlpath)) == NULL) {
			ga_error("Load %s failed.\n", sdlpath);
			return -1;
		}
	}
	if(ga_conf_readv("hook-audio", audio_type, sizeof(audio_type)) == NULL)
		audio_type[0] = '\0';
	//
	//load_hook_function(hMod, t_SDL_Init, old_SDL_Init, "SDL_Init");
	load_hook_function(hMod, t_SDL_Init, old_SDL_Init, "SDL_Init");
	load_hook_function(hMod, t_SDL_SetVideoMode, old_SDL_SetVideoMode, "SDL_SetVideoMode");
	load_hook_function(hMod, t_SDL_UpperBlit, old_SDL_UpperBlit, "SDL_UpperBlit");
	// XXX: BlitSurface == UpperBlit
	load_hook_function(hMod, t_SDL_BlitSurface, old_SDL_BlitSurface, "SDL_UpperBlit");
	load_hook_function(hMod, t_SDL_Flip, old_SDL_Flip, "SDL_Flip");
	load_hook_function(hMod, t_SDL_UpdateRect, old_SDL_UpdateRect, "SDL_UpdateRect");
	load_hook_function(hMod, t_SDL_UpdateRects, old_SDL_UpdateRects, "SDL_UpdateRects");
	load_hook_function(hMod, t_SDL_GL_SwapBuffers, old_SDL_GL_SwapBuffers, "SDL_GL_SwapBuffers");
	load_hook_function(hMod, t_SDL_PollEvent, old_SDL_PollEvent, "SDL_PollEvent");
	load_hook_function(hMod, t_SDL_WaitEvent, old_SDL_WaitEvent, "SDL_WaitEvent");
	load_hook_function(hMod, t_SDL_PeepEvents, old_SDL_PeepEvents, "SDL_PeepEvents");
	load_hook_function(hMod, t_SDL_SetEventFilter, old_SDL_SetEventFilter, "SDL_SetEventFilter");
	if(strcmp("sdlaudio", audio_type) == 0) {
	/////////////////////////////////////////
	load_hook_function(hMod, t_SDL_OpenAudio, old_SDL_OpenAudio, "SDL_OpenAudio");
	load_hook_function(hMod, t_SDL_PauseAudio, old_SDL_PauseAudio, "SDL_PauseAudio");
	load_hook_function(hMod, t_SDL_CloseAudio, old_SDL_CloseAudio, "SDL_CloseAudio");
	ga_error("hook: sdlaudio enabled.\n");
	/////////////////////////////////////////
	}
	// for internal use
	load_hook_function(hMod, t_SDL_CreateRGBSurface, old_SDL_CreateRGBSurface, "SDL_CreateRGBSurface");
	load_hook_function(hMod, t_SDL_FreeSurface, old_SDL_FreeSurface, "SDL_FreeSurface");
	load_hook_function(hMod, t_SDL_PushEvent, old_SDL_PushEvent, "SDL_PushEvent");
	//
#define	SDL_DO_HOOK(name)	ga_hook_function(#name, old_##name, hook_##name)
	//SDL_DO_HOOK(SDL_Init);
	SDL_DO_HOOK(SDL_SetVideoMode);
	SDL_DO_HOOK(SDL_UpperBlit);
	SDL_DO_HOOK(SDL_Flip);
	SDL_DO_HOOK(SDL_UpdateRect);
	SDL_DO_HOOK(SDL_UpdateRects);
	SDL_DO_HOOK(SDL_WaitEvent);
	SDL_DO_HOOK(SDL_PeepEvents);
	SDL_DO_HOOK(SDL_SetEventFilter);
	if(strcmp("sdlaudio", audio_type) == 0) {
	/////////////////////////////////////////
	SDL_DO_HOOK(SDL_OpenAudio);
	SDL_DO_HOOK(SDL_PauseAudio);
	SDL_DO_HOOK(SDL_CloseAudio);
	/////////////////////////////////////////
	}
	SDL_DO_HOOK(SDL_GL_SwapBuffers);
	SDL_DO_HOOK(SDL_PollEvent);
#undef	SDL_DO_HOOK
	ga_error("hook_sdl12: done\n");
	//
	return 0;
}

static int
hook_sdl2(const char *hook_type, const char *hook_method) {
	HMODULE hMod, hModGL;
	char audio_type[64] = "";
	//const char *sdlpath = getenv("LIBSDL_SO");
	char real_sdlpath[256];
	const char *sdlpath = NULL;
	const char def_sdlpath[] = "SDL.dll";
	if(getenv("LIBSDL_SO") != NULL) {
		strncpy(real_sdlpath, getenv("LIBSDL_SO"), sizeof(real_sdlpath));
		sdlpath = real_sdlpath;
	} else if(ga_conf_readv("hook-sdl-path", real_sdlpath, sizeof(real_sdlpath)) != NULL) {
		sdlpath = real_sdlpath;
	} else {
		sdlpath = def_sdlpath;
	}
	//
	if((hMod = GetModuleHandle(sdlpath)) == NULL) {
		if((hMod = LoadLibrary(sdlpath)) == NULL) {
			ga_error("Load %s failed.\n", sdlpath);
			return -1;
		}
	}
	if((hModGL = GetModuleHandle("OPENGL32.DLL")) == NULL) {
		if((hModGL = LoadLibrary("OPENGL32.DLL")) == NULL) {
			ga_error("hook_sdl2: unable to load opengl32.dll.\n");
		} else {
			ga_error("hook_sdl2: opengl32.dll loaded.\n");
		}
	}
	if(ga_conf_readv("hook-audio", audio_type, sizeof(audio_type)) == NULL)
		audio_type[0] = '\0';
	//
	//load_hook_function(hMod, t_SDL_Init, old_SDL_Init, "SDL_Init");
	load_hook_function(hMod, t_SDL2_Init, old_SDL2_Init, "SDL_Init");
	load_hook_function(hMod, t_SDL2_CreateWindow, old_SDL2_CreateWindow, "SDL_CreateWindow");
	load_hook_function(hMod, t_SDL2_CreateRenderer, old_SDL2_CreateRenderer, "SDL_CreateRenderer");
	load_hook_function(hMod, t_SDL2_CreateTexture, old_SDL2_CreateTexture, "SDL_CreateTexture");
	load_hook_function(hMod, t_SDL2_UpperBlit, old_SDL2_UpperBlit, "SDL_UpperBlit");
	// XXX: BlitSurface == UpperBlit
	load_hook_function(hMod, t_SDL2_BlitSurface, old_SDL2_BlitSurface, "SDL_UpperBlit");
	load_hook_function(hMod, t_SDL2_GetRendererInfo, old_SDL2_GetRendererInfo, "SDL_GetRendererInfo");
	load_hook_function(hMod, t_SDL2_RenderReadPixels, old_SDL2_RenderReadPixels, "SDL_RenderReadPixels");
	load_hook_function(hMod, t_SDL2_RenderPresent, old_SDL2_RenderPresent, "SDL_RenderPresent");
	load_hook_function(hMod, t_SDL2_GL_SwapWindow, old_SDL2_GL_SwapWindow, "SDL_GL_SwapWindow");
	if(hModGL != NULL) {
		load_hook_function(hModGL, t_SDL2_GL_glFlush, old_SDL2_GL_glFlush, "glFlush");
	}
	load_hook_function(hMod, t_SDL2_PollEvent, old_SDL2_PollEvent, "SDL_PollEvent");
	load_hook_function(hMod, t_SDL2_WaitEvent, old_SDL2_WaitEvent, "SDL_WaitEvent");
	load_hook_function(hMod, t_SDL2_PeepEvents, old_SDL2_PeepEvents, "SDL_PeepEvents");
	load_hook_function(hMod, t_SDL2_SetEventFilter, old_SDL2_SetEventFilter, "SDL_SetEventFilter");
	if(strcmp("sdl2audio", audio_type) == 0) {
	/////////////////////////////////////////
	load_hook_function(hMod, t_SDL2_OpenAudio, old_SDL2_OpenAudio, "SDL_OpenAudio");
	load_hook_function(hMod, t_SDL2_PauseAudio, old_SDL2_PauseAudio, "SDL_PauseAudio");
	load_hook_function(hMod, t_SDL2_CloseAudio, old_SDL2_CloseAudio, "SDL_CloseAudio");
	ga_error("hook: sdl2audio enabled.\n");
	/////////////////////////////////////////
	}
	// for internal use
	load_hook_function(hMod, t_SDL2_CreateRGBSurface, old_SDL2_CreateRGBSurface, "SDL_CreateRGBSurface");
	load_hook_function(hMod, t_SDL2_FreeSurface, old_SDL2_FreeSurface, "SDL_FreeSurface");
	load_hook_function(hMod, t_SDL2_PushEvent, old_SDL2_PushEvent, "SDL_PushEvent");
	//
#define	SDL_DO_HOOK(name)	ga_hook_function(#name, old_##name, hook_##name)
	//SDL_DO_HOOK(SDL_Init);
	SDL_DO_HOOK(SDL2_CreateWindow);
	SDL_DO_HOOK(SDL2_CreateRenderer);
	SDL_DO_HOOK(SDL2_CreateTexture);
	SDL_DO_HOOK(SDL2_UpperBlit);
	SDL_DO_HOOK(SDL2_RenderPresent);
	SDL_DO_HOOK(SDL2_WaitEvent);
	SDL_DO_HOOK(SDL2_PeepEvents);
	SDL_DO_HOOK(SDL2_SetEventFilter);
	if(strcmp("sdl2audio", audio_type) == 0) {
	/////////////////////////////////////////
	SDL_DO_HOOK(SDL2_OpenAudio);
	SDL_DO_HOOK(SDL2_PauseAudio);
	SDL_DO_HOOK(SDL2_CloseAudio);
	/////////////////////////////////////////
	}
	SDL_DO_HOOK(SDL2_GL_SwapWindow);
	SDL_DO_HOOK(SDL2_GL_glFlush);
	SDL_DO_HOOK(SDL2_PollEvent);
#undef	SDL_DO_HOOK
	ga_error("hook_sdl2: done\n");
	//
	return 0;
}

static int
hook_gl() {
	HMODULE hMod;
	//
	if((hMod = GetModuleHandle("OPENGL32.DLL")) == NULL) {
		if((hMod = LoadLibrary("OPENGL32.DLL")) == NULL) {
			ga_error("hook_gl: unable to load opengl32.dll.\n");
			return -1;
		} else {
			ga_error("hook_gl: opengl32.dll loaded.\n");
		}
	}
	//
	load_hook_function(hMod, t_glFlush, old_glFlush, "glFlush");
	//
	ga_hook_function("glFlush", old_glFlush, hook_glFlush);
	ga_error("hook_gl: done\n");
	//
	return 0;
}

// OLD D3D9 hook codes - disabled
static int
create_d3d9_object() {
	HRESULT hr;
	LPDIRECT3D9 pD3D = NULL;
	LPDIRECT3DDEVICE9 pd3dDevice = NULL;
	LPDIRECT3DSWAPCHAIN9 pSwapChain = NULL;
	HMODULE hMod;
	HWND hWnd;
	//
	D3DDISPLAYMODE d3ddm;
	D3DCAPS9 d3dCaps;
	DWORD dwBehaviorFlags = 0;
	D3DPRESENT_PARAMETERS d3dpp;

	if((hMod = GetModuleHandle("d3d9.dll")) == NULL) {
		if((hMod = LoadLibrary("d3d9.dll")) == NULL) {
			ga_error("Load d3d9.dll failed.\n");
			return -1;
		}
	}

	hWnd = CreateWindowA("BUTTON", 
			"Temporary Window", 
			WS_SYSMENU | WS_MINIMIZEBOX, 
			CW_USEDEFAULT, 
			CW_USEDEFAULT, 
			300, 300, 
			NULL, NULL, NULL, NULL);
	if (hWnd == NULL) {
		ga_error("CreateWindow failed.\n");
		return -1;
	}
			
	if((pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL) {
		ga_error("Direct3DCreate9 failed.\n");
		return -1;
	}

	// provide information for the current display mode for a
	// specified device d3d format
	if(FAILED(pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm))) {
		ga_error("GetAdapterDisplayMode failed.\n");
		return -1;
	}
	ga_error("Display mode: w: %d, h: %d, rate: %d, format: %d\n",
		d3ddm.Width, d3ddm.Height, d3ddm.RefreshRate, d3ddm.Format);

	// retrieve device-specific information about a device
	// D3DDEVTYPE_HAL: hardware rasterization, shading is done with software,
	//                 hardware, or mixed transform and lighting
	if(FAILED(pD3D->GetDeviceCaps(D3DADAPTER_DEFAULT, 
				D3DDEVTYPE_HAL, &d3dCaps))) {
		ga_error("GetDeviceCaps failed.\n");
		return -1;
	}
	ga_error("Device information: type: %d, intervals: %d", 
		d3dCaps.DeviceType, d3dCaps.PresentationIntervals);
	//
	if(d3dCaps.VertexProcessingCaps != 0 ) {
		dwBehaviorFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
	} else {
		dwBehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
	}

	ZeroMemory(&d3dpp, sizeof(d3dpp));

	d3dpp.BackBufferFormat		= d3ddm.Format;
	//d3dpp.BackBufferHeight	= d3ddm.Height;
	//d3dpp.BackBufferWidth		= d3ddm.Width;
	d3dpp.SwapEffect		= D3DSWAPEFFECT_DISCARD;
	d3dpp.Windowed			= TRUE; //FALSE;
	d3dpp.EnableAutoDepthStencil	= TRUE;
	d3dpp.AutoDepthStencilFormat	= D3DFMT_D16;
	d3dpp.PresentationInterval	= D3DPRESENT_INTERVAL_IMMEDIATE;
	d3dpp.Flags			= D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;

	hr = pD3D->CreateDevice(D3DADAPTER_DEFAULT, 
				D3DDEVTYPE_HAL, 
				hWnd,
				dwBehaviorFlags, 
				&d3dpp, 
				&pd3dDevice);
	if(FAILED(hr)){
		ga_error("CreateDevice failed.: %s\n", DXGetErrorString(hr));
		return -1;
	}

	// start to hook D3D9DevicePresent()
	// 17: IDirect3DDevice9::Present
	uintptr_t* pInterfaceVTable = (uintptr_t*)*(uintptr_t*)pd3dDevice;
	pD3D9DevicePresent = (TD3D9DevicePresent)pInterfaceVTable[17];
	//OutputDebugString("Start to hook IDirect3DDevice9::Present");

	// start to hook D3D9SwapChainPresent()
	// 3: IDirect3DSwapChain9::Present
	pd3dDevice->GetSwapChain(0, &pSwapChain);
	uintptr_t* pInterfaceVTable2 = (uintptr_t*)*(uintptr_t*)pSwapChain;
	pSwapChainPresent = (TSwapChainPresent)pInterfaceVTable2[3];
	//OutputDebugString("Start to hook IDirect3DSwapChain9::Present");

	ga_hook_function("D3D9DevicePresent", pD3D9DevicePresent, hook_D3D9DevicePresent);
	ga_hook_function("D3D9SwapChainPresent", pSwapChainPresent, hook_D3D9SwapChainPresent);

	pd3dDevice->Release();
	pD3D->Release();
	DestroyWindow(hWnd);

	return 0;
}

static int
hook_d9(const char *hook_type, const char *hook_method) {
	HMODULE hMod;
	// GetCreateDeviceAddress
	//ga_error("Start to hook Direct3DCreate9 ...");
	//
	if((hMod = GetModuleHandle("d3d9.dll")) == NULL) {
		if((hMod = LoadLibrary("d3d9.dll")) == NULL) {
			ga_error("Load d3d9.dll failed.\n");
			return -1;
		}
	}
	// Direct3DCreate9()
	pD3d = (TDirect3DCreate9)
		GetProcAddress(hMod, "Direct3DCreate9");
	if(pD3d == NULL) {
		ga_error("GetProcAddress(Direct3DCreate9) failed.\n");
		return -1;
	}
	//
	ga_hook_function("Direct3DCreate9", pD3d, hook_d3d);
	//
	return 0;
}

static int
hook_dxgi(const char *hook_type, const char *hook_method) {
	HMODULE hMod;
	//
	if((hMod = GetModuleHandle("dxgi.dll")) == NULL) {
		ga_error("Load dxgi.dll failed.\n");
		return -1;
	}
	//
	pCreateDXGIFactory = (TCreateDXGIFactory) GetProcAddress(hMod, "CreateDXGIFactory");
	if (pCreateDXGIFactory == NULL) {
		ga_error("GetProcAddress(CreateDXGIFactory) failed.\n");
		return -1;
	}
	//
	ga_hook_function("CreateDXGIFactory", pCreateDXGIFactory, hook_CreateDXGIFactory);
	return 0;
}

static int
hook_d10_1(const char *hook_type, const char *hook_method) {
	HMODULE hMod;
	//
	if((hMod = GetModuleHandle("d3d10_1.dll")) == NULL) {
		if((hMod = LoadLibrary("d3d10_1.dll")) == NULL) {
			ga_error("Load d3d10_1.dll failed.\n");
			return -1;
		}
	}
	//
	pD3D10CreateDeviceAndSwapChain1 = (TD3D10CreateDeviceAndSwapChain1)
		GetProcAddress(hMod, "D3D10CreateDeviceAndSwapChain1");
	if (pD3D10CreateDeviceAndSwapChain1 == NULL) {
		ga_error("GetProcAddress(D3D10CreateDeviceAndSwapChain1) failed.\n");
		return -1;
	}
	//
	ga_hook_function("D3D10CreateDeviceAndSwapChain1",
		pD3D10CreateDeviceAndSwapChain1,
		hook_D3D10CreateDeviceAndSwapChain1);
	return 0;
}

static int
hook_d10(const char *hook_type, const char *hook_method) {
	HMODULE hMod;
	//
	if((hMod = GetModuleHandle("d3d10.dll")) == NULL) {
		if((hMod = LoadLibrary("d3d10.dll")) == NULL) {
			ga_error("Load d3d10.dll failed.\n");
			return -1;
		}
	}
	//
	pD3D10CreateDeviceAndSwapChain = (TD3D10CreateDeviceAndSwapChain)
		GetProcAddress(hMod, "D3D10CreateDeviceAndSwapChain");
	if (pD3D10CreateDeviceAndSwapChain == NULL) {
		ga_error("GetProcAddress(D3D10CreateDeviceAndSwapChain) failed.\n");
		return -1;
	}
	//
	ga_hook_function("D3D10CreateDeviceAndSwapChain",
		pD3D10CreateDeviceAndSwapChain,
		hook_D3D10CreateDeviceAndSwapChain);
	return 0;
}

static int
hook_d11(const char *hook_type, const char *hook_method) {
	HMODULE hMod;
	//
	if((hMod = GetModuleHandle("d3d11.dll")) == NULL) {
		if((hMod = LoadLibrary("d3d11.dll")) == NULL) {
			ga_error("Load d3d11.dll failed.\n");
			return -1;
		}
	}
	//
	pD3D11CreateDeviceAndSwapChain = (TD3D11CreateDeviceAndSwapChain)
		GetProcAddress(hMod, "D3D11CreateDeviceAndSwapChain");
	if (pD3D11CreateDeviceAndSwapChain == NULL) {
		ga_error("GetProcAddress(D3D11CreateDeviceAndSwapChain) failed.\n");
		return -1;
	}
	//
	ga_hook_function("D3D11CreateDeviceAndSwapChain",
		pD3D11CreateDeviceAndSwapChain,
		hook_D3D11CreateDeviceAndSwapChain);
	return 0;
}

int
do_hook(char *hook_type, int hook_type_sz) {
	char *ptr, app_exe[1024], hook_method[1024];
	char game_dir[1024];
	char audio_type[64] = "";
	int resolution[2];
	//
	if(CoInitializeEx(NULL, COINIT_MULTITHREADED) != S_OK) {
		ga_error("*** CoInitializeEx failed.\n");
	}
	//
	if(ga_hook_init() < 0)
		return -1;
	// handle ga-hook specific configurations
	if((ptr = ga_conf_readv("game-exe", app_exe, sizeof(app_exe))) == NULL) {
		ga_error("*** no game executable specified.\n");
		return -1;
	}
	if((ptr = ga_conf_readv("game-dir", game_dir, sizeof(game_dir))) != NULL) {
		SetCurrentDirectory(game_dir);
		ga_error("gamedir: set to %s\n", game_dir);
	}
	if((ptr = ga_conf_readv("hook-type", hook_type, hook_type_sz)) == NULL) {
		ga_error("*** no hook type specified.\n");
		return -1;
	}
	if((ptr = ga_conf_readv("hook-method", hook_method, sizeof(hook_method))) != NULL) {
		ga_error("*** hook method specified: %s\n", hook_method);
	}
	if((ptr = ga_conf_readv("hook-audio", audio_type, sizeof(audio_type))) != NULL) {
		ga_error("*** hook audio = %s\n", audio_type);
	}
	//
	ga_error("[start-hook] exe=%s; type=%s\n", app_exe, hook_type);
	//
	if(hook_input() < 0)
		return -1;
	if(hook_audio(audio_type) < 0)
		return -1;
	// ---
	if(strcasecmp(hook_type, "sdl") == 0) {
		return hook_sdl12(hook_type, hook_method);
	}
	if(strcasecmp(hook_type, "sdl2") == 0) {
		return hook_sdl2(hook_type, hook_method);
	}
	if(strcasecmp(hook_type, "gl") == 0) {
		return hook_gl();
	}
	// d9?
	if(strcasecmp(hook_type, "d9") == 0) {
		return hook_d9(hook_type, hook_method);
	}
	// dxgi?
	if(strcasecmp(hook_type, "dxgi") == 0) {
	//if(strstr(hook_method, "GetDXGIFactoryAddress") != NULL)
		return hook_dxgi(hook_type, hook_method);
	}
	// d10?
	if(strcasecmp(hook_type, "d10") == 0) {
		return hook_d10(hook_type, hook_method);
	}
	// d10.1?
	if(strcasecmp(hook_type, "d10.1") == 0) {
		return hook_d10_1(hook_type, hook_method);
	}
	// d11?
	if(strcasecmp(hook_type, "d11") == 0) {
		return hook_d11(hook_type, hook_method);
	}
	//
	ga_error("Unsupported hook type (%s)\n", hook_type);
	return -1;
}

int
hook_proc() {
	char hook_type[64];
	char cwd[1024];
	pthread_t ga_server_thread;

	if(do_hook(hook_type, sizeof(hook_type)) < 0) {
		ga_error("hook_proc: hook failed.\n");
		return -1;
	}
	
	// SDL: override controller
	if(strcasecmp(hook_type, "sdl") == 0) {
		sdl12_mapinit();
		sdlmsg_kb_init();
		ctrl_server_setreplay(sdl_hook_replay_callback);
		no_default_controller = 1;
		ga_error("hook_proc: sdl - use native replayer.\n");
	} else if(strcasecmp(hook_type, "sdl2") == 0) {
		sdlmsg_kb_init();
		ctrl_server_setreplay(sdl2_hook_replay_callback);
		no_default_controller = 1;
		ga_error("hook_proc: sdl2 - use native replayer.\n");
	}

	// start hook server
	if(pthread_create(&ga_server_thread, NULL, ga_server, NULL) != 0) {
		ga_error("cannot create GA server thread\n");
		return -1;
	}
	pthread_detach(ga_server_thread);
	
	// show current directory
	if(GetCurrentDirectory(sizeof(cwd), cwd) > 0) {
		ga_error("hook_proc: current directory [%s]\n", cwd);
	}

	return 0;
}

MODULE MODULE_EXPORT void WINAPI
NativeInjectionEntryPoint(REMOTE_ENTRY_INFO *info) {
	if(hook_proc() < 0)
		exit(-1);
	RhWakeUpProcess();
	return;
}

MODULE MODULE_EXPORT BOOL APIENTRY
DllMain(HINSTANCE hModule, DWORD fdwReason, LPVOID lpReserved) {
	/* always return true */
	return TRUE;
}

