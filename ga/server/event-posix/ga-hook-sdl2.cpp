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
#include <string.h>
#include <errno.h>
#include <pthread.h>
#ifndef WIN32
#include <dlfcn.h>
#endif

#include "ga-common.h"
#include "ga-conf.h"
#include "asource.h"
#include "vsource.h"
#include "dpipe.h"
#include "controller.h"
#include "ctrl-sdl.h"

#include "ga-hook-common.h"
#include "ga-hook-sdl2.h"
#ifndef WIN32
#include "ga-hook-lib.h"
#endif

#ifdef __APPLE__
#include <OpenGL/glu.h>
#else
#include <GL/glu.h>
#endif

#include <map>
using namespace std;

// for hooking
t_SDL2_Init		old_SDL2_Init = NULL;
t_SDL2_CreateWindow	old_SDL2_CreateWindow = NULL;
t_SDL2_CreateRenderer	old_SDL2_CreateRenderer = NULL;
t_SDL2_CreateTexture	old_SDL2_CreateTexture = NULL;
t_SDL2_UpperBlit	old_SDL2_UpperBlit = NULL;
t_SDL2_BlitSurface	old_SDL2_BlitSurface = NULL;
t_SDL2_GetRendererInfo	old_SDL2_GetRendererInfo = NULL;
t_SDL2_RenderReadPixels	old_SDL2_RenderReadPixels = NULL;
t_SDL2_RenderPresent	old_SDL2_RenderPresent = NULL;
t_SDL2_GL_SwapWindow	old_SDL2_GL_SwapWindow = NULL;
t_SDL2_GL_glFlush	old_SDL2_GL_glFlush = NULL;
t_SDL2_PollEvent	old_SDL2_PollEvent = NULL;
t_SDL2_WaitEvent	old_SDL2_WaitEvent = NULL;
t_SDL2_PeepEvents	old_SDL2_PeepEvents = NULL;
t_SDL2_SetEventFilter	old_SDL2_SetEventFilter = NULL;
// for internal use
t_SDL2_CreateRGBSurface	old_SDL2_CreateRGBSurface = NULL;
t_SDL2_PushEvent	old_SDL2_PushEvent = NULL;
t_SDL2_FreeSurface	old_SDL2_FreeSurface = NULL;

//static SDL_Surface	*screensurface = NULL;
//static SDL_Surface	*dupsurface = NULL;
// video information
static int curr_width = -1;
static int curr_height = -1;
static SDL_Window *curr_window = NULL;
static SDL_Renderer *curr_renderer = NULL;
//
static SDL_EventFilter local_filter = NULL;

int
sdl2_hook_init() {
	static int initialized = 0;
	pthread_t t;
	//
	if(initialized != 0)
		return 0;
	// reset global variables
	curr_width = curr_height = -1;
	curr_window = NULL;
	curr_renderer = NULL;
	// override controller
	sdlmsg_kb_init();
	ctrl_server_setreplay(sdl2_hook_replay_callback);
	no_default_controller = 1;
	//
	if(pthread_create(&t, NULL, ga_server, NULL) != 0) {
		ga_error("ga_hook: create thread failed.\n");
		exit(-1);
	}
	pthread_detach(t);
	//
	initialized = 1;
	//
	ga_error("SDL hook: initialized.\n");
	//
	return 0;
}

