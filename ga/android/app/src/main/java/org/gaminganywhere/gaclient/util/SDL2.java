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

package org.gaminganywhere.gaclient.util;

public class SDL2 {

	class Button {
		public static final int LEFT = 1;
		public static final int MIDDLE = 2;
		public static final int RIGHT = 3;
		public static final int X1 = 4;
		public static final int X2 = 5;
		//
		public static final int LMASK = 0x01;	// 1 << (button)-1
		public static final int MMASK = 0x02;
		public static final int RMASK = 0x04;
		public static final int X1MASK = 0x08;
		public static final int X2MASK = 0x10;
	}
	class Scancode {
		public static final int A = 4;
		public static final int B = 5;
		public static final int C = 6;
		public static final int D = 7;
		public static final int E = 8;
		public static final int F = 9;
		public static final int G = 10;
		public static final int H = 11;
		public static final int I = 12;
		public static final int J = 13;
		public static final int K = 14;
		public static final int L = 15;
		public static final int M = 16;
		public static final int N = 17;
		public static final int O = 18;
		public static final int P = 19;
		public static final int Q = 20;
		public static final int R = 21;
		public static final int S = 22;
		public static final int T = 23;
		public static final int U = 24;
		public static final int V = 25;
		public static final int W = 26;
		public static final int X = 27;
		public static final int Y = 28;
		public static final int Z = 29;

		public static final int _1 = 30;
		public static final int _2 = 31;
		public static final int _3 = 32;
		public static final int _4 = 33;
		public static final int _5 = 34;
		public static final int _6 = 35;
		public static final int _7 = 36;
		public static final int _8 = 37;
		public static final int _9 = 38;
		public static final int _0 = 39;

		public static final int RETURN = 40;
		public static final int ESCAPE = 41;
		public static final int BACKSPACE = 42;
		public static final int TAB = 43;
		public static final int SPACE = 44;

		public static final int MINUS = 45;
		public static final int EQUALS = 46;
		public static final int LEFTBRACKET = 47;
		public static final int RIGHTBRACKET = 48;
		public static final int BACKSLASH = 49;
		/**
		 * < Located at the lower left of the return key on ISO keyboards and at
		 * the right end of the QWERTY row on ANSI keyboards. Produces REVERSE
		 * SOLIDUS (backslash) and VERTICAL LINE in a US layout, REVERSE SOLIDUS
		 * and VERTICAL LINE in a UK Mac layout, NUMBER SIGN and TILDE in a UK
		 * Windows layout, DOLLAR SIGN and POUND SIGN in a Swiss German layout,
		 * NUMBER SIGN and APOSTROPHE in a German layout, GRAVE ACCENT and POUND
		 * SIGN in a French Mac layout, and ASTERISK and MICRO SIGN in a French
		 * Windows layout.
		 */
		public static final int NONUSHASH = 50;
		/**
		 * < ISO USB keyboards actually use this code instead of 49 for the same
		 * key, but all OSes I've seen treat the two codes identically. So, as
		 * an implementor, unless your keyboard generates both of those codes
		 * and your OS treats them differently; you should generate
		 * SDL_SCANCODE_BACKSLASH instead of this code. As a user, you should
		 * not rely on this code because SDL will never generate it with most
		 * (all?) keyboards.
		 */
		public static final int SEMICOLON = 51;
		public static final int APOSTROPHE = 52;
		public static final int GRAVE = 53;
		/**
		 * < Located in the top left corner (on both ANSI and ISO keyboards).
		 * Produces GRAVE ACCENT and TILDE in a US Windows layout and in US and
		 * UK Mac layouts on ANSI keyboards, GRAVE ACCENT and NOT SIGN in a UK
		 * Windows layout, SECTION SIGN and PLUS-MINUS SIGN in US and UK Mac
		 * layouts on ISO keyboards, SECTION SIGN and DEGREE SIGN in a Swiss
		 * German layout (Mac: only on ISO keyboards), CIRCUMFLEX ACCENT and
		 * DEGREE SIGN in a German layout (Mac: only on ISO keyboards),
		 * SUPERSCRIPT TWO and TILDE in a French Windows layout, COMMERCIAL AT
		 * and NUMBER SIGN in a French Mac layout on ISO keyboards, and
		 * LESS-THAN SIGN and GREATER-THAN SIGN in a Swiss German, German, or
		 * French Mac layout on ANSI keyboards.
		 */
		public static final int COMMA = 54;
		public static final int PERIOD = 55;
		public static final int SLASH = 56;

