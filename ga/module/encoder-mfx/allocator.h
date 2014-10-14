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

#ifndef __ALLOCATOR_H__
#define __ALLOCATOR_H__

#include <mfxvideo.h>

#define MFX_ALIGN16(x)	((((x) + 15) >> 4) << 4)
#define MFX_ALIGN32(x)	((((x) + 31) >> 5) << 5)

mfxStatus ba_alloc(mfxHDL pthis, mfxU32 nbytes, mfxU16 type, mfxMemId *mid);
mfxStatus ba_lock(mfxHDL pthis, mfxMemId mid, mfxU8 **ptr);
mfxStatus ba_unlock(mfxHDL pthis, mfxMemId mid);
mfxStatus ba_free(mfxHDL pthis, mfxMemId mid);
mfxStatus ba_register(mfxSession s);

mfxStatus fa_alloc(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
mfxStatus fa_lock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
mfxStatus fa_unlock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
mfxStatus fa_gethdl(mfxHDL pthis, mfxMemId mid, mfxHDL *handle);
mfxStatus fa_free(mfxHDL pthis, mfxFrameAllocResponse *response);
mfxStatus fa_register(mfxSession s);

mfxFrameSurface1 * frame_pool_alloc(mfxFrameInfo *info, mfxFrameAllocResponse *response);
void frame_pool_free(mfxFrameSurface1 *pool);
mfxFrameSurface1 * frame_pool_get(mfxFrameSurface1 *pool, mfxFrameAllocResponse *response);

#endif /* __ALLOCATOR_H__ */
