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

#ifndef __SDL12_VIDEO_H__
#define __SDL12_VIDEO_H__

/** Available for SDL_CreateRGBSurface() or SDL_SetVideoMode() */
/*@{*/
#define SDL12_SWSURFACE	0x00000000	/**< Surface is in system memory */
#define SDL12_HWSURFACE	0x00000001	/**< Surface is in video memory */
#define SDL12_ASYNCBLIT	0x00000004	/**< Use asynchronous blits if possible */
/*@}*/

/** Available for SDL_SetVideoMode() */
/*@{*/
#define SDL_ANYFORMAT	0x10000000	/**< Allow any video depth/pixel-format */
#define SDL_HWPALETTE	0x20000000	/**< Surface has exclusive palette */
#define SDL_DOUBLEBUF	0x40000000	/**< Set up double-buffered video mode */
#define SDL_FULLSCREEN	0x80000000	/**< Surface is a full screen display */
#define SDL_OPENGL      0x00000002      /**< Create an OpenGL rendering context */
#define SDL_OPENGLBLIT	0x0000000A	/**< Create an OpenGL rendering context and use it for blitting */
#define SDL_RESIZABLE	0x00000010	/**< This video mode may be resized */
#define SDL_NOFRAME	0x00000020	/**< No window caption or edge frame */
/*@}*/

typedef struct SDL12_Rect {
	int16_t x, y;
	uint16_t w, h;
} SDL12_Rect;

typedef struct SDL12_Color {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t unused;
} SDL12_Color;

typedef struct SDL12_Palette {
	int       ncolors;
	SDL12_Color *colors;
} SDL12_Palette;

typedef struct SDL12_PixelFormat {
	SDL12_Palette *palette;
	uint8_t  BitsPerPixel;
	uint8_t  BytesPerPixel;
	uint8_t  Rloss;
	uint8_t  Gloss;
	uint8_t  Bloss;
	uint8_t  Aloss;
	uint8_t  Rshift;
	uint8_t  Gshift;
	uint8_t  Bshift;
	uint8_t  Ashift;
	uint32_t Rmask;
	uint32_t Gmask;
	uint32_t Bmask;
	uint32_t Amask;

	/** RGB color key information */
	uint32_t colorkey;
	/** Alpha value information (per-surface alpha) */
	uint8_t  alpha;
} SDL12_PixelFormat;

typedef struct SDL12_Surface {
	uint32_t flags;				/**< Read-only */
	SDL12_PixelFormat *format;		/**< Read-only */
	int w, h;				/**< Read-only */
	uint16_t pitch;				/**< Read-only */
	void *pixels;				/**< Read-write */
	int offset;				/**< Private */

	/** Hardware-specific surface info */
	struct private_hwdata *hwdata;

	/** clipping information */
	SDL12_Rect clip_rect;			/**< Read-only */
	uint32_t unused1;				/**< for binary compatibility */

	/** Allow recursive locks */
	uint32_t locked;				/**< Private */

	/** info for fast blit mapping to other surfaces */
	struct SDL_BlitMap *map;		/**< Private */

	/** format version, bumped at every change to invalidate blit maps */
	unsigned int format_version;		/**< Private */

	/** Reference count -- used when freeing surface */
	int refcount;				/**< Read-mostly */
} SDL12_Surface;

#endif
