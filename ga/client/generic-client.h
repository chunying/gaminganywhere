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

#ifndef __GA_IOS_CLIENT_H__
#define __GA_IOS_CLIENT_H__

#include <stdio.h>

/* XXX: must be pure-C interface */

#ifdef __cplusplus
extern "C" {
#endif

int ga_client_init();

int ga_client_set_host(const char *host);
int ga_client_set_port(int port);
int ga_client_set_object_path(const char *path);
int ga_client_set_rtp_over_tcp(bool enabled);
int ga_client_set_ctrl_enable(bool enabled);
int ga_client_set_ctrl_proto(bool tcp);
int ga_client_set_ctrl_port(int port);
int ga_client_set_builtin_audio(bool enabled);
int ga_client_set_builtin_video(bool enabled);
int ga_client_set_audio_codec(int samplerate, int channels);
int ga_client_set_drop_late_frame(int ms);

int ga_client_send_key(bool pressed, int scancode, int sym, int mod, int unicode);
int ga_client_send_mouse_button(bool pressed, int button, int x, int y);
int ga_client_send_mouse_motion(int x, int y, int xrel, int yrel, int state, bool relative);
int ga_client_send_mouse_wheel(int dx, int dy);
int ga_client_launch_controller();
int ga_client_start();
int ga_client_stop();
int ga_client_cleanup();

#ifdef __cplusplus
}
#endif

#endif /* defined(__GA_IOS_CLIENT_H__) */
