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

#ifndef __GA_HOOK_SDLAUDIO_H__
#define __GA_HOOK_SDLAUDIO_H__

#include "sdl12-audio.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef int	(*t_SDL_OpenAudio)(SDL12_AudioSpec *, SDL12_AudioSpec *);
typedef void	(*t_SDL_PauseAudio)(int);
typedef void	(*t_SDL_CloseAudio)();
#ifdef __cplusplus
}
#endif

extern t_SDL_OpenAudio		old_SDL_OpenAudio;
extern t_SDL_PauseAudio		old_SDL_PauseAudio;
extern t_SDL_CloseAudio		old_SDL_CloseAudio;

int hook_SDL_OpenAudio(SDL12_AudioSpec *desired, SDL12_AudioSpec *obtained);
void hook_SDL_PauseAudio(int pause_on);
void hook_SDL_CloseAudio();

#endif