static void
sdl2_hook_symbols() {
#ifndef WIN32
	void *handle = NULL;
	char *ptr, soname[2048];
	if((ptr = getenv("LIBSDL_SO")) == NULL) {
		strncpy(soname, "libSDL-2.0.so.0", sizeof(soname));
	} else if((ptr = ga_conf_readv("hook-sdl-path", soname, sizeof(soname))) != NULL) {
		// use that from "hook-sdl-path" configuration
		// do nothing
	} else {
		strncpy(soname, ptr, sizeof(soname));
	}
	if((handle = dlopen(soname, RTLD_LAZY)) == NULL) {
		ga_error("dlopen '%s' failed: %s\n", soname, strerror(errno));
		exit(-1);
	}
	// in case SDL_Init hook failed ...
	sdl2_hook_init();
	// for hooking
	old_SDL2_Init = (t_SDL2_Init)
				ga_hook_lookup_or_quit(handle, "SDL_Init");
	old_SDL2_CreateWindow = (t_SDL2_CreateWindow)
				ga_hook_lookup_or_quit(handle, "SDL_CreateWindow");
	old_SDL2_CreateRenderer = (t_SDL2_CreateRenderer)
				ga_hook_lookup_or_quit(handle, "SDL_CreateRenderer");
	old_SDL2_CreateTexture = (t_SDL2_CreateTexture)
				ga_hook_lookup_or_quit(handle, "SDL_CreateTexture");
	old_SDL2_UpperBlit = (t_SDL2_UpperBlit)
				ga_hook_lookup(handle, "SDL_UpperBlit");
#ifndef SDL_BlitSurface
	old_SDL2_BlitSurface = (t_SDL2_BlitSurface)
				ga_hook_lookup(handle, "SDL_BlitSurface");
	if(old_SDL2_BlitSurface == NULL) {
		old_SDL2_BlitSurface = old_SDL2_UpperBlit;
		ga_error("SDL hook: BlitSurface == UpperBlit.\n");
	} else {
		ga_error("SDL hook: BlitSurface != UpperBlit.\n");
	}
#else
	old_SDL2_BlitSurface = old_SDL2_UpperBlit;
#endif
	old_SDL2_GetRendererInfo = (t_SDL2_GetRendererInfo)
				ga_hook_lookup_or_quit(handle, "SDL_GetRendererInfo");
	old_SDL2_RenderReadPixels = (t_SDL2_RenderReadPixels)
				ga_hook_lookup_or_quit(handle, "SDL_RenderReadPixels");
	old_SDL2_RenderPresent = (t_SDL2_RenderPresent)
				ga_hook_lookup_or_quit(handle, "SDL_RenderPresent");
	old_SDL2_GL_SwapWindow = (t_SDL2_GL_SwapWindow)
				ga_hook_lookup_or_quit(handle, "SDL_GL_SwapWindow");
	old_SDL2_PollEvent = (t_SDL2_PollEvent)
				ga_hook_lookup_or_quit(handle, "SDL_PollEvent");
	old_SDL2_WaitEvent = (t_SDL2_WaitEvent)
				ga_hook_lookup_or_quit(handle, "SDL_WaitEvent");
	old_SDL2_PeepEvents = (t_SDL2_PeepEvents)
				ga_hook_lookup_or_quit(handle, "SDL_PeepEvents");
	old_SDL2_SetEventFilter = (t_SDL2_SetEventFilter)
				ga_hook_lookup_or_quit(handle, "SDL_SetEventFilter");
	// for internal use
	old_SDL2_CreateRGBSurface = (t_SDL2_CreateRGBSurface)
				ga_hook_lookup_or_quit(handle, "SDL_CreateRGBSurface");
	old_SDL2_FreeSurface = (t_SDL2_FreeSurface)
				ga_hook_lookup_or_quit(handle, "SDL_FreeSurface");
	old_SDL2_PushEvent = (t_SDL2_PushEvent)
				ga_hook_lookup_or_quit(handle, "SDL_PushEvent");
	//
	if((ptr = getenv("HOOKVIDEO")) == NULL)
		goto quit;
	strncpy(soname, ptr, sizeof(soname));
	// hook indirectly
	if((handle = dlopen(soname, RTLD_LAZY)) != NULL) {
	//////////////////////////////////////////////////
	hook_lib_generic(soname, handle, "SDL_Init", (void*) hook_SDL2_Init);
	hook_lib_generic(soname, handle, "SDL_CreateWindow", (void*) hook_SDL2_CreateWindow);
	hook_lib_generic(soname, handle, "SDL_CreateRenderer", (void*) hook_SDL2_CreateRenderer);
	hook_lib_generic(soname, handle, "SDL_CreateTexture", (void*) hook_SDL2_CreateTexture);
	// BlitSurface!?
	if(old_SDL2_BlitSurface != old_SDL2_UpperBlit) {
		hook_lib_generic(soname, handle, "SDL_BlitSurface", (void*) hook_SDL2_BlitSurface);
	} else {
		hook_lib_generic(soname, handle, "SDL_UpperBlit", (void*) hook_SDL2_BlitSurface);
	}
	hook_lib_generic(soname, handle, "SDL_RenderPresent", (void*) hook_SDL2_RenderPresent);
	hook_lib_generic(soname, handle, "SDL_GL_SwapWindow", (void*) hook_SDL2_GL_SwapWindow);
	hook_lib_generic(soname, handle, "SDL_PollEvent", (void*) hook_SDL2_PollEvent);
	hook_lib_generic(soname, handle, "SDL_WaitEvent", (void*) hook_SDL2_WaitEvent);
	hook_lib_generic(soname, handle, "SDL_PeepEvents", (void*) hook_SDL2_PeepEvents);
	hook_lib_generic(soname, handle, "SDL_SetEventFilter", (void*) hook_SDL2_SetEventFilter);
	//
	ga_error("hook-sdl2: hooked into %s\n", soname);
	}
quit:
#endif
	return;
}

