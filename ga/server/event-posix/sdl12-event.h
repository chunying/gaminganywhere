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

#ifndef __SDL12_EVENT_H__
#define __SDL12_EVENT_H__

#include <SDL2/SDL_keycode.h>

#define SDL_RELEASED	0
#define	SDL_PRESSED	1

typedef enum {
	SDL12_ADDEVENT,
	SDL12_PEEKEVENT,
	SDL12_GETEVENT
} SDL12_eventaction;

/* SDL 1.2 event type */
/** Event enumerations */
typedef enum {
       SDL12_NOEVENT = 0,			/**< Unused (do not remove) */
       SDL12_ACTIVEEVENT,			/**< Application loses/gains visibility */
       SDL12_KEYDOWN,			/**< Keys pressed */
       SDL12_KEYUP,			/**< Keys released */
       SDL12_MOUSEMOTION,			/**< Mouse moved */
       SDL12_MOUSEBUTTONDOWN,		/**< Mouse button pressed */
       SDL12_MOUSEBUTTONUP,		/**< Mouse button released */
       SDL12_JOYAXISMOTION,		/**< Joystick axis motion */
       SDL12_JOYBALLMOTION,		/**< Joystick trackball motion */
       SDL12_JOYHATMOTION,		/**< Joystick hat position change */
       SDL12_JOYBUTTONDOWN,		/**< Joystick button pressed */
       SDL12_JOYBUTTONUP,			/**< Joystick button released */
       SDL12_QUIT,			/**< User-requested quit */
       SDL12_SYSWMEVENT,			/**< System specific event */
       SDL12_EVENT_RESERVEDA,		/**< Reserved for future use.. */
       SDL12_EVENT_RESERVEDB,		/**< Reserved for future use.. */
       SDL12_VIDEORESIZE,			/**< User resized video mode */
       SDL12_VIDEOEXPOSE,			/**< Screen needs to be redrawn */
       SDL12_EVENT_RESERVED2,		/**< Reserved for future use.. */
       SDL12_EVENT_RESERVED3,		/**< Reserved for future use.. */
       SDL12_EVENT_RESERVED4,		/**< Reserved for future use.. */
       SDL12_EVENT_RESERVED5,		/**< Reserved for future use.. */
       SDL12_EVENT_RESERVED6,		/**< Reserved for future use.. */
       SDL12_EVENT_RESERVED7,		/**< Reserved for future use.. */
       /** Events SDL_USEREVENT through SDL_MAXEVENTS-1 are for your use */
       SDL12_USEREVENT = 24,
       /** This last event is only for bounding internal arrays
	*  It is the number of bits in the event mask datatype -- Uint32
        */
       SDL12_NUMEVENTS = 32
} SDL12_EventType;