		public static final int CAPSLOCK = 57;

		public static final int F1 = 58;
		public static final int F2 = 59;
		public static final int F3 = 60;
		public static final int F4 = 61;
		public static final int F5 = 62;
		public static final int F6 = 63;
		public static final int F7 = 64;
		public static final int F8 = 65;
		public static final int F9 = 66;
		public static final int F10 = 67;
		public static final int F11 = 68;
		public static final int F12 = 69;

		public static final int PRINTSCREEN = 70;
		public static final int SCROLLLOCK = 71;
		public static final int PAUSE = 72;
		public static final int INSERT = 73;
		/**
		 * < insert on PC, help on some Mac keyboards (but does send code 73,
		 * not 117)
		 */
		public static final int HOME = 74;
		public static final int PAGEUP = 75;
		public static final int DELETE = 76;
		public static final int END = 77;
		public static final int PAGEDOWN = 78;
		public static final int RIGHT = 79;
		public static final int LEFT = 80;
		public static final int DOWN = 81;
		public static final int UP = 82;

		public static final int NUMLOCKCLEAR = 83;
		/**
		 * < num lock on PC, clear on Mac keyboards
		 */
		public static final int KP_DIVIDE = 84;
		public static final int KP_MULTIPLY = 85;
		public static final int KP_MINUS = 86;
		public static final int KP_PLUS = 87;
		public static final int KP_ENTER = 88;
		public static final int KP_1 = 89;
		public static final int KP_2 = 90;
		public static final int KP_3 = 91;
		public static final int KP_4 = 92;
		public static final int KP_5 = 93;
		public static final int KP_6 = 94;
		public static final int KP_7 = 95;
		public static final int KP_8 = 96;
		public static final int KP_9 = 97;
		public static final int KP_0 = 98;
		public static final int KP_PERIOD = 99;

		public static final int NONUSBACKSLASH = 100;
		/**
		 * < This is the additional key that ISO keyboards have over ANSI ones;
		 * located between left shift and Y. Produces GRAVE ACCENT and TILDE in
		 * a US or UK Mac layout, REVERSE SOLIDUS (backslash) and VERTICAL LINE
		 * in a US or UK Windows layout, and LESS-THAN SIGN and GREATER-THAN
		 * SIGN in a Swiss German, German, or French layout.
		 */
		public static final int APPLICATION = 101;
		/** < windows contextual menu, compose */
		public static final int POWER = 102;
		/**
		 * < The USB document says this is a status flag; not a physical key -
		 * but some Mac keyboards do have a power key.
		 */
		public static final int KP_EQUALS = 103;
		public static final int F13 = 104;
		public static final int F14 = 105;
		public static final int F15 = 106;
		public static final int F16 = 107;
		public static final int F17 = 108;
		public static final int F18 = 109;
		public static final int F19 = 110;
		public static final int F20 = 111;
		public static final int F21 = 112;
		public static final int F22 = 113;
		public static final int F23 = 114;
		public static final int F24 = 115;
		public static final int EXECUTE = 116;
		public static final int HELP = 117;
		public static final int MENU = 118;
		public static final int SELECT = 119;
		public static final int STOP = 120;
		public static final int AGAIN = 121;
		/** < redo */
		public static final int UNDO = 122;
		public static final int CUT = 123;
		public static final int COPY = 124;
		public static final int PASTE = 125;
		public static final int FIND = 126;
		public static final int MUTE = 127;
		public static final int VOLUMEUP = 128;
		public static final int VOLUMEDOWN = 129;
		/* not sure whether there's a reason to enable these */
		/* LOCKINGCAPSLOCK = 130, */
		/* LOCKINGNUMLOCK = 131, */
		/* LOCKINGSCROLLLOCK = 132, */
		public static final int KP_COMMA = 133;
		public static final int KP_EQUALSAS400 = 134;

