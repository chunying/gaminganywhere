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
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "allocator.h"

static int
bafa_error(const char *fmt, ...) {
#if 1
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
#endif
	return -1;
}

typedef struct mfx_buffer_s {
	mfxU16	type;
	mfxU32	nbytes;
	unsigned alignment;
}	mfx_buffer_t;

typedef struct mfx_frame_s {
	mfxU32		id;
	mfxFrameInfo	info;
}	mfx_frame_t;

mfxStatus
ba_alloc(mfxHDL pthis, mfxU32 nbytes, mfxU16 type, mfxMemId *mid) {
	mfx_buffer_t *mem = NULL;
	//
	if(mid == NULL) {
		bafa_error("ba_alloc: mid == NULL\n");
		return MFX_ERR_NULL_PTR;
	}
#if 0	// CHECK: must be system memory?
	if((type & MFX_MEMTYPE_SYSTEM_MEMORY) == 0) {
		bafa_error("ba_alloc: allocate non-system memory (0x%x)\n", type);
		return MFX_ERR_UNSUPPORTED;
	}
#endif
	if((mem = (mfx_buffer_t*) malloc(sizeof(mfx_buffer_t) + nbytes + 32)) == NULL) {
		bafa_error("ba_alloc: malloc failed (%d+%d+%d)\n",
			sizeof(mfx_buffer_t), nbytes, 32);
		return MFX_ERR_MEMORY_ALLOC;
	}
	memset(mem, 0, sizeof(mfx_buffer_t) + nbytes + 32);
	mem->type = type;
	mem->nbytes = nbytes;
	mem->alignment = ((unsigned) (((mfxU8*) mem) + sizeof(mfx_buffer_t))) & 0x1f;
	mem->alignment = 32 - mem->alignment;
	mem->alignment &= 0x1f;
	*mid = (mfxHDL) mem;
	return MFX_ERR_NONE;
}

mfxStatus
ba_lock(mfxHDL pthis, mfxMemId mid, mfxU8 **ptr) {
	mfx_buffer_t *m = (mfx_buffer_t*) mid;
	if(ptr == NULL) {
		bafa_error("ba_lock: ptr == NULL\n");
		return MFX_ERR_NULL_PTR;
	}
	if(m == NULL) {
		bafa_error("ba_lock: mid == NULL\n");
		return MFX_ERR_INVALID_HANDLE;
	}
	*ptr = ((mfxU8*) m) + m->alignment;
	return MFX_ERR_NONE;
}

mfxStatus
ba_unlock(mfxHDL pthis, mfxMemId mid) {
	return MFX_ERR_NONE;
}

mfxStatus
ba_free(mfxHDL pthis, mfxMemId mid) {
	mfx_buffer_t *m = (mfx_buffer_t*) mid;
	if(m == NULL) {
		bafa_error("ba_free: mid == NULL\n");
		return MFX_ERR_INVALID_HANDLE;
	}
	free(m);
	return MFX_ERR_NONE;
}

mfxStatus
ba_register(mfxSession s) {
	static mfxBufferAllocator ba;
	memset(&ba, 0, sizeof(ba));
	ba.pthis  = &ba;
	ba.Alloc  = ba_alloc;
	ba.Lock   = ba_lock;
	ba.Unlock = ba_unlock;
	ba.Free   = ba_free;
	return MFXVideoCORE_SetBufferAllocator(s, &ba);
}

//////////////////////////////////////////////////////////////////////////////

