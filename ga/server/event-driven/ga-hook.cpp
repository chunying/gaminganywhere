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
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#include "ga-common.h"
#include "ga-conf.h"
#include "ga-module.h"
#include "rtspconf.h"
#include "server.h"
#include "controller.h"
#include "encoder-common.h"

#include "hook-function.h"
#include "ga-hook-common.h"
#include "ga-hook-sdl.h"
#include "ga-hook-sdlaudio.h"
#include "ga-hook-coreaudio.h"
#include "detours.h"

// DLL share segment
//#pragma data_seg(".shared") 
static HHOOK gHook = NULL;
//char g_root[1024] = "";
//char g_confpath[1024] = "";
//char g_appexe[1024] = "";
static int app_hooked = 0;
//#pragma data_seg()
//#pragma comment(linker,"/section:.shared,rws")
//
static HMODULE hInst = NULL;
static int hookid = 0;

//static struct gaRect rect;

static struct ga_module *m_filter, *m_vencoder, *m_asource, *m_aencoder, *m_ctrl;

static int module_checked = 0;

#define	load_hook_function(mod, type, ptr, func)	\
		if((ptr = (type) GetProcAddress(mod, func)) == NULL) { \
			ga_error("GetProcAddress(%s) failed.\n", func); \
			return -1; \
		}

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
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)pGetRawInputData, hook_GetRawInputData);
	DetourTransactionCommit();
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
	const char *sdlpath = getenv("LIBSDL_SO");
	const char *def_sdlpath = "SDL.dll";
	if(sdlpath == NULL)
		sdlpath = def_sdlpath;
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
#define	SDL_DO_HOOK(name)	\
		DetourTransactionBegin(); \
		DetourUpdateThread(GetCurrentThread()); \
		DetourAttach(&(PVOID&)old_##name, hook_##name); \
		DetourTransactionCommit();
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
#if 1
	SDL_DO_HOOK(SDL_GL_SwapBuffers);
	SDL_DO_HOOK(SDL_PollEvent);
#else
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)old_SDL_GL_SwapBuffers, hook_SDL_GL_SwapBuffers);
	DetourTransactionCommit();
	//
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)old_SDL_PollEvent, hook_SDL_PollEvent);
	DetourTransactionCommit();
#endif
#undef	SDL_DO_HOOK
	ga_error("hook_sdl12: done\n");
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

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)pD3D9DevicePresent, hook_D3D9DevicePresent);
	DetourAttach(&(PVOID&)pSwapChainPresent, hook_D3D9SwapChainPresent);
	DetourTransactionCommit();

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
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)pD3d, hook_d3d);
	DetourTransactionCommit();
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
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)pCreateDXGIFactory, hook_CreateDXGIFactory);
	DetourTransactionCommit();
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
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)pD3D10CreateDeviceAndSwapChain1, hook_D3D10CreateDeviceAndSwapChain1);
	DetourTransactionCommit();
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
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)pD3D10CreateDeviceAndSwapChain, hook_D3D10CreateDeviceAndSwapChain);
	DetourTransactionCommit();
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
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)pD3D11CreateDeviceAndSwapChain, hook_D3D11CreateDeviceAndSwapChain);
	DetourTransactionCommit();
	return 0;
}

int
do_hook(char *hook_type, int hook_type_sz) {
	char *ptr, app_exe[1024], hook_method[1024];
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
hook_app() {
	char *appexe;
	char module_name[1024];
	int pid = GetCurrentProcessId();

	module_checked = 1;

	if(GetModuleFileName(NULL, module_name, sizeof(module_name)) == 0) {
		ga_error("GetModuleFileName failed: %s\n", GetLastError());
		return -1;
	}

	if((appexe = getenv("GA_APPEXE")) == NULL) {
		ga_error("[%d] No GA_APPEXE provided.\n", pid);
		return -1;
	}

	if(strstr(module_name, appexe) == NULL) {
		ga_error("will not hook: %s\n", module_name);
		return -1;
	}

	app_hooked = 1;

	ga_error("hooked app module: [%d] %s\n", pid, module_name);

	return 0;
}

LRESULT CALLBACK
hook_proc(int nCode, WPARAM wParam, LPARAM lParam) {
	char hook_type[64];
	pthread_t ga_server_thread;

	if(module_checked || app_hooked)
		return CallNextHookEx(gHook, nCode, wParam, lParam);

	if(hook_app() < 0)
		return CallNextHookEx(gHook, nCode, wParam, lParam);
	
	// identified the executable: initialize once

	if(do_hook(hook_type, sizeof(hook_type)) < 0) {
		ga_error("hook_proc: hook failed.\n");
		return CallNextHookEx(gHook, nCode, wParam, lParam);
	}
	
	// SDL: override controller
	if(strcasecmp(hook_type, "sdl") == 0) {
		sdl12_mapinit();
		ctrl_server_setreplay(sdl_hook_replay_callback);
		no_default_controller = 1;
		ga_error("hook_proc: sdl - use native replayer.\n");
	}
	// start hook server
	if(pthread_create(&ga_server_thread, NULL, ga_server, NULL) != 0) {
		ga_error("cannot create GA server thread\n");
		return CallNextHookEx(gHook, nCode, wParam, lParam);
	}
	pthread_detach(ga_server_thread);
	
	return CallNextHookEx(gHook, nCode, wParam, lParam);
}

MODULE MODULE_EXPORT int
install_hook(const char *ga_root, const char *config, const char *app_exe)
{
	if(ga_root == NULL || config == NULL) {
		ga_error("[install_hook] no ga-root nor configuration were specified.\n");
		return -1;
	}
	
	if((gHook = SetWindowsHookEx(WH_CBT, hook_proc, hInst, 0)) == NULL) {
		ga_error("SetWindowsHookEx filaed (0x%08x)\n", GetLastError());
		return -1;
	}

	ga_error("[install_hook] success.\n");
	
	return 0;
}

MODULE MODULE_EXPORT int
uninstall_hook() {
	if(gHook != NULL) {
		UnhookWindowsHookEx(gHook);
		gHook = NULL;
		ga_error("[uninstall_hook] success.\n");
	}
	return 0;
}

MODULE MODULE_EXPORT BOOL APIENTRY
DllMain(HINSTANCE hModule, DWORD fdwReason, LPVOID lpReserved) {
	char module_name[1024];
	static int initialized = 0;

	if(initialized == 0) {
		srand(time(NULL));
		hookid = rand() & 0x0ffff;
		initialized = 1;
	}

	if(GetModuleFileName(NULL, module_name, sizeof(module_name)) <= 0)
		module_name[0] = '\0';

	switch(fdwReason) {
	case DLL_PROCESS_ATTACH:
		hInst = hModule;
		ga_error("[ga-hook-%04x] attached to %s\n", hookid, module_name);
		break;
	case DLL_PROCESS_DETACH:
		ga_error("[ga-hook-%04x] detached from %s\n", hookid, module_name);
		break;
	}

	return TRUE;
}