		public static final int INTERNATIONAL1 = 135;
		/**
		 * < used on Asian keyboards, see footnotes in USB doc
		 */
		public static final int INTERNATIONAL2 = 136;
		public static final int INTERNATIONAL3 = 137;
		/** < Yen */
		public static final int INTERNATIONAL4 = 138;
		public static final int INTERNATIONAL5 = 139;
		public static final int INTERNATIONAL6 = 140;
		public static final int INTERNATIONAL7 = 141;
		public static final int INTERNATIONAL8 = 142;
		public static final int INTERNATIONAL9 = 143;
		public static final int LANG1 = 144;
		/** < Hangul/English toggle */
		public static final int LANG2 = 145;
		/** < Hanja conversion */
		public static final int LANG3 = 146;
		/** < Katakana */
		public static final int LANG4 = 147;
		/** < Hiragana */
		public static final int LANG5 = 148;
		/** < Zenkaku/Hankaku */
		public static final int LANG6 = 149;
		/** < reserved */
		public static final int LANG7 = 150;
		/** < reserved */
		public static final int LANG8 = 151;
		/** < reserved */
		public static final int LANG9 = 152;
		/** < reserved */

		public static final int ALTERASE = 153;
		/** < Erase-Eaze */
		public static final int SYSREQ = 154;
		public static final int CANCEL = 155;
		public static final int CLEAR = 156;
		public static final int PRIOR = 157;
		public static final int RETURN2 = 158;
		public static final int SEPARATOR = 159;
		public static final int OUT = 160;
		public static final int OPER = 161;
		public static final int CLEARAGAIN = 162;
		public static final int CRSEL = 163;
		public static final int EXSEL = 164;

		public static final int KP_00 = 176;
		public static final int KP_000 = 177;
		public static final int THOUSANDSSEPARATOR = 178;
		public static final int DECIMALSEPARATOR = 179;
		public static final int CURRENCYUNIT = 180;
		public static final int CURRENCYSUBUNIT = 181;
		public static final int KP_LEFTPAREN = 182;
		public static final int KP_RIGHTPAREN = 183;
		public static final int KP_LEFTBRACE = 184;
		public static final int KP_RIGHTBRACE = 185;
		public static final int KP_TAB = 186;
		public static final int KP_BACKSPACE = 187;
		public static final int KP_A = 188;
		public static final int KP_B = 189;
		public static final int KP_C = 190;
		public static final int KP_D = 191;
		public static final int KP_E = 192;
		public static final int KP_F = 193;
		public static final int KP_XOR = 194;
		public static final int KP_POWER = 195;
		public static final int KP_PERCENT = 196;
		public static final int KP_LESS = 197;
		public static final int KP_GREATER = 198;
		public static final int KP_AMPERSAND = 199;
		public static final int KP_DBLAMPERSAND = 200;
		public static final int KP_VERTICALBAR = 201;
		public static final int KP_DBLVERTICALBAR = 202;
		public static final int KP_COLON = 203;
		public static final int KP_HASH = 204;
		public static final int KP_SPACE = 205;
		public static final int KP_AT = 206;
		public static final int KP_EXCLAM = 207;
		public static final int KP_MEMSTORE = 208;
		public static final int KP_MEMRECALL = 209;
		public static final int KP_MEMCLEAR = 210;
		public static final int KP_MEMADD = 211;
		public static final int KP_MEMSUBTRACT = 212;
		public static final int KP_MEMMULTIPLY = 213;
		public static final int KP_MEMDIVIDE = 214;
		public static final int KP_PLUSMINUS = 215;
		public static final int KP_CLEAR = 216;
		public static final int KP_CLEARENTRY = 217;
		public static final int KP_BINARY = 218;
		public static final int KP_OCTAL = 219;
		public static final int KP_DECIMAL = 220;
		public static final int KP_HEXADECIMAL = 221;

		public static final int LCTRL = 224;
		public static final int LSHIFT = 225;
		public static final int LALT = 226;
		/** < alt, option */
		public static final int LGUI = 227;
		/** < windows, command (apple), meta */
		public static final int RCTRL = 228;
		public static final int RSHIFT = 229;
		public static final int RALT = 230;
		/** < alt gr, option */
		public static final int RGUI = 231;
		/** < windows, command (apple), meta */

		public static final int MODE = 257;
		/**
		 * < I'm not sure if this is really not covered by any of the above, but
		 * since there's a special KMOD_MODE for it I'm adding it here
		 */