static void
sdl2_hook_replay(sdlmsg_t *msg) {
	SDL_Event sdl2evt;
	map<int,int>::iterator mi;
	double scaleX, scaleY;
	sdlmsg_keyboard_t *msgk = (sdlmsg_keyboard_t*) msg;
	sdlmsg_mouse_t *msgm = (sdlmsg_mouse_t*) msg;
	//
	ctrl_server_get_scalefactor(&scaleX, &scaleY);
	//
	bzero(&sdl2evt, sizeof(sdl2evt));
	switch(msg->msgtype) {
	case SDL_EVENT_MSGTYPE_KEYBOARD:
		sdl2evt.key.type =
			msgk->is_pressed ? SDL_KEYDOWN : SDL_KEYUP;
		sdl2evt.key.timestamp = time(0);
		// sdl2evt.key.windowId?
		sdl2evt.key.state =
			msgk->is_pressed ? SDL_PRESSED : SDL_RELEASED;
		sdl2evt.key.keysym.scancode = (SDL_Scancode) msgk->scancode;
		sdl2evt.key.keysym.sym = (SDL_Keycode) msgk->sdlkey;
		sdl2evt.key.keysym.mod = (SDL_Keymod) msgk->sdlmod;
		//
		if(local_filter == NULL) {
			old_SDL2_PushEvent(&sdl2evt);
		} else {
			if(local_filter(&sdl2evt, NULL) != 0)
				old_SDL2_PushEvent(&sdl2evt);
		}
		//ga_error("XXX: PushEvent\n");
		break;
	case SDL_EVENT_MSGTYPE_MOUSEKEY:
		sdl2evt.button.type =
			msgm->is_pressed ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
		sdl2evt.button.timestamp = time(0);
		sdl2evt.button.button =
			msgm->mousebutton > 5 ? msgm->mousebutton - 2 : msgm->mousebutton;
		sdl2evt.button.state = msgm->is_pressed ? SDL_PRESSED : SDL_RELEASED;
		sdl2evt.button.x = msgm->mousex * scaleX;
		sdl2evt.button.y = msgm->mousey * scaleY;
		if(local_filter == NULL) {
			old_SDL2_PushEvent(&sdl2evt);
		} else {
			if(local_filter(&sdl2evt, NULL) != 0)
				old_SDL2_PushEvent(&sdl2evt);
		}
		//ga_error("XXX: PushEvent: x=%d*%.2f, y=%dx%.2f\n", msg->mousex, scaleX, msg->mousey, scaleY);
		break;
	case SDL_EVENT_MSGTYPE_MOUSEWHEEL:
		if(msgm->mousey == 0)
			break;
		sdl2evt.wheel.type = SDL_MOUSEWHEEL;
		sdl2evt.wheel.timestamp = time(0);
		sdl2evt.wheel.which = 0;
		sdl2evt.wheel.x = msgm->mousex * scaleX;
		sdl2evt.wheel.y = msgm->mousey * scaleY;
		//sdl2evt.wheel.direction = SDL_MOUSEWHEEL_NORMAL;	// SDL >= 2.0.4
		if(local_filter == NULL) {
			old_SDL2_PushEvent(&sdl2evt);
		} else {
			if(local_filter(&sdl2evt, NULL) != 0)
				old_SDL2_PushEvent(&sdl2evt);
		}
		//ga_error("XXX: PushEvent\n");
		break;
	case SDL_EVENT_MSGTYPE_MOUSEMOTION:
		sdl2evt.motion.type = SDL_MOUSEMOTION;
		sdl2evt.motion.timestamp = time(0);
		sdl2evt.motion.state = msgm->mousestate;
		sdl2evt.motion.x = msgm->mousex * scaleX;
		sdl2evt.motion.y = msgm->mousey * scaleY;
		sdl2evt.motion.xrel = ((short) msgm->mouseRelX) * scaleX;
		sdl2evt.motion.yrel = ((short) msgm->mouseRelY) * scaleY;
		if(local_filter == NULL) {
			old_SDL2_PushEvent(&sdl2evt);
		} else {
			if(local_filter(&sdl2evt, NULL) != 0)
				old_SDL2_PushEvent(&sdl2evt);
		}
		//ga_error("XXX: PushEvent: x=%d*%.2f, y=%dx%.2f\n", msg->mousex, scaleX, msg->mousey, scaleY);
		break;
	}
	return;
}