mfxStatus
fa_alloc(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response) {
	mfxU32 allocwidth, allocheight;
	mfxU32 nbytes;
	mfxMemId *mids;
	int i;

	allocwidth = MFX_ALIGN32(request->Info.Width);
	allocheight = MFX_ALIGN32(request->Info.Height);

	if ((request->Type & MFX_MEMTYPE_SYSTEM_MEMORY) == 0) {
		bafa_error("fa_alloc: non-system memory allocation is not supported.\n");
		return MFX_ERR_UNSUPPORTED;
	}

	switch(request->Info.FourCC) {
	case MFX_FOURCC_YV12:
	case MFX_FOURCC_NV12: nbytes = allocwidth * allocheight * 3 / 2; break;
	case MFX_FOURCC_RGB3: nbytes = allocwidth * allocheight * 3;	break;
	case MFX_FOURCC_RGB4: nbytes = allocwidth * allocheight * 4;	break;
	case MFX_FOURCC_YUY2: nbytes = allocwidth * allocheight * 2;	break;
	case MFX_FOURCC_P010: nbytes = allocwidth * allocheight * 3;	break;
	case MFX_FOURCC_A2RGB10: nbytes = allocwidth * allocheight * 4;	break;
	default:
		bafa_error("fa_alloc: unsupported FourCC (%d,%x)\n",
			request->Info.FourCC, request->Info.FourCC);
		return MFX_ERR_UNSUPPORTED;
	}

	if((mids = (mfxMemId*) malloc(sizeof(mfxMemId) * request->NumFrameSuggested)) == NULL) {
		bafa_error("fa_alloc: malloc failed (%d * %d)\n",
			sizeof(mfxMemId), request->NumFrameSuggested);
		return MFX_ERR_MEMORY_ALLOC;
	}
	memset(mids, 0, sizeof(mfxMemId) * request->NumFrameSuggested);

	for(i = 0; i < request->NumFrameSuggested; i++) {
		mfx_frame_t *frame = NULL;
		//
		if(ba_alloc(NULL, nbytes + MFX_ALIGN32(sizeof(mfx_frame_t)), request->Type, &mids[i]) != MFX_ERR_NONE)
			break;
		if(ba_lock(NULL, mids[i], (mfxU8**) &frame) != MFX_ERR_NONE)
			break;
		memcpy(&frame->info, &request->Info, sizeof(mfxFrameInfo));
		ba_unlock(NULL, &mids[i]);
	}

	if(i < request->NumFrameSuggested) {
		bafa_error("fa_alloc: incomplete request (%d < %d)\n",
			i, request->NumFrameSuggested);
		return MFX_ERR_MEMORY_ALLOC;
	}

	response->NumFrameActual = request->NumFrameSuggested;
	response->mids = mids;

	return MFX_ERR_NONE;
}

mfxStatus
fa_lock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr) {
	mfx_frame_t *frame = NULL;
	mfxU16 allocwidth, allocheight;
	mfxStatus sts;

	if(ptr == NULL) {
		bafa_error("fa_lock: ptr == NULL\n");
		return MFX_ERR_NULL_PTR;
	}

	if((sts = ba_lock(NULL, mid, (mfxU8**) &frame)) != MFX_ERR_NONE) {
		bafa_error("fa_lock: ba_lock(NULL, %p, %p) failed\n", mid, &frame);
		return sts;
	}

	allocwidth = (mfxU16) MFX_ALIGN32(frame->info.Width);
	allocheight = (mfxU16) MFX_ALIGN32(frame->info.Height);
	ptr->B = ptr->Y = (mfxU8*) frame + MFX_ALIGN32(sizeof(mfx_frame_t));

	switch(frame->info.FourCC) {
	case MFX_FOURCC_NV12:
		ptr->U = ptr->Y + allocwidth * allocheight;
		ptr->V = ptr->U + 1;
		ptr->Pitch = allocwidth;
		break;
	case MFX_FOURCC_YV12:
		ptr->V = ptr->Y + allocwidth * allocheight;
		ptr->U = ptr->V + (allocwidth >> 1) * (allocheight >> 1);
		ptr->Pitch = allocwidth;
		break;
	case MFX_FOURCC_YUY2:
		ptr->U = ptr->Y + 1;
		ptr->V = ptr->Y + 3;
		ptr->Pitch = 2 * allocwidth;
		break;
	case MFX_FOURCC_RGB3:
		ptr->G = ptr->B + 1;
		ptr->R = ptr->B + 2;
		ptr->Pitch = 3 * allocwidth;
		break;
	case MFX_FOURCC_RGB4:
	case MFX_FOURCC_A2RGB10:
		ptr->G = ptr->B + 1;
		ptr->R = ptr->B + 2;
		ptr->A = ptr->B + 3;
		ptr->Pitch = 4 * allocwidth;
		break;
	case MFX_FOURCC_P010:
		ptr->U = ptr->Y + allocwidth * allocheight * 2;
		ptr->V = ptr->U + 2;
		ptr->Pitch = allocwidth * 2;
		break;
	default:
		return MFX_ERR_UNSUPPORTED;
	}

	return MFX_ERR_NONE;
}