		/**
		 * \name Usage page 0x0C
		 * 
		 * These values are mapped from usage page 0x0C (USB consumer page).
		 */

		public static final int AUDIONEXT = 258;
		public static final int AUDIOPREV = 259;
		public static final int AUDIOSTOP = 260;
		public static final int AUDIOPLAY = 261;
		public static final int AUDIOMUTE = 262;
		public static final int MEDIASELECT = 263;
		public static final int WWW = 264;
		public static final int MAIL = 265;
		public static final int CALCULATOR = 266;
		public static final int COMPUTER = 267;
		public static final int AC_SEARCH = 268;
		public static final int AC_HOME = 269;
		public static final int AC_BACK = 270;
		public static final int AC_FORWARD = 271;
		public static final int AC_STOP = 272;
		public static final int AC_REFRESH = 273;
		public static final int AC_BOOKMARKS = 274;

		/**
		 * \name Walther keys
		 * 
		 * These are values that Christian Walther added (for mac keyboard?).
		 */

		public static final int BRIGHTNESSDOWN = 275;
		public static final int BRIGHTNESSUP = 276;
		public static final int DISPLAYSWITCH = 277;
		/**
		 * < display mirroring/dual display switch, video mode switch
		 */
		public static final int KBDILLUMTOGGLE = 278;
		public static final int KBDILLUMDOWN = 279;
		public static final int KBDILLUMUP = 280;
		public static final int EJECT = 281;
		public static final int SLEEP = 282;

		public static final int APP1 = 283;
		public static final int APP2 = 284;
	}
	class Keycode {
		public static final int SCAN2KEY_MASK = 1<<30;
		//
		public static final int RETURN = '\r';
		public static final int ESCAPE = '\033';
		public static final int BACKSPACE = '\b';
		public static final int TAB = '\t';
		public static final int SPACE = ' ';
		public static final int EXCLAIM = '!';
		public static final int QUOTEDBL = '"';
		public static final int HASH = '#';
		public static final int PERCENT = '%';
		public static final int DOLLAR = '$';
		public static final int AMPERSAND = '&';
		public static final int QUOTE = '\'';
		public static final int LEFTPAREN = '(';
		public static final int RIGHTPAREN = ')';
		public static final int ASTERISK = '*';
		public static final int PLUS = '+';
		public static final int COMMA = ',';
		public static final int MINUS = '-';
		public static final int PERIOD = '.';
		public static final int SLASH = '/';
		public static final int _0 = '0';
		public static final int _1 = '1';
		public static final int _2 = '2';
		public static final int _3 = '3';
		public static final int _4 = '4';
		public static final int _5 = '5';
		public static final int _6 = '6';
		public static final int _7 = '7';
		public static final int _8 = '8';
		public static final int _9 = '9';
		public static final int COLON = ':';
		public static final int SEMICOLON = ';';
		public static final int LESS = '<';
		public static final int EQUALS = '=';
		public static final int GREATER = '>';
		public static final int QUESTION = '?';
		public static final int AT = '@';
		/*
		 * Skip uppercase letters
		 */
		public static final int LEFTBRACKET = '[';
		public static final int BACKSLASH = '\\';
		public static final int RIGHTBRACKET = ']';
		public static final int CARET = '^';
		public static final int UNDERSCORE = '_';
		public static final int BACKQUOTE = '`';
		public static final int a = 'a';
		public static final int b = 'b';
		public static final int c = 'c';
		public static final int d = 'd';
		public static final int e = 'e';
		public static final int f = 'f';
		public static final int g = 'g';
		public static final int h = 'h';
		public static final int i = 'i';
		public static final int j = 'j';
		public static final int k = 'k';
		public static final int l = 'l';
		public static final int m = 'm';
		public static final int n = 'n';
		public static final int o = 'o';
		public static final int p = 'p';
		public static final int q = 'q';
		public static final int r = 'r';
		public static final int s = 's';
		public static final int t = 't';
		public static final int u = 'u';
		public static final int v = 'v';
		public static final int w = 'w';
		public static final int x = 'x';
		public static final int y = 'y';
		public static final int z = 'z';

		public static final int CAPSLOCK = Scancode.CAPSLOCK | SCAN2KEY_MASK;