void
sdl2_hook_replay_callback(void *msg, int msglen) {
	sdlmsg_t *smsg = (sdlmsg_t*) msg;
	sdlmsg_ntoh(smsg);
	if(sdlmsg_key_blocked(smsg)) {
		return;
	}
	sdl2_hook_replay(smsg);
	return;
}

int
hook_SDL2_Init(unsigned int flags) {
	int ret;
	//
#ifndef WIN32
	if(old_SDL2_Init == NULL) {
		sdl2_hook_symbols();
	}
#endif
	//
	if((ret = old_SDL2_Init(flags)) < 0)
		return ret;
	//
	sdl2_hook_init();
	//
	return ret;
}

SDL_Window*
hook_SDL2_CreateWindow(const char *title, int x, int y, int w, int h, uint32_t flags) {
	SDL_Window* ret;
	if(old_SDL2_CreateWindow == NULL) {
		sdl2_hook_symbols();
	}
	if((ret = old_SDL2_CreateWindow(title, x, y, w, h, flags)) != NULL) {
		if(w * h > curr_width * curr_height) {
			curr_window = ret;
			curr_width = w;
			curr_height = h;
		}
	}
	ga_error("XXX: SDL2_CreateWindow [%s] pos=(%d,%d), size=(%d,%d), flags=%08x\n",
		title, x, y, w, h, flags);
	return ret;
}

SDL_Renderer *
hook_SDL2_CreateRenderer(SDL_Window* window, int index, uint32_t flags) {
	SDL_Renderer *ret;
	if(old_SDL2_CreateRenderer == NULL) {
		sdl2_hook_symbols();
	}
	if((ret = old_SDL2_CreateRenderer(window, index, flags)) != NULL) {
		if(window == curr_window) {
			curr_renderer = ret;
		}
	}
	if(ret != NULL && old_SDL2_GetRendererInfo != NULL) {
		do {
		////////////////////////
		int i, pos;
		char formats[2048];
		SDL_RendererInfo info;
		if(old_SDL2_GetRendererInfo(ret, &info) != 0) {
			ga_error("hook_sdl2: cannot get renderer info: %s\n", SDL_GetError());
			break;
		}
		ga_error("hook_sdl2: Renderer[%s] flags=%d(%08x), formats=%d, max=(%d,%d)\n",
			info.name, info.flags, info.flags, info.num_texture_formats,
			info.max_texture_width, info.max_texture_height);
		for(i = 0, pos=0; i < info.num_texture_formats; i++) {
			int n = 0;
			unsigned int f = info.texture_formats[i];
			if((f>>28) != 1) {
				// FourCC
				formats[pos] = ' ';
				formats[pos+1] = (unsigned char) (f & 0xff);
				formats[pos+2] = (unsigned char) ((f>>8) & 0xff);
				formats[pos+3] = (unsigned char) ((f>>16) & 0xff);
				formats[pos+4] = (unsigned char) ((f>>24) & 0xff);
				formats[pos+5] = '\0';
				n = 5;
			} else {
				// not FourCC
				n = snprintf(&formats[pos], sizeof(formats)-pos, " %08x[type=%d,order=%d,layout=%d,bits=%d,bytes=%d]",
					f,
					(f>>24)&0x0f,	/*type*/
					(f>>20)&0x0f,	/*order*/
					(f>>16)&0x0f,	/*layout*/
					(f>>8)&0xff,	/*bits*/
					f&0xff);	/*bytes*/
			}
			pos += n;
		}
		ga_error("hook_sdl2: Renderer[%s] formats:%s\n", info.name, formats);
		////////////////////////
		} while(0);
	} else {
		ga_error("hook_sdl2: No renderer info, renderer=%p, fpointer=%p\n", ret, old_SDL2_GetRendererInfo);
	}
	return ret;
}