typedef enum {
        /** @name ASCII mapped keysyms
         *  The keyboard syms have been cleverly chosen to map to ASCII
         */
        /*@{*/
	SDLK12_UNKNOWN		= 0,
	SDLK12_FIRST		= 0,
	SDLK12_BACKSPACE		= 8,
	SDLK12_TAB		= 9,
	SDLK12_CLEAR		= 12,
	SDLK12_RETURN		= 13,
	SDLK12_PAUSE		= 19,
	SDLK12_ESCAPE		= 27,
	SDLK12_SPACE		= 32,
	SDLK12_EXCLAIM		= 33,
	SDLK12_QUOTEDBL		= 34,
	SDLK12_HASH		= 35,
	SDLK12_DOLLAR		= 36,
	SDLK12_AMPERSAND		= 38,
	SDLK12_QUOTE		= 39,
	SDLK12_LEFTPAREN		= 40,
	SDLK12_RIGHTPAREN		= 41,
	SDLK12_ASTERISK		= 42,
	SDLK12_PLUS		= 43,
	SDLK12_COMMA		= 44,
	SDLK12_MINUS		= 45,
	SDLK12_PERIOD		= 46,
	SDLK12_SLASH		= 47,
	SDLK12_0			= 48,
	SDLK12_1			= 49,
	SDLK12_2			= 50,
	SDLK12_3			= 51,
	SDLK12_4			= 52,
	SDLK12_5			= 53,
	SDLK12_6			= 54,
	SDLK12_7			= 55,
	SDLK12_8			= 56,
	SDLK12_9			= 57,
	SDLK12_COLON		= 58,
	SDLK12_SEMICOLON		= 59,
	SDLK12_LESS		= 60,
	SDLK12_EQUALS		= 61,
	SDLK12_GREATER		= 62,
	SDLK12_QUESTION		= 63,
	SDLK12_AT			= 64,
	/* 
	   Skip uppercase letters
	 */
	SDLK12_LEFTBRACKET	= 91,
	SDLK12_BACKSLASH		= 92,
	SDLK12_RIGHTBRACKET	= 93,
	SDLK12_CARET		= 94,
	SDLK12_UNDERSCORE		= 95,
	SDLK12_BACKQUOTE		= 96,
	SDLK12_a			= 97,
	SDLK12_b			= 98,
	SDLK12_c			= 99,
	SDLK12_d			= 100,
	SDLK12_e			= 101,
	SDLK12_f			= 102,
	SDLK12_g			= 103,
	SDLK12_h			= 104,
	SDLK12_i			= 105,
	SDLK12_j			= 106,
	SDLK12_k			= 107,
	SDLK12_l			= 108,
	SDLK12_m			= 109,
	SDLK12_n			= 110,
	SDLK12_o			= 111,
	SDLK12_p			= 112,
	SDLK12_q			= 113,
	SDLK12_r			= 114,
	SDLK12_s			= 115,
	SDLK12_t			= 116,
	SDLK12_u			= 117,
	SDLK12_v			= 118,
	SDLK12_w			= 119,
	SDLK12_x			= 120,
	SDLK12_y			= 121,
	SDLK12_z			= 122,
	SDLK12_DELETE		= 127,
	/* End of ASCII mapped keysyms */
        /*@}*/

	/** @name International keyboard syms */
        /*@{*/
	SDLK12_WORLD_0		= 160,		/* 0xA0 */
	SDLK12_WORLD_1		= 161,
	SDLK12_WORLD_2		= 162,
	SDLK12_WORLD_3		= 163,
	SDLK12_WORLD_4		= 164,
	SDLK12_WORLD_5		= 165,
	SDLK12_WORLD_6		= 166,
	SDLK12_WORLD_7		= 167,
	SDLK12_WORLD_8		= 168,
	SDLK12_WORLD_9		= 169,
	SDLK12_WORLD_10		= 170,
	SDLK12_WORLD_11		= 171,
	SDLK12_WORLD_12		= 172,
	SDLK12_WORLD_13		= 173,
	SDLK12_WORLD_14		= 174,
	SDLK12_WORLD_15		= 175,
	SDLK12_WORLD_16		= 176,
	SDLK12_WORLD_17		= 177,
	SDLK12_WORLD_18		= 178,
	SDLK12_WORLD_19		= 179,
	SDLK12_WORLD_20		= 180,
	SDLK12_WORLD_21		= 181,
	SDLK12_WORLD_22		= 182,
	SDLK12_WORLD_23		= 183,
	SDLK12_WORLD_24		= 184,
	SDLK12_WORLD_25		= 185,
	SDLK12_WORLD_26		= 186,
	SDLK12_WORLD_27		= 187,
	SDLK12_WORLD_28		= 188,
	SDLK12_WORLD_29		= 189,
	SDLK12_WORLD_30		= 190,
	SDLK12_WORLD_31		= 191,
	SDLK12_WORLD_32		= 192,
	SDLK12_WORLD_33		= 193,
	SDLK12_WORLD_34		= 194,
	SDLK12_WORLD_35		= 195,
	SDLK12_WORLD_36		= 196,
	SDLK12_WORLD_37		= 197,
	SDLK12_WORLD_38		= 198,
	SDLK12_WORLD_39		= 199,
	SDLK12_WORLD_40		= 200,
	SDLK12_WORLD_41		= 201,
	SDLK12_WORLD_42		= 202,
	SDLK12_WORLD_43		= 203,
	SDLK12_WORLD_44		= 204,
	SDLK12_WORLD_45		= 205,
	SDLK12_WORLD_46		= 206,
	SDLK12_WORLD_47		= 207,
	SDLK12_WORLD_48		= 208,
	SDLK12_WORLD_49		= 209,
	SDLK12_WORLD_50		= 210,
	SDLK12_WORLD_51		= 211,
	SDLK12_WORLD_52		= 212,
	SDLK12_WORLD_53		= 213,
	SDLK12_WORLD_54		= 214,
	SDLK12_WORLD_55		= 215,
	SDLK12_WORLD_56		= 216,
	SDLK12_WORLD_57		= 217,
	SDLK12_WORLD_58		= 218,
	SDLK12_WORLD_59		= 219,
	SDLK12_WORLD_60		= 220,
	SDLK12_WORLD_61		= 221,
	SDLK12_WORLD_62		= 222,
	SDLK12_WORLD_63		= 223,
	SDLK12_WORLD_64		= 224,
	SDLK12_WORLD_65		= 225,
	SDLK12_WORLD_66		= 226,
	SDLK12_WORLD_67		= 227,
	SDLK12_WORLD_68		= 228,
	SDLK12_WORLD_69		= 229,
	SDLK12_WORLD_70		= 230,
	SDLK12_WORLD_71		= 231,
	SDLK12_WORLD_72		= 232,
	SDLK12_WORLD_73		= 233,
	SDLK12_WORLD_74		= 234,
	SDLK12_WORLD_75		= 235,
	SDLK12_WORLD_76		= 236,
	SDLK12_WORLD_77		= 237,
	SDLK12_WORLD_78		= 238,
	SDLK12_WORLD_79		= 239,
	SDLK12_WORLD_80		= 240,
	SDLK12_WORLD_81		= 241,
	SDLK12_WORLD_82		= 242,
	SDLK12_WORLD_83		= 243,
	SDLK12_WORLD_84		= 244,
	SDLK12_WORLD_85		= 245,
	SDLK12_WORLD_86		= 246,
	SDLK12_WORLD_87		= 247,
	SDLK12_WORLD_88		= 248,
	SDLK12_WORLD_89		= 249,
	SDLK12_WORLD_90		= 250,
	SDLK12_WORLD_91		= 251,
	SDLK12_WORLD_92		= 252,
	SDLK12_WORLD_93		= 253,
	SDLK12_WORLD_94		= 254,
	SDLK12_WORLD_95		= 255,		/* 0xFF */
        /*@}*/

	/** @name Numeric keypad */
        /*@{*/
	SDLK12_KP0		= 256,
	SDLK12_KP1		= 257,
	SDLK12_KP2		= 258,
	SDLK12_KP3		= 259,
	SDLK12_KP4		= 260,
	SDLK12_KP5		= 261,
	SDLK12_KP6		= 262,
	SDLK12_KP7		= 263,
	SDLK12_KP8		= 264,
	SDLK12_KP9		= 265,
	SDLK12_KP_PERIOD		= 266,
	SDLK12_KP_DIVIDE		= 267,
	SDLK12_KP_MULTIPLY	= 268,
	SDLK12_KP_MINUS		= 269,
	SDLK12_KP_PLUS		= 270,
	SDLK12_KP_ENTER		= 271,
	SDLK12_KP_EQUALS		= 272,
        /*@}*/

	/** @name Arrows + Home/End pad */
        /*@{*/
	SDLK12_UP			= 273,
	SDLK12_DOWN		= 274,
	SDLK12_RIGHT		= 275,
	SDLK12_LEFT		= 276,
	SDLK12_INSERT		= 277,
	SDLK12_HOME		= 278,
	SDLK12_END		= 279,
	SDLK12_PAGEUP		= 280,
	SDLK12_PAGEDOWN		= 281,
        /*@}*/

	/** @name Function keys */
        /*@{*/
	SDLK12_F1			= 282,
	SDLK12_F2			= 283,
	SDLK12_F3			= 284,
	SDLK12_F4			= 285,
	SDLK12_F5			= 286,
	SDLK12_F6			= 287,
	SDLK12_F7			= 288,
	SDLK12_F8			= 289,
	SDLK12_F9			= 290,
	SDLK12_F10		= 291,
	SDLK12_F11		= 292,
	SDLK12_F12		= 293,
	SDLK12_F13		= 294,
	SDLK12_F14		= 295,
	SDLK12_F15		= 296,
        /*@}*/

	/** @name Key state modifier keys */
        /*@{*/
	SDLK12_NUMLOCK		= 300,
	SDLK12_CAPSLOCK		= 301,
	SDLK12_SCROLLOCK		= 302,
	SDLK12_RSHIFT		= 303,
	SDLK12_LSHIFT		= 304,
	SDLK12_RCTRL		= 305,
	SDLK12_LCTRL		= 306,
	SDLK12_RALT		= 307,
	SDLK12_LALT		= 308,
	SDLK12_RMETA		= 309,
	SDLK12_LMETA		= 310,
	SDLK12_LSUPER		= 311,		/**< Left "Windows" key */
	SDLK12_RSUPER		= 312,		/**< Right "Windows" key */
	SDLK12_MODE		= 313,		/**< "Alt Gr" key */
	SDLK12_COMPOSE		= 314,		/**< Multi-key compose key */
        /*@}*/

	/** @name Miscellaneous function keys */
        /*@{*/
	SDLK12_HELP		= 315,
	SDLK12_PRINT		= 316,
	SDLK12_SYSREQ		= 317,
	SDLK12_BREAK		= 318,
	SDLK12_MENU		= 319,
	SDLK12_POWER		= 320,		/**< Power Macintosh power key */
	SDLK12_EURO		= 321,		/**< Some european keyboards */
	SDLK12_UNDO		= 322,		/**< Atari keyboard has Undo */
        /*@}*/

	/* Add any other keys here */

	SDLK12_LAST
} SDL12Key;