		public static final int F1 = Scancode.F1 | SCAN2KEY_MASK;
		public static final int F2 = Scancode.F2 | SCAN2KEY_MASK;
		public static final int F3 = Scancode.F3 | SCAN2KEY_MASK;
		public static final int F4 = Scancode.F4 | SCAN2KEY_MASK;
		public static final int F5 = Scancode.F5 | SCAN2KEY_MASK;
		public static final int F6 = Scancode.F6 | SCAN2KEY_MASK;
		public static final int F7 = Scancode.F7 | SCAN2KEY_MASK;
		public static final int F8 = Scancode.F8 | SCAN2KEY_MASK;
		public static final int F9 = Scancode.F9 | SCAN2KEY_MASK;
		public static final int F10 = Scancode.F10 | SCAN2KEY_MASK;
		public static final int F11 = Scancode.F11 | SCAN2KEY_MASK;
		public static final int F12 = Scancode.F12 | SCAN2KEY_MASK;

		public static final int PRINTSCREEN = Scancode.PRINTSCREEN | SCAN2KEY_MASK;
		public static final int SCROLLLOCK = Scancode.SCROLLLOCK | SCAN2KEY_MASK;
		public static final int PAUSE = Scancode.PAUSE | SCAN2KEY_MASK;
		public static final int INSERT = Scancode.INSERT | SCAN2KEY_MASK;
		public static final int HOME = Scancode.HOME | SCAN2KEY_MASK;
		public static final int PAGEUP = Scancode.PAGEUP | SCAN2KEY_MASK;
		public static final int DELETE = '\177';
		public static final int END = Scancode.END | SCAN2KEY_MASK;
		public static final int PAGEDOWN = Scancode.PAGEDOWN | SCAN2KEY_MASK;
		public static final int RIGHT = Scancode.RIGHT | SCAN2KEY_MASK;
		public static final int LEFT = Scancode.LEFT | SCAN2KEY_MASK;
		public static final int DOWN = Scancode.DOWN | SCAN2KEY_MASK;
		public static final int UP = Scancode.UP | SCAN2KEY_MASK;

		public static final int NUMLOCKCLEAR = Scancode.NUMLOCKCLEAR | SCAN2KEY_MASK;
		public static final int KP_DIVIDE = Scancode.KP_DIVIDE | SCAN2KEY_MASK;
		public static final int KP_MULTIPLY = Scancode.KP_MULTIPLY | SCAN2KEY_MASK;
		public static final int KP_MINUS = Scancode.KP_MINUS | SCAN2KEY_MASK;
		public static final int KP_PLUS = Scancode.KP_PLUS | SCAN2KEY_MASK;
		public static final int KP_ENTER = Scancode.KP_ENTER | SCAN2KEY_MASK;
		public static final int KP_1 = Scancode.KP_1 | SCAN2KEY_MASK;
		public static final int KP_2 = Scancode.KP_2 | SCAN2KEY_MASK;
		public static final int KP_3 = Scancode.KP_3 | SCAN2KEY_MASK;
		public static final int KP_4 = Scancode.KP_4 | SCAN2KEY_MASK;
		public static final int KP_5 = Scancode.KP_5 | SCAN2KEY_MASK;
		public static final int KP_6 = Scancode.KP_6 | SCAN2KEY_MASK;
		public static final int KP_7 = Scancode.KP_7 | SCAN2KEY_MASK;
		public static final int KP_8 = Scancode.KP_8 | SCAN2KEY_MASK;
		public static final int KP_9 = Scancode.KP_9 | SCAN2KEY_MASK;
		public static final int KP_0 = Scancode.KP_0 | SCAN2KEY_MASK;
		public static final int KP_PERIOD = Scancode.KP_PERIOD | SCAN2KEY_MASK;

