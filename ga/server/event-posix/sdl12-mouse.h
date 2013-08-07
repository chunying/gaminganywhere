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

#ifndef __SDL12_MOUSE__
#define __SDL12_MOUSE__

#define SDL12_BUTTON(X)		(1 << ((X)-1))
#define SDL12_BUTTON_LEFT		1
#define SDL12_BUTTON_MIDDLE	2
#define SDL12_BUTTON_RIGHT	3
#define SDL12_BUTTON_WHEELUP	4
#define SDL12_BUTTON_WHEELDOWN	5
#define SDL12_BUTTON_X1		6
#define SDL12_BUTTON_X2		7
#define SDL12_BUTTON_LMASK        SDL_BUTTON(SDL_BUTTON_LEFT)
#define SDL12_BUTTON_MMASK        SDL_BUTTON(SDL_BUTTON_MIDDLE)
#define SDL12_BUTTON_RMASK        SDL_BUTTON(SDL_BUTTON_RIGHT)
#define SDL12_BUTTON_X1MASK       SDL_BUTTON(SDL_BUTTON_X1)
#define SDL12_BUTTON_X2MASK       SDL_BUTTON(SDL_BUTTON_X2)

#endif