/*
#define KMOD_CTRL	(KMOD_LCTRL|KMOD_RCTRL)
#define KMOD_SHIFT	(KMOD_LSHIFT|KMOD_RSHIFT)
#define KMOD_ALT	(KMOD_LALT|KMOD_RALT)
#define KMOD_META	(KMOD_LMETA|KMOD_RMETA)
*/

typedef struct SDL12_keysym {
	uint8_t scancode;		/**< hardware specific scancode */
	SDL12Key sym;			/**< SDL virtual keysym */
	/*SDLMod*/SDL_Keymod mod;	/**< current key modifiers */
	uint16_t unicode;		/**< translated character */
} SDL12_keysym;

#define SDL_APPMOUSEFOCUS	0x01	/**< The app has mouse coverage */
#define SDL_APPINPUTFOCUS	0x02	/**< The app has input focus */
#define SDL_APPACTIVE		0x04	/**< The application is active */

/** Application visibility event structure */
typedef struct SDL12_ActiveEvent {
	uint8_t type;	/**< SDL_ACTIVEEVENT */
	uint8_t gain;	/**< Whether given states were gained or lost (1/0) */
 	uint8_t state;	/**< A mask of the focus states */
} SDL12_ActiveEvent;

/** Keyboard event structure */
typedef struct SDL12_KeyboardEvent {
	uint8_t type;	/**< SDL_KEYDOWN or SDL_KEYUP */
	uint8_t which;	/**< The keyboard device index */
	uint8_t state;	/**< SDL_PRESSED or SDL_RELEASED */
	SDL12_keysym keysym;  // need to include SDL.h
} SDL12_KeyboardEvent;