		public static final int APPLICATION = Scancode.APPLICATION | SCAN2KEY_MASK;
		public static final int POWER = Scancode.POWER | SCAN2KEY_MASK;
		public static final int KP_EQUALS = Scancode.KP_EQUALS | SCAN2KEY_MASK;
		public static final int F13 = Scancode.F13 | SCAN2KEY_MASK;
		public static final int F14 = Scancode.F14 | SCAN2KEY_MASK;
		public static final int F15 = Scancode.F15 | SCAN2KEY_MASK;
		public static final int F16 = Scancode.F16 | SCAN2KEY_MASK;
		public static final int F17 = Scancode.F17 | SCAN2KEY_MASK;
		public static final int F18 = Scancode.F18 | SCAN2KEY_MASK;
		public static final int F19 = Scancode.F19 | SCAN2KEY_MASK;
		public static final int F20 = Scancode.F20 | SCAN2KEY_MASK;
		public static final int F21 = Scancode.F21 | SCAN2KEY_MASK;
		public static final int F22 = Scancode.F22 | SCAN2KEY_MASK;
		public static final int F23 = Scancode.F23 | SCAN2KEY_MASK;
		public static final int F24 = Scancode.F24 | SCAN2KEY_MASK;
		public static final int EXECUTE = Scancode.EXECUTE | SCAN2KEY_MASK;
		public static final int HELP = Scancode.HELP | SCAN2KEY_MASK;
		public static final int MENU = Scancode.MENU | SCAN2KEY_MASK;
		public static final int SELECT = Scancode.SELECT | SCAN2KEY_MASK;
		public static final int STOP = Scancode.STOP | SCAN2KEY_MASK;
		public static final int AGAIN = Scancode.AGAIN | SCAN2KEY_MASK;
		public static final int UNDO = Scancode.UNDO | SCAN2KEY_MASK;
		public static final int CUT = Scancode.CUT | SCAN2KEY_MASK;
		public static final int COPY = Scancode.COPY | SCAN2KEY_MASK;
		public static final int PASTE = Scancode.PASTE | SCAN2KEY_MASK;
		public static final int FIND = Scancode.FIND | SCAN2KEY_MASK;
		public static final int MUTE = Scancode.MUTE | SCAN2KEY_MASK;
		public static final int VOLUMEUP = Scancode.VOLUMEUP | SCAN2KEY_MASK;
		public static final int VOLUMEDOWN = Scancode.VOLUMEDOWN | SCAN2KEY_MASK;
		public static final int KP_COMMA = Scancode.KP_COMMA | SCAN2KEY_MASK;
		public static final int KP_EQUALSAS400 = Scancode.KP_EQUALSAS400 | SCAN2KEY_MASK;

		public static final int ALTERASE = Scancode.ALTERASE | SCAN2KEY_MASK;
		public static final int SYSREQ = Scancode.SYSREQ | SCAN2KEY_MASK;
		public static final int CANCEL = Scancode.CANCEL | SCAN2KEY_MASK;
		public static final int CLEAR = Scancode.CLEAR | SCAN2KEY_MASK;
		public static final int PRIOR = Scancode.PRIOR | SCAN2KEY_MASK;
		public static final int RETURN2 = Scancode.RETURN2 | SCAN2KEY_MASK;
		public static final int SEPARATOR = Scancode.SEPARATOR | SCAN2KEY_MASK;
		public static final int OUT = Scancode.OUT | SCAN2KEY_MASK;
		public static final int OPER = Scancode.OPER | SCAN2KEY_MASK;
		public static final int CLEARAGAIN = Scancode.CLEARAGAIN | SCAN2KEY_MASK;
		public static final int CRSEL = Scancode.CRSEL | SCAN2KEY_MASK;
		public static final int EXSEL = Scancode.EXSEL | SCAN2KEY_MASK;

