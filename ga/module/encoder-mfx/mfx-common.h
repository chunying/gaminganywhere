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

#ifndef __MFX_COMMON_H__
#define	__MFX_COMMON_H__

#include <mfxvideo.h>

#ifdef NO_LIBGA
#define	ga_error			printf
#define VIDEO_SOURCE_CHANNEL_MAX	2
#endif

extern mfxSession _session[VIDEO_SOURCE_CHANNEL_MAX];
extern mfxVideoParam _vppparam[VIDEO_SOURCE_CHANNEL_MAX];
extern mfxFrameAllocResponse _vppresponse[VIDEO_SOURCE_CHANNEL_MAX][2];
extern mfxFrameSurface1 *_vpppool[VIDEO_SOURCE_CHANNEL_MAX][2];

extern mfxVideoParam _encparam[VIDEO_SOURCE_CHANNEL_MAX];
#if 0	// No need because we will use vppout as encoder input
extern mfxFrameAllocResponse _encresponse[VIDEO_SOURCE_CHANNEL_MAX];
extern mfxFrameSurface1 *_encpool[VIDEO_SOURCE_CHANNEL_MAX];
#endif
extern mfxBitstream _mfxbs[VIDEO_SOURCE_CHANNEL_MAX];

int mfx_invalid_status(mfxStatus sts);
int mfx_deinit_internal(int cid);
mfxStatus mfx_init_session(mfxSession *s);
mfxStatus mfx_init_vpp(mfxSession s, int width, int height, int fps);
mfxStatus mfx_init_encoder(mfxSession s, int width, int height, int fps, int bitrateKbps);
mfxStatus mfx_reconfigure(mfxSession s, int bitrateKbps);
mfxStatus mfx_vpp_pool(mfxSession s, mfxVideoParam *param,
		mfxFrameAllocResponse *inres,  mfxFrameSurface1 **inpool,
		mfxFrameAllocResponse *outres, mfxFrameSurface1 **outpool);
mfxStatus mfx_encoder_pool(mfxSession s, mfxVideoParam *param,
		mfxFrameAllocResponse *response, mfxFrameSurface1 **pool);
int mfx_init_internal(int cid, int width, int height, int fps, int bitrateKbps, int useRGB);
mfxStatus mfx_realloc_buffer(mfxSession s, mfxBitstream *pbs);
mfxStatus mfx_encode_vpp(mfxSession s, mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxSyncPoint *syncp);
mfxStatus mfx_encode_encode(mfxSession s, mfxFrameSurface1 *in, mfxBitstream *pbs, mfxSyncPoint *syncp);

#endif