/** Mouse motion event */
typedef struct SDL12_MouseMotionEvent {
	uint8_t type;
	uint8_t which;
	uint8_t state;
	uint16_t x, y;
	int16_t xrel;
	int16_t yrel;
} SDL12_MouseMotionEvent;

/** Mouse button event */
typedef struct SDL12_MouseButtonEvent {
	uint8_t type;
	uint8_t which;
	uint8_t button;
	uint8_t state;
	uint16_t x, y;
} SDL12_MouseButtonEvent;

/** General event structure */
typedef union SDL12_Event {
	uint8_t type;
	SDL12_ActiveEvent active;
	SDL12_KeyboardEvent key;
	SDL12_MouseMotionEvent motion;
	SDL12_MouseButtonEvent button;
	/*
	SDL_JoyAxisEvent jaxis;
	SDL_JoyBallEvent jball;
	SDL_JoyHatEvent jhat;
	SDL_JoyButtonEvent jbutton;
	SDL_ResizeEvent resize;
	SDL_ExposeEvent expose;
	SDL_QuitEvent quit;
	SDL_UserEvent user;
	SDL_SysWMEvent syswm;
	*/
} SDL12_Event;

#define	SDL12_ALLEVENTS	0xFFFFFFFF

typedef int (*SDL12_EventFilter)(const SDL12_Event *event);

#endif
