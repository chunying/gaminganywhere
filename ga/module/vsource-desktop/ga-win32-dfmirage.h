// Copyright (c) 2003-2010 DemoForge, LLC.
// All rights reserved.

#ifndef	__DFMIRAGE_H__
#define	__DFMIRAGE_H__
// NOTE: shared among DISPLAY and R3 projects

//namespace df_mirage_drv
//{

enum
{
	DMF_ESCAPE_BASE_1_VB	= 1030,
	DMF_ESCAPE_BASE_2_VB	= 1026, 
	DMF_ESCAPE_BASE_3_VB	= 24
};

//VB++
#ifdef  _WIN64

#define CLIENT_64BIT   0x8000

enum
{
	DMF_ESCAPE_BASE_1	= CLIENT_64BIT | DMF_ESCAPE_BASE_1_VB,
	DMF_ESCAPE_BASE_2	= CLIENT_64BIT | DMF_ESCAPE_BASE_2_VB, 
	DMF_ESCAPE_BASE_3	= CLIENT_64BIT | DMF_ESCAPE_BASE_3_VB, 
};

#else

enum
{
	DMF_ESCAPE_BASE_1	= DMF_ESCAPE_BASE_1_VB,
	DMF_ESCAPE_BASE_2	= DMF_ESCAPE_BASE_2_VB, 
	DMF_ESCAPE_BASE_3	= DMF_ESCAPE_BASE_3_VB, 
};

#endif

// Define the IOCTL function code
typedef enum
{
	dmf_esc_qry_ver_info	= DMF_ESCAPE_BASE_2+ 0,

//	dmf_esc_connect			= DMF_ESCAPE_BASE_2+ 1,
//	dmf_esc_disconnect		= DMF_ESCAPE_BASE_2+ 2,
//	dmf_esc_session_reset	= DMF_ESCAPE_BASE_2+ 3,

// both legacy and new protocols should be supported;
// dmf_esc_connect/dmf_esc_usm_pipe_map are mutually exclusive.

	dmf_esc_usm_pipe_map	= DMF_ESCAPE_BASE_1+ 0,		// create the R3 mapping of update-stream pipe
	dmf_esc_usm_pipe_unmap	= DMF_ESCAPE_BASE_1+ 1,	// release the mapping; deprecated (read docs)

	dmf_esc_test = DMF_ESCAPE_BASE_1+ 20,
	dmf_esc_usm_pipe_mapping_test = DMF_ESCAPE_BASE_1+ 21,

// 1.0.9.0
	dmf_esc_pointer_shape_get = DMF_ESCAPE_BASE_3,

} dmf_escape;

#define MAXCHANGES_BUF		20000
#define CLIP_LIMIT		50

// operations
typedef enum
{
	dmf_dfo_IGNORE		= 0,
	dmf_dfo_FROM_SCREEN	= 1,
	dmf_dfo_FROM_DIB	= 2,
	dmf_dfo_TO_SCREEN	= 3,

	dmf_dfo_SCREEN_SCREEN	= 11,
	dmf_dfo_BLIT		= 12,
	dmf_dfo_SOLIDFILL	= 13,
	dmf_dfo_BLEND		= 14,
	dmf_dfo_TRANS		= 15,
	dmf_dfo_PLG		= 17,
	dmf_dfo_TEXTOUT		= 18,
	
	dmf_dfo_Ptr_Shape	= 19,
	dmf_dfo_Ptr_Engage	= 48,	// point is used with this record
	dmf_dfo_Ptr_Avert	= 49,

// 1.0.9.0
// mode-assert notifications to manifest PDEV limbo status
	dmf_dfn_assert_on	= 64,	// DrvAssert(TRUE): PDEV reenabled
	dmf_dfn_assert_off	= 65,	// DrvAssert(FALSE): PDEV disabled

} dmf_UpdEvent;

#define NOCACHE 1
#define OLDCACHE 2
#define NEWCACHE 3

struct	CHANGES_RECORD
{
	ULONG	type;		//screen_to_screen, blit, newcache,oldcache
	RECT	rect;
#ifndef DFMIRAGE_LEAN
	RECT	origrect;
	POINT	point;
	ULONG	color;		// number used in cache array
	ULONG	refcolor;	// slot used to pass bitmap data
#endif
};
// sizeof (CHANGES_RECORD) = 52, 8-aligned: 56

typedef CHANGES_RECORD *PCHANGES_RECORD;

struct	CHANGES_BUF
{
// 殥錒 朢 覷?朢?樦樇蠉謺 譇賚歑 ?
// 鴈鐕謽僪? ?瞂貘鳷 鴈蠈贄樥黟,
// 斁緱?朢鋋 朢 儋摳睯譔 鴈鐕謽僪噮 ?鐕謽僗?譇縺
	ULONG	counter;
	CHANGES_RECORD	pointrect[MAXCHANGES_BUF];
};
// 4+ 56*2000 = 112004

#define	DMF_PIPE_SEC_SIZE_DEFAULT	ALIGN64K(sizeof(CHANGES_BUF))

struct GETCHANGESBUF
{
	 CHANGES_BUF *	buffer;
	 PVOID	Userbuffer;
};