mfxStatus
fa_unlock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr) {
	mfxStatus sts;
	if((sts = ba_unlock(NULL, mid)) != MFX_ERR_NONE)
		return sts;
	if(ptr != NULL) {
		ptr->Pitch = 0;
		ptr->Y = ptr->U = ptr->V = ptr->A = 0;
	}
	return MFX_ERR_NONE;
}

mfxStatus
fa_gethdl(mfxHDL pthis, mfxMemId mid, mfxHDL *handle) {
      return MFX_ERR_UNSUPPORTED;
}

mfxStatus
fa_free(mfxHDL pthis, mfxFrameAllocResponse *response) {
	if(response == NULL) {
		bafa_error("fa_free: response == NULL\n");
		return MFX_ERR_NULL_PTR;
	}

	if(response->mids == NULL)
		return MFX_ERR_NONE;

	for(int i = 0; i < response->NumFrameActual; i++) {
		if(response->mids[i]) {
			mfxStatus sts = MFX_ERR_NONE;
			if((sts = ba_free(NULL, response->mids[i])) != MFX_ERR_NONE)
				return sts;
		}
	}

	free(response->mids);
	response->mids = NULL;

	return MFX_ERR_NONE;
}

mfxStatus
fa_register(mfxSession s) {
	static mfxFrameAllocator fa;
	memset(&fa, 0, sizeof(fa));
	fa.pthis  = &fa;
	fa.Alloc  = fa_alloc;
	fa.Lock   = fa_lock;
	fa.Unlock = fa_unlock;
	fa.GetHDL = fa_gethdl;
	fa.Free   = fa_free;
	return MFXVideoCORE_SetFrameAllocator(s, &fa);
}

//////////////////////////////////////////////////////////////////////////////

mfxFrameSurface1 *
frame_pool_alloc(mfxFrameInfo *info, mfxFrameAllocResponse *response) {
	mfxFrameSurface1 *pool;
	//
	if(info == NULL || response == NULL)
		return NULL;
	if((pool = (mfxFrameSurface1*) malloc(sizeof(mfxFrameSurface1) * response->NumFrameActual)) == NULL)
		return NULL;
	memset(pool, 0, sizeof(mfxFrameSurface1) * response->NumFrameActual);
	//
	for(int i = 0; i < response->NumFrameActual; i++) {
		memcpy(&pool[i].Info, info, sizeof(mfxFrameInfo));
		pool[i].Data.MemId = response->mids[i];
	}
	return pool;
}

void
frame_pool_free(mfxFrameSurface1 *pool) {
	if(pool != NULL)
		free(pool);
	return;
}

mfxFrameSurface1 *
frame_pool_get(mfxFrameSurface1 *pool, mfxFrameAllocResponse *response) {
	if(pool == NULL || response == NULL)
		return NULL;
	for(int i = 0; i < response->NumFrameActual; i++) {
		if(pool[i].Data.Locked == 0) {
			fa_lock(NULL, pool[i].Data.MemId, &pool[i].Data);
			return &pool[i];
		}
	}
	return NULL;
}