		public static final int KP_00 = Scancode.KP_00 | SCAN2KEY_MASK;
		public static final int KP_000 = Scancode.KP_000 | SCAN2KEY_MASK;
		public static final int THOUSANDSSEPARATOR = Scancode.THOUSANDSSEPARATOR | SCAN2KEY_MASK;
		public static final int DECIMALSEPARATOR = Scancode.DECIMALSEPARATOR | SCAN2KEY_MASK;
		public static final int CURRENCYUNIT = Scancode.CURRENCYUNIT | SCAN2KEY_MASK;
		public static final int CURRENCYSUBUNIT = Scancode.CURRENCYSUBUNIT | SCAN2KEY_MASK;
		public static final int KP_LEFTPAREN = Scancode.KP_LEFTPAREN | SCAN2KEY_MASK;
		public static final int KP_RIGHTPAREN = Scancode.KP_RIGHTPAREN | SCAN2KEY_MASK;
		public static final int KP_LEFTBRACE = Scancode.KP_LEFTBRACE | SCAN2KEY_MASK;
		public static final int KP_RIGHTBRACE = Scancode.KP_RIGHTBRACE | SCAN2KEY_MASK;
		public static final int KP_TAB = Scancode.KP_TAB | SCAN2KEY_MASK;
		public static final int KP_BACKSPACE = Scancode.KP_BACKSPACE | SCAN2KEY_MASK;
		public static final int KP_A = Scancode.KP_A | SCAN2KEY_MASK;
		public static final int KP_B = Scancode.KP_B | SCAN2KEY_MASK;
		public static final int KP_C = Scancode.KP_C | SCAN2KEY_MASK;
		public static final int KP_D = Scancode.KP_D | SCAN2KEY_MASK;
		public static final int KP_E = Scancode.KP_E | SCAN2KEY_MASK;
		public static final int KP_F = Scancode.KP_F | SCAN2KEY_MASK;
		public static final int KP_XOR = Scancode.KP_XOR | SCAN2KEY_MASK;
		public static final int KP_POWER = Scancode.KP_POWER | SCAN2KEY_MASK;
		public static final int KP_PERCENT = Scancode.KP_PERCENT
				| SCAN2KEY_MASK;
		public static final int KP_LESS = Scancode.KP_LESS | SCAN2KEY_MASK;
		public static final int KP_GREATER = Scancode.KP_GREATER
				| SCAN2KEY_MASK;
		public static final int KP_AMPERSAND = Scancode.KP_AMPERSAND
				| SCAN2KEY_MASK;
		public static final int KP_DBLAMPERSAND = Scancode.KP_DBLAMPERSAND
				| SCAN2KEY_MASK;
		public static final int KP_VERTICALBAR = Scancode.KP_VERTICALBAR
				| SCAN2KEY_MASK;
		public static final int KP_DBLVERTICALBAR = Scancode.KP_DBLVERTICALBAR
				| SCAN2KEY_MASK;
		public static final int KP_COLON = Scancode.KP_COLON | SCAN2KEY_MASK;
		public static final int KP_HASH = Scancode.KP_HASH | SCAN2KEY_MASK;
		public static final int KP_SPACE = Scancode.KP_SPACE | SCAN2KEY_MASK;
		public static final int KP_AT = Scancode.KP_AT | SCAN2KEY_MASK;
		public static final int KP_EXCLAM = Scancode.KP_EXCLAM | SCAN2KEY_MASK;
		public static final int KP_MEMSTORE = Scancode.KP_MEMSTORE
				| SCAN2KEY_MASK;
		public static final int KP_MEMRECALL = Scancode.KP_MEMRECALL
				| SCAN2KEY_MASK;
		public static final int KP_MEMCLEAR = Scancode.KP_MEMCLEAR
				| SCAN2KEY_MASK;
		public static final int KP_MEMADD = Scancode.KP_MEMADD | SCAN2KEY_MASK;
		public static final int KP_MEMSUBTRACT = Scancode.KP_MEMSUBTRACT
				| SCAN2KEY_MASK;
		public static final int KP_MEMMULTIPLY = Scancode.KP_MEMMULTIPLY
				| SCAN2KEY_MASK;
		public static final int KP_MEMDIVIDE = Scancode.KP_MEMDIVIDE
				| SCAN2KEY_MASK;
		public static final int KP_PLUSMINUS = Scancode.KP_PLUSMINUS
				| SCAN2KEY_MASK;
		public static final int KP_CLEAR = Scancode.KP_CLEAR | SCAN2KEY_MASK;
		public static final int KP_CLEARENTRY = Scancode.KP_CLEARENTRY
				| SCAN2KEY_MASK;
		public static final int KP_BINARY = Scancode.KP_BINARY | SCAN2KEY_MASK;
		public static final int KP_OCTAL = Scancode.KP_OCTAL | SCAN2KEY_MASK;
		public static final int KP_DECIMAL = Scancode.KP_DECIMAL
				| SCAN2KEY_MASK;
		public static final int KP_HEXADECIMAL = Scancode.KP_HEXADECIMAL
				| SCAN2KEY_MASK;