typedef	enum
{
// IMPORTANT: this status travels as lower-word of packet's flags;
// it also forms a part of ExtEscape output code
// so, we have 16 bits only
	dmf_sprb_internal_error		= 0x0001,	// non-specific fatal trap
	dmf_sprb_miniport_gen_error	= 0x0004,	// non-specific miniport error
	dmf_sprb_memory_alloc_failed	= 0x0008,	// ...and there were no recover (trap)

	dmf_sprb_pipe_buff_overflow	= 0x0010,
	dmf_sprb_pipe_buff_insufficient	= 0x0020,	// too large blob post attempted. need bigger buffer
	dmf_sprb_pipe_not_ready		= 0x0040,

	dmf_sprb_gdi_err		= 0x0100,	// internal error attributed to GDI

	dmf_sprb_owner_died		= 0x0400,	// owner app died; emergency cleanup have been performed
#define	dmf_sprb_ERRORMASK		0x07ff

// generally, not errors
	dmf_sprb_tgtwnd_gone		= 0x0800,	// target wnd is gone; nothing to capture
//	dmf_sprb_xyz					= 0x1000,
#define	dmf_sprb_STRICTSESSION_AFF	0x1fff

// NON-STRICT SESSION AFFILIATION
	dmf_sprb_pdev_detached		= 0x2000,	// DrvAssertMode: FALSE; transient problem, in general

//// NOTE: these flags are not part of the persistent state;
//// these are only signaled for that commands;
//	dmf_refl_cmd_start		= 0x4000,
//	dmf_refl_cmd_stop		= 0x8000,

} dmf_session_prob_status;

// IMPORTANT: ExtEscape return code is treated as follows:
//	-- in general (Win32 API rules)
//		>0 indicates success,
//		0 indicates not-impl,
//		<0 indicates faulure;
//	-- 0x80000000 is the failure flag :)
#define	DMF_ESC_RET_FAILF		0x80000000
//	-- lower-word (16 positions) is a dmf_session_prob_status
//	   that be [as a result of a call],
#define	DMF_ESC_RET_SSTMASK		0x0000FFFF
//	-- bits 30-16 (15 positions) is an immediate call status properies,
//	   defined (in principle) on per-function basis
#define	DMF_ESC_RET_IMMMASK		0x7FFF0000

typedef	enum
{
	dmf_escret_generic_ok		= 0x00010000,	// everything is hunky dory 8;-)
//	-- this status is a generic success value (when no specific info provided);
//	   here it is to distinguish the value from 0 (when session-status is 0)

// FIXED INTERPRETATION
	dmf_escret_bad_state		= 0x00100000,
	dmf_escret_access_denied	= 0x00200000,	// not yet connected or a left guy
	dmf_escret_bad_buffer_size	= 0x00400000,
	dmf_escret_internal_err		= 0x00800000,

// CASE-TO-CASE
	dmf_escret_out_of_memory	= 0x02000000,
// NOTE: this code for local, not a session-trap failure;
// in case of session trap, the error is reflected in the lower-word
// of retcode (dmf_session_prob_status)

	dmf_escret_already_connected	= 0x04000000,
	dmf_escret_oh_boy_too_late	= 0x08000000,	// i'm already torn ;))
	dmf_escret_bad_window		= 0x10000000,

// NOTE: may be not a error
// NOTE: treated as error with DMF_ESC_RET_FAILF
	dmf_escret_drv_ver_higher	= 0x20000000,	// drv ver higher than app
	dmf_escret_drv_ver_lower	= 0x40000000,	// app ver higher than drv
// <<

} dmf_esc_retcode;


// 2005.11.21
struct	Esc_dmf_Qvi_IN
{
	ULONG	cbSize;

	ULONG	app_actual_version;
	ULONG	display_minreq_version;

	ULONG	connect_options;		// reserved. must be 0.
};

enum
{
	esc_qvi_prod_name_max	= 16,
};

#define	ESC_QVI_PROD_MIRAGE	"MIRAGE"
#define	ESC_QVI_PROD_QUASAR	"QUASAR"

struct	Esc_dmf_Qvi_OUT
{
	ULONG	cbSize;

	ULONG	display_actual_version;
	ULONG	miniport_actual_version;
	ULONG	app_minreq_version;
	ULONG	display_buildno;
	ULONG	miniport_buildno;

// MIRAGE
// QUASAR
	char	prod_name[esc_qvi_prod_name_max];
};

//dmf_esc_pointer_shape_get
struct	Esc_dmf_pointer_shape_get_IN
{
	ULONG	cbSize;
	char *	pDstBmBuf;
	ULONG	nDstBmBufSize;		// (64* 64)* (33/ 8)+ 256* 4 = 17920
};

// connect:
// buff-capacity
//  buff-full evt
//  buff-empty evt

struct	Esc_dmf_pointer_shape_get_OUT
{
// we get
//	mask-bm
//	format of color-bm, color-bm
//	palette for color-bm

	ULONG	cbSize;
	POINTL	BmSize;		// XxY

	char *	pMaskBm;
	ULONG	nMaskBmSize;

	char *	pColorBm;
	ULONG	nColorBmSize;
// if nBitmapSize is more than Esc_dmf_pointer_shape_get_IN::nDstBmBufSize,
// than bitmap is not copied and a larger buffer is required

	char *	pColorBmPal;
	ULONG	nColorBmPalEntries;
};

//};	// namespace df_mirage_drv

#endif	/* __DFMIRAGE_H__ */
