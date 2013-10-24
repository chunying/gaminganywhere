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

#ifndef __GA_HOOK_GL_H__
#define __GA_HOOK_GL_H__

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
typedef void	(*t_glFlush)(void);
#ifdef __cplusplus
}
#endif

extern t_glFlush	old_glFlush;

void hook_glFlush();

#endif
