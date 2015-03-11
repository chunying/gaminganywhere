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

#ifndef __GA_HOOK_SDL2_H__
#define __GA_HOOK_SDL2_H__

#ifndef __GA_SDL2_INCLUDED__
#define __GA_SDL2_INCLUDED__
#include <SDL2/SDL.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
typedef int	(*t_SDL2_Init)(unsigned int);
typedef SDL_Window * (*t_SDL2_CreateWindow)(const char *title, int x, int y, int w, int h, uint32_t flags);
typedef SDL_Renderer * (*t_SDL2_CreateRenderer)(SDL_Window* window, int index, uint32_t flags);
typedef SDL_Texture * (*t_SDL2_CreateTexture)(SDL_Renderer* renderer, uint32_t format, int access, int w, int h);
typedef SDL_Surface * (*t_SDL2_CreateRGBSurface)(uint32_t flags, int width, int height, int depth, uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask);
typedef int	(*t_SDL2_UpperBlit)(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect);
typedef int	(*t_SDL2_BlitSurface)(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect);
typedef int	(*t_SDL2_GetRendererInfo)(SDL_Renderer *renderer, SDL_RendererInfo *info);
typedef int	(*t_SDL2_RenderReadPixels)(SDL_Renderer *renderer, const SDL_Rect* rect, uint32_t format, void* pixels, int pitch);
typedef void	(*t_SDL2_RenderPresent)(SDL_Renderer* renderer);
typedef void	(*t_SDL2_FreeSurface)(SDL_Surface *surface);
typedef void	(*t_SDL2_GL_SwapWindow)(SDL_Window* window);
typedef void	(*t_SDL2_GL_glFlush)(void);
typedef int	(*t_SDL2_PollEvent)(SDL_Event *e);
typedef int	(*t_SDL2_PushEvent)(SDL_Event *e);
typedef int	(*t_SDL2_WaitEvent)(SDL_Event *e);
typedef int	(*t_SDL2_PeepEvents)(SDL_Event *e, int numevents, SDL_eventaction action, uint32_t minType, uint32_t maxType);
typedef void	(*t_SDL2_SetEventFilter)(SDL_EventFilter filter, void *userdata);
#ifdef __cplusplus
}
#endif

extern t_SDL2_Init		old_SDL2_Init;
extern t_SDL2_CreateWindow	old_SDL2_CreateWindow;
extern t_SDL2_CreateRenderer	old_SDL2_CreateRenderer;
extern t_SDL2_CreateTexture	old_SDL2_CreateTexture;
extern t_SDL2_CreateRGBSurface	old_SDL2_CreateRGBSurface;
extern t_SDL2_UpperBlit		old_SDL2_UpperBlit;
extern t_SDL2_BlitSurface	old_SDL2_BlitSurface;
extern t_SDL2_GetRendererInfo	old_SDL2_GetRendererInfo;
extern t_SDL2_RenderReadPixels	old_SDL2_RenderReadPixels;
extern t_SDL2_RenderPresent	old_SDL2_RenderPresent;
extern t_SDL2_FreeSurface	old_SDL2_FreeSurface;
extern t_SDL2_GL_SwapWindow	old_SDL2_GL_SwapWindow;
extern t_SDL2_GL_glFlush	old_SDL2_GL_glFlush;
extern t_SDL2_PollEvent		old_SDL2_PollEvent;
extern t_SDL2_PushEvent		old_SDL2_PushEvent;
extern t_SDL2_WaitEvent		old_SDL2_WaitEvent;
extern t_SDL2_PeepEvents	old_SDL2_PeepEvents;
extern t_SDL2_SetEventFilter	old_SDL2_SetEventFilter;

int sdl2_hook_init();

int hook_SDL2_Init(unsigned int flags);
SDL_Window* hook_SDL2_CreateWindow(const char *title, int x, int y, int w, int h, uint32_t flags);
SDL_Renderer * hook_SDL2_CreateRenderer(SDL_Window* window, int index, uint32_t flags);
SDL_Texture * hook_SDL2_CreateTexture(SDL_Renderer* renderer, uint32_t format, int access, int w, int h);
int hook_SDL2_UpperBlit(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect);
int hook_SDL2_BlitSurface(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect);
void hook_SDL2_RenderPresent(SDL_Renderer *renderer);
void hook_SDL2_FreeSurface(SDL_Surface *surface);
void hook_SDL2_GL_SwapWindow(SDL_Window *window);
void hook_SDL2_GL_glFlush(void);
int hook_SDL2_PollEvent(SDL_Event *event);
int hook_SDL2_WaitEvent(SDL_Event *event);
int hook_SDL2_PeepEvents(SDL_Event *event, int numevents, SDL_eventaction action, uint32_t minType, uint32_t maxType);
void hook_SDL2_SetEventFilter(SDL_EventFilter filter, void *userdata);

void sdl2_mapinit();
void sdl2_hook_replay_callback(void *msg, int msglen);

#endif