SDL_Texture *
hook_SDL2_CreateTexture(SDL_Renderer* renderer, uint32_t format, int access, int w, int h) {
	SDL_Texture *ret;
	if(old_SDL2_CreateTexture == NULL) {
		sdl2_hook_symbols();
	}
	ret = old_SDL2_CreateTexture(renderer, format, access, w, h);
	//ga_error("XXX: SDL_CreateTexture: renderer=%p, format=%d(%08x), access=%08x, w=%d, h=%d, texture=%p\n",
	//	renderer, format, format, access, w, h, ret);
	return ret;
}

static void
hook_SDL2_capture_screen(const char *caller, SDL_Renderer *renderer) {
	static int frame_interval;
	static struct timeval initialTv, captureTv;
	static int sb_initialized = 0;
	dpipe_buffer_t *data;
	vsource_frame_t *frame;
	//
	if(renderer != curr_renderer)
		return;
	//
	if(ga_hook_capture_prepared(curr_width/*dupsurface->w*/, curr_height/*dupsurface->h*/, 0) < 0)
		return;
	//
	if(sb_initialized == 0) {
		frame_interval = 1000000/video_fps; // in the unif of us
		frame_interval++;
		gettimeofday(&initialTv, NULL);
		sb_initialized = 1;
	} else {
		gettimeofday(&captureTv, NULL);
	}
	//
	if (enable_server_rate_control && ga_hook_video_rate_control() < 0)
		return;
	// copy screen
	do {
		data = dpipe_get(g_pipe[0]);
		frame = (vsource_frame_t*) data->pointer;
		frame->pixelformat = AV_PIX_FMT_BGRA;
		frame->realwidth = curr_width; //dupsurface->w;
		frame->realheight = curr_height; //dupsurface->h;
		frame->realstride = curr_width * 4; //dupsurface->pitch;
		frame->realsize = curr_width * curr_height * 4; //dupsurface->h * dupsurface->pitch;
		frame->linesize[0] = curr_width * 4; //dupsurface->pitch;
		//bcopy(dupsurface->pixels, frame->imgbuf, frame->realsize);
		if(old_SDL2_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, frame->imgbuf, curr_width * 4) != 0) {
			ga_error("hook_sdl2: read pixels failed: %s\n", SDL_GetError());
		}
		frame->imgpts = tvdiff_us(&captureTv, &initialTv)/frame_interval;
		frame->timestamp = captureTv;
	} while(0);
	// duplicate from channel 0 to other channels
	ga_hook_capture_dupframe(frame);
	dpipe_store(g_pipe[0], data);
	return;
}

int
hook_SDL2_UpperBlit(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect) {
	int ret;
	//
	if(old_SDL2_UpperBlit == NULL) {
		sdl2_hook_symbols();
	}
	ret = old_SDL2_UpperBlit(src, srcrect, dst, dstrect);
	//
#if 0	/* FIXME */
	if(dst == screensurface) {
		if((screensurface->flags & SDL_HWSURFACE) != 0) {
			hook_SDL2_capture_screen("SDL_BlitSurface");
		}
	}
#endif
	//
	return ret;
}

int
hook_SDL2_BlitSurface(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect) {
	int ret;
	//
	if(old_SDL2_BlitSurface == NULL) {
		sdl2_hook_symbols();
	}
	ret = old_SDL2_BlitSurface(src, srcrect, dst, dstrect);
	//
#if 0	/* FIXME */
	if(dst == screensurface) {
		if((screensurface->flags & SDL_HWSURFACE) != 0) {
			hook_SDL2_capture_screen("SDL_BlitSurface");
		}
	}
#endif
	//
	return ret;
}

