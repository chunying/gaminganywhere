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

#ifndef __CTRL_SDL_H__
#define __CTRL_SDL_H__

#include <SDL2/SDL_version.h>
#include <SDL2/SDL_keycode.h>

#include "ga-common.h"
#include "ga-module.h"
#include "rtspconf.h"

#define	SDL_EVENT_MSGTYPE_NULL		0
#define	SDL_EVENT_MSGTYPE_KEYBOARD	1
#define	SDL_EVENT_MSGTYPE_MOUSEKEY	2
#define SDL_EVENT_MSGTYPE_MOUSEMOTION	3
#define SDL_EVENT_MSGTYPE_MOUSEWHEEL	4

#ifdef WIN32
#pragma pack(push, 1)
#endif
struct sdlmsg {
	unsigned short msgsize;		// size of this data-structure
					// every message MUST start from a
					// unsigned short message size
					// the size includes the 'msgsize'
	unsigned char msgtype;
	unsigned char is_pressed;	// for keyboard/mousekey
	unsigned char mousebutton;	// mouse button
	unsigned char mousestate;	// mouse state - key combinations for motion
#if SDL_VERSION_ATLEAST(2,0,0)
	unsigned char unused1;		// padding - 3+1 chars
	unsigned short scancode;	// keyboard scan code
	int sdlkey;			// SDLKey value
	unsigned int unicode;		// unicode or ASCII value
#else
	unsigned char scancode;		// keyboard scan code
	unsigned short sdlkey;		// SDLKey value
	unsigned short unicode;		// unicode or ASCII value
#endif
	unsigned short sdlmod;		// SDLMod value
	unsigned short mousex;		// mouse position (big-endian)
	unsigned short mousey;		// mouse position (big-endian)
	unsigned short mouseRelX;	// mouse relative position (big-endian)
	unsigned short mouseRelY;	// mouse relative position (big-endian)
	unsigned char relativeMouseMode;// relative mouse mode?
	unsigned char padding[8];	// reserved padding
}
#ifdef WIN32
#pragma pack(pop)
#else
__attribute__((__packed__))
#endif
;

struct sdlmsg* sdlmsg_ntoh(struct sdlmsg *msg);

#if SDL_VERSION_ATLEAST(2,0,0)
struct sdlmsg* sdlmsg_keyboard(struct sdlmsg *msg, unsigned char pressed, unsigned short scancode, SDL_Keycode key, unsigned short mod, unsigned int unicode);
struct sdlmsg* sdlmsg_mousewheel(struct sdlmsg *msg, unsigned short mousex, unsigned short mousey);
#else
struct sdlmsg* sdlmsg_keyboard(struct sdlmsg *msg, unsigned char pressed, unsigned char scancode, SDLKey key, SDLMod mod, unsigned short unicode);
#endif
struct sdlmsg* sdlmsg_mousekey(struct sdlmsg *msg, unsigned char pressed, unsigned char button, unsigned short x, unsigned short y);
struct sdlmsg* sdlmsg_mousemotion(struct sdlmsg *msg, unsigned short mousex, unsigned short mousey, unsigned short relx, unsigned short rely, unsigned char state, int relativeMouseMode);

MODULE MODULE_EXPORT int sdlmsg_replay_init(void *arg);
MODULE MODULE_EXPORT void sdlmsg_replay_deinit(void *arg);
int sdlmsg_replay(struct sdlmsg *msg);
void sdlmsg_replay_callback(void *msg, int msglen);

#endif /* __CTRL_SDL_H__ */
