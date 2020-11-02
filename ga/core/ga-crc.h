/**
 * @file
 * CRC function headers
 */

#ifndef __GA_CRC_H__
#define __GA_CRC_H__

#include "ga-common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char crc5_t;

/// for CRC5 (CRC5-USB and CRC5-CCITT)
/**
 * Initialization function for CRC5 functions */
static inline crc5_t crc5_init(void)		{ return 0x1f << 3; }
/**
 * Finalize function for CRC5 functions */
static inline crc5_t crc5_finalize(crc5_t crc)	{ return (crc>>3) ^ 0x1f; }
EXPORT crc5_t crc5_reflect(crc5_t data, int data_len);
EXPORT crc5_t crc5_update(crc5_t crc, const unsigned char *data, int data_len, const crc5_t *table);
EXPORT crc5_t crc5_update_usb(crc5_t crc, const unsigned char *data, int data_len);
EXPORT crc5_t crc5_update_ccitt(crc5_t crc, const unsigned char *data, int data_len);

#ifdef __cplusplus
}
#endif

#endif