void
hook_SDL2_RenderPresent(SDL_Renderer *renderer) {
	if(old_SDL2_RenderPresent == NULL) {
		sdl2_hook_symbols();
	}
	// XXX: need to check renderer?
	hook_SDL2_capture_screen("SDL_RenderPresent", renderer);
	//
	old_SDL2_RenderPresent(renderer);
	return;
}

static void
GL_capture() {
	static int frame_interval;
	static struct timeval initialTv, captureTv;
	static int frameLinesize;
	static unsigned char *frameBuf;
	static int sb_initialized = 0;
	//
	GLint vp[4];
	int vp_x, vp_y, vp_width, vp_height;
	int i;
	dpipe_buffer_t *data;
	vsource_frame_t *frame;
	//
	glGetIntegerv(GL_VIEWPORT, vp);
	vp_x = vp[0];
	vp_y = vp[1];
	vp_width = vp[2];
	vp_height = vp[3];		
	//

	if(ga_hook_capture_prepared(vp_width, vp_height, 0) < 0)
		return;

	if(sb_initialized == 0) {
		frame_interval = 1000000/video_fps; // in the unif of us
		frame_interval++;
		gettimeofday(&initialTv, NULL);
		frameBuf = (unsigned char*) malloc(encoder_width * encoder_height * 4);
		if(frameBuf == NULL) {
			ga_error("allocate frame failed.\n");
			return;
		}
		frameLinesize = game_width * 4;
		sb_initialized = 1;
	} else {
		gettimeofday(&captureTv, NULL);
	}
	
	if (enable_server_rate_control && ga_hook_video_rate_control() < 0)
		return;

	// copy screen
	do {
		unsigned char *src, *dst;
		//
		frameLinesize = game_width<<2;
		//
		data = dpipe_get(g_pipe[0]);
		frame = (vsource_frame_t*) data->pointer;
		frame->pixelformat = AV_PIX_FMT_RGBA;
		frame->realwidth = game_width;
		frame->realheight = game_height;
		frame->realstride = frameLinesize;
		frame->realsize = game_height * frameLinesize;
		frame->linesize[0] = frameLinesize;
		// read a block of pixels from the framebuffer (backbuffer)
		glReadBuffer(GL_BACK);
		glReadPixels(0, 0, game_width, game_height, GL_RGBA, GL_UNSIGNED_BYTE, frameBuf);
		// image is upside down!
		src = frameBuf + frameLinesize * (game_height - 1);
		dst = frame->imgbuf;
		for(i = 0; i < frame->realheight; i++) {
			bcopy(src, dst, frameLinesize);
			dst += frameLinesize;
			src -= frameLinesize;
		}
		frame->imgpts = tvdiff_us(&captureTv, &initialTv)/frame_interval;
		frame->timestamp = captureTv;
	} while(0);

	// duplicate from channel 0 to other channels
	ga_hook_capture_dupframe(frame);
	dpipe_store(g_pipe[0], data);
	
	return;
}

void
hook_SDL2_GL_SwapWindow(SDL_Window *window) {
	//
	if(old_SDL2_GL_SwapWindow == NULL) {
		sdl2_hook_symbols();
	}
	old_SDL2_GL_SwapWindow(window);
	ga_error("XXX: SDL_GL_SwapWindow: window=%p\n", window);
	GL_capture();
	return;
}

void
hook_SDL2_GL_glFlush() {
	if(old_SDL2_GL_glFlush == NULL) {
		sdl2_hook_symbols();
	}
	old_SDL2_GL_glFlush();
	//ga_error("XXX: glFlush called\n");
	GL_capture();
	return;
}

static int
filter_SDL2_Event(SDL_Event *event) {
	if(event->type == SDL_WINDOWEVENT) {
		ga_error("SDL_Event: WindowEvent - event=%d\n",
			event->window.event);
		// will not lose
		if(event->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
			event->window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
		}
		//
		return 1;
	}
	return 0;
}

