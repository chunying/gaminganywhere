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

#ifndef __GA_HOOK_SDL_H__
#define __GA_HOOK_SDL_H__

#include "sdl12-event.h"
#include "sdl12-video.h"
#include "sdl12-mouse.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef int	(*t_SDL_Init)(unsigned int);
typedef SDL12_Surface * (*t_SDL_SetVideoMode)(int width, int height, int bpp, uint32_t flags);
typedef SDL12_Surface * (*t_SDL_CreateRGBSurface)(uint32_t flags, int width, int height, int depth, uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask);
typedef int	(*t_SDL_UpperBlit)(SDL12_Surface *src, SDL12_Rect *srcrect, SDL12_Surface *dst, SDL12_Rect *dstrect);
typedef int	(*t_SDL_BlitSurface)(SDL12_Surface *src, SDL12_Rect *srcrect, SDL12_Surface *dst, SDL12_Rect *dstrect);
typedef int	(*t_SDL_Flip)(SDL12_Surface *screen);
typedef void	(*t_SDL_UpdateRect)(SDL12_Surface *screen, int32_t x, int32_t y, int32_t w, int32_t h);
typedef void	(*t_SDL_UpdateRects)(SDL12_Surface *screen, int numrects, SDL12_Rect *rects);
typedef void	(*t_SDL_FreeSurface)(SDL12_Surface *surface);
typedef void	(*t_SDL_GL_SwapBuffers)();
typedef int	(*t_SDL_PollEvent)(SDL12_Event *);
typedef int	(*t_SDL_PushEvent)(SDL12_Event *);
typedef int	(*t_SDL_WaitEvent)(SDL12_Event *);
typedef int	(*t_SDL_PeepEvents)(SDL12_Event *, int, SDL12_eventaction, uint32_t);
typedef void	(*t_SDL_SetEventFilter)(SDL12_EventFilter filter);
#ifdef __cplusplus
}
#endif

extern t_SDL_Init		old_SDL_Init;
extern t_SDL_SetVideoMode	old_SDL_SetVideoMode;
extern t_SDL_CreateRGBSurface	old_SDL_CreateRGBSurface;
extern t_SDL_BlitSurface	old_SDL_BlitSurface;
extern t_SDL_UpperBlit		old_SDL_UpperBlit;
extern t_SDL_Flip		old_SDL_Flip;
extern t_SDL_UpdateRect		old_SDL_UpdateRect;
extern t_SDL_UpdateRects	old_SDL_UpdateRects;
extern t_SDL_FreeSurface	old_SDL_FreeSurface;
extern t_SDL_GL_SwapBuffers	old_SDL_GL_SwapBuffers;
extern t_SDL_PollEvent		old_SDL_PollEvent;
extern t_SDL_PushEvent		old_SDL_PushEvent;
extern t_SDL_WaitEvent		old_SDL_WaitEvent;
extern t_SDL_PeepEvents		old_SDL_PeepEvents;
extern t_SDL_SetEventFilter	old_SDL_SetEventFilter;

int sdl_hook_init();

int hook_SDL_Init(unsigned int flags);
SDL12_Surface * hook_SDL_SetVideoMode(int width, int height, int bpp, uint32_t flags);
int hook_SDL_BlitSurface(SDL12_Surface *src, SDL12_Rect *srcrect, SDL12_Surface *dst, SDL12_Rect *dstrect);
int hook_SDL_Flip(SDL12_Surface *screen);
void hook_SDL_FreeSurface(SDL12_Surface *surface);
void hook_SDL_UpdateRect(SDL12_Surface *screen, int32_t x, int32_t y, int32_t w, int32_t h);
void hook_SDL_UpdateRects(SDL12_Surface *screen, int numrects, SDL12_Rect *rects);
void hook_SDL_GL_SwapBuffers();
int hook_SDL_PollEvent(SDL12_Event *event);
int hook_SDL_WaitEvent(SDL12_Event *event);
int hook_SDL_PeepEvents(SDL12_Event *event, int numevents, SDL12_eventaction action, uint32_t mask);
void hook_SDL_SetEventFilter(SDL12_EventFilter filter);

void sdl12_mapinit();
void sdl_hook_replay_callback(void *msg, int msglen);

#endif
