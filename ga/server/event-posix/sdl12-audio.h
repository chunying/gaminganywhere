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

#ifndef __SDL12_AUDIO_H__
#define __SDL12_AUDIO_H__

struct SDL12_AudioSpec {
	int freq;               /**< DSP frequency -- samples per second */
	uint16_t format;          /**< Audio data format */
	uint8_t channels;        /**< Number of channels: 1 mono, 2 stereo */
	uint8_t silence;         /**< Audio buffer silence value (calculated) */
	uint16_t samples;         /**< Audio buffer size in samples (power of 2) */
	uint16_t padding;         /**< Necessary for some compile environments */
	uint32_t size;            /**< Audio buffer size in bytes (calculated) */
	/**
	 *  This function is called when the audio device needs more data.
	 *
	 *  @param[out] stream  A pointer to the audio data buffer
	 *  @param[in]  len     The length of the audio buffer in bytes.
	 *
	 *  Once the callback returns, the buffer will no longer be valid.
	 *  Stereo samples are stored in a LRLRLR ordering.
	 */
	void (*callback)(void *userdata, uint8_t *stream, int len);
	void  *userdata;
};

#define SDL12_AUDIO_U8		0x0008	/**< Unsigned 8-bit samples */
#define SDL12_AUDIO_S8		0x8008	/**< Signed 8-bit samples */
#define SDL12_AUDIO_U16LSB	0x0010	/**< Unsigned 16-bit samples */
#define SDL12_AUDIO_S16LSB	0x8010	/**< Signed 16-bit samples */
#define SDL12_AUDIO_U16MSB	0x1010	/**< As above, but big-endian byte order */
#define SDL12_AUDIO_S16MSB	0x9010	/**< As above, but big-endian byte order */
#define SDL12_AUDIO_U16		SDL12_AUDIO_U16LSB
#define SDL12_AUDIO_S16		SDL12_AUDIO_S16LSB

#endif