		public static final int LCTRL = Scancode.LCTRL | SCAN2KEY_MASK;
		public static final int LSHIFT = Scancode.LSHIFT | SCAN2KEY_MASK;
		public static final int LALT = Scancode.LALT | SCAN2KEY_MASK;
		public static final int LGUI = Scancode.LGUI | SCAN2KEY_MASK;
		public static final int RCTRL = Scancode.RCTRL | SCAN2KEY_MASK;
		public static final int RSHIFT = Scancode.RSHIFT | SCAN2KEY_MASK;
		public static final int RALT = Scancode.RALT | SCAN2KEY_MASK;
		public static final int RGUI = Scancode.RGUI | SCAN2KEY_MASK;

		public static final int MODE = Scancode.MODE | SCAN2KEY_MASK;

		public static final int AUDIONEXT = Scancode.AUDIONEXT | SCAN2KEY_MASK;
		public static final int AUDIOPREV = Scancode.AUDIOPREV | SCAN2KEY_MASK;
		public static final int AUDIOSTOP = Scancode.AUDIOSTOP | SCAN2KEY_MASK;
		public static final int AUDIOPLAY = Scancode.AUDIOPLAY | SCAN2KEY_MASK;
		public static final int AUDIOMUTE = Scancode.AUDIOMUTE | SCAN2KEY_MASK;
		public static final int MEDIASELECT = Scancode.MEDIASELECT
				| SCAN2KEY_MASK;
		public static final int WWW = Scancode.WWW | SCAN2KEY_MASK;
		public static final int MAIL = Scancode.MAIL | SCAN2KEY_MASK;
		public static final int CALCULATOR = Scancode.CALCULATOR
				| SCAN2KEY_MASK;
		public static final int COMPUTER = Scancode.COMPUTER | SCAN2KEY_MASK;
		public static final int AC_SEARCH = Scancode.AC_SEARCH | SCAN2KEY_MASK;
		public static final int AC_HOME = Scancode.AC_HOME | SCAN2KEY_MASK;
		public static final int AC_BACK = Scancode.AC_BACK | SCAN2KEY_MASK;
		public static final int AC_FORWARD = Scancode.AC_FORWARD
				| SCAN2KEY_MASK;
		public static final int AC_STOP = Scancode.AC_STOP | SCAN2KEY_MASK;
		public static final int AC_REFRESH = Scancode.AC_REFRESH
				| SCAN2KEY_MASK;
		public static final int AC_BOOKMARKS = Scancode.AC_BOOKMARKS
				| SCAN2KEY_MASK;

		public static final int BRIGHTNESSDOWN = Scancode.BRIGHTNESSDOWN
				| SCAN2KEY_MASK;
		public static final int BRIGHTNESSUP = Scancode.BRIGHTNESSUP
				| SCAN2KEY_MASK;
		public static final int DISPLAYSWITCH = Scancode.DISPLAYSWITCH
				| SCAN2KEY_MASK;
		public static final int KBDILLUMTOGGLE = Scancode.KBDILLUMTOGGLE
				| SCAN2KEY_MASK;
		public static final int KBDILLUMDOWN = Scancode.KBDILLUMDOWN
				| SCAN2KEY_MASK;
		public static final int KBDILLUMUP = Scancode.KBDILLUMUP
				| SCAN2KEY_MASK;
		public static final int EJECT = Scancode.EJECT | SCAN2KEY_MASK;
		public static final int SLEEP = Scancode.SLEEP | SCAN2KEY_MASK;
	}
	class Keymod {
		public static final int NONE = 0x0000;
		public static final int LSHIFT = 0x0001;
		public static final int RSHIFT = 0x0002;
		public static final int LCTRL = 0x0040;
		public static final int RCTRL = 0x0080;
		public static final int LALT = 0x0100;
		public static final int RALT = 0x0200;
		public static final int LGUI = 0x0400;
		public static final int RGUI = 0x0800;
		public static final int NUM = 0x1000;
		public static final int CAPS = 0x2000;
		public static final int MODE = 0x4000;
		public static final int RESERVED = 0x8000;
		public static final int CTRL = LCTRL | RCTRL;
		public static final int SHIFT = LSHIFT | RSHIFT;
		public static final int ALT = LALT | RALT;
	}
	
}