int
hook_SDL2_PollEvent(SDL_Event *event) {
	int ret;
	if(old_SDL2_PollEvent == NULL) {
		sdl2_hook_symbols();
	}
	ret = old_SDL2_PollEvent(event);
	if(ret > 0 && event != NULL) {
		filter_SDL2_Event(event);
	}
	return ret;
}

int
hook_SDL2_WaitEvent(SDL_Event *event) {
	int ret;
	if(old_SDL2_WaitEvent == NULL) {
		sdl2_hook_symbols();
	}
	ret = old_SDL2_WaitEvent(event);
	if(ret > 0 && event != NULL) {
		filter_SDL2_Event(event);
	}
	return ret;
}

int
hook_SDL2_PeepEvents(SDL_Event *event, int numevents, SDL_eventaction action, uint32_t minType, uint32_t maxType) {
	int i, ret;
	// XXX: altering event messages
	//	SDL_PeepEvents is called by SDL_WaitEvent and SDL_PollEvent
	if(old_SDL2_PeepEvents == NULL) {
		sdl2_hook_symbols();
	}
	if(action == SDL_ADDEVENT)
		return old_SDL2_PeepEvents(event, numevents, action, minType, maxType);
	ret = old_SDL2_PeepEvents(event, numevents, action, minType, maxType);
	if(ret > 0) {
		for(i = 0; i < ret; i++) {
			filter_SDL2_Event(&event[i]);
		}
	}
	return ret;
}

void
hook_SDL2_SetEventFilter(SDL_EventFilter filter, void *userdata) {
	if(old_SDL2_SetEventFilter == NULL) {
		sdl2_hook_symbols();
	}
	local_filter = filter;
	old_SDL2_SetEventFilter(filter, userdata);
	return;
}

#ifndef WIN32	/* POSIX interfaces */
int
SDL_Init(unsigned int flags) {
	return hook_SDL2_Init(flags);
}

SDL_Window *
SDL_CreateWindow(const char *title, int x, int y, int w, int h, uint32_t flags) {
	return hook_SDL2_CreateWindow(title, x, y, w, h, flags);
}

SDL_Renderer *
SDL_CreateRenderer(SDL_Window* window, int index, uint32_t flags) {
	return hook_SDL2_CreateRenderer(window, index, flags);
}

SDL_Texture *
SDL_CreateTexture(SDL_Renderer* renderer, uint32_t format, int access, int w, int h) {
	return hook_SDL2_CreateTexture(renderer, format, access, w, h);
}

#ifndef SDL_BlitSurface
int
SDL_BlitSurface(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect) {
	return hook_SDL2_BlitSurface(src, srcrect, dst, dstrect);
}
#endif

int
SDL_UpperBlit(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect) {
	if(old_SDL2_UpperBlit == old_SDL2_BlitSurface)
		return hook_SDL2_BlitSurface(src, srcrect, dst, dstrect);
	return old_SDL2_UpperBlit(src, srcrect, dst, dstrect);
}

void SDL2_RenderPresent(SDL_Renderer *renderer) {
	old_SDL2_RenderPresent(renderer);
}

void
SDL_GL_SwapWindow(SDL_Window *window) {
	hook_SDL2_GL_SwapWindow(window);
}

int
SDL_PollEvent(SDL_Event *event) {
	return hook_SDL2_PollEvent(event);
}

int
SDL_WaitEvent(SDL_Event *event) {
	return hook_SDL2_WaitEvent(event);
}

int
SDL_PeepEvents(SDL_Event *event, int numevents, SDL_eventaction action, uint32_t minType, uint32_t maxType) {
	return hook_SDL2_PeepEvents(event, numevents, action, minType, maxType);
}

void
SDL_SetEventFilter(SDL_EventFilter filter, void *userdata) {
	hook_SDL2_SetEventFilter(filter, userdata);
}

__attribute__((constructor))
static void
sdl_hook_loaded(void) {
	ga_error("ga-hook-sdl2 loaded!\n");
	if(ga_hook_init() < 0) {
		ga_error("ga_hook: init failed.\n");
		exit(-1);
	}
	return;
}
#endif	/* ! WIN32 */

