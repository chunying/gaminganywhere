/*
 * Copyright (c) 2013-2014 Chun-Ying Huang
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

#ifndef __XCAP_XWIN_H__
#define	__XCAP_XWIN_H__

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

#include "ga-common.h"

#ifdef __cplusplus
extern "C" {
#endif
//int	ga_xwin_init(const char *displayname, Display **pdisp, Window *proot, XImage **pimg);
//void	ga_xwin_deinit(Display *display, XImage *image);
int	ga_xwin_init(const char *displayname, gaImage *gaimg);
void	ga_xwin_deinit();
void	ga_xwin_imageinfo(XImage *image);
void	ga_xwin_capture(char *buf, int buflen, struct gaRect *rect);
#ifdef __cplusplus
}
#endif

#endif
