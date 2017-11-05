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

/**
 * @file
 * ctrl-msg: generic message and system message handler
 */

#ifndef __CTRL_MSG_H__
#define	__CTRL_MSG_H__

#include "ga-common.h"

#define	CTRL_MSGTYPE_NULL	0xff	/* system control message starting from 0xff - reserved */
#define	CTRL_MSGTYPE_SYSTEM	0xfe	/* system control message type */

#define	CTRL_MSGSYS_SUBTYPE_NULL	0	/* system control message: NULL */
#define	CTRL_MSGSYS_SUBTYPE_SHUTDOWN	1	/* system control message: shutdown */
#define	CTRL_MSGSYS_SUBTYPE_NETREPORT	2	/* system control message: report networking */
#define	CTRL_MSGSYS_SUBTYPE_MAX		2	/* must equal to the last sub message type */

#if defined(WIN32) && !defined(MSYS)
#define	BEGIN_CTRL_MESSAGE_STRUCT	__pragma(pack(push, 1))	/* equal to #pragma pack(push, 1) */
#define END_CTRL_MESSAGE_STRUCT		; \
					__pragma(pack(pop))	/* equal to #pragma pack(pop) */ 
#else
#define	BEGIN_CTRL_MESSAGE_STRUCT
#define END_CTRL_MESSAGE_STRUCT		__attribute__((__packed__));
#endif

BEGIN_CTRL_MESSAGE_STRUCT
/**
 * Generic (minimal) control message. This is compatible with that sdlmsg_t.
 */
struct ctrlmsg_s {
	unsigned short msgsize;		/*< size of this message, including msgsize */
	unsigned char msgtype;		/*< message type */
	unsigned char which;		/*< unused */
	unsigned char padding[124];	/*< a sufficient large buffer to fit all types of message */
}
END_CTRL_MESSAGE_STRUCT
typedef struct ctrlmsg_s ctrlmsg_t;

////////////////////////////////////////////////////////////////////////////

BEGIN_CTRL_MESSAGE_STRUCT
/**
 * General system control message structure.
 */
struct ctrlmsg_system_s {
	unsigned short msgsize;		/*< size of this message, including msgsize */
	unsigned char msgtype;		/*< message type */
	unsigned char subtype;		/*< system command message subtype */
}
END_CTRL_MESSAGE_STRUCT
typedef struct ctrlmsg_system_s ctrlmsg_system_t;

////////////////////////////////////////////////////////////////////////////

BEGIN_CTRL_MESSAGE_STRUCT
struct ctrlmsg_system_netreport_s {
	unsigned short msgsize;		/*< size of this message, including this field */
	unsigned char msgtype;		/*< must be CTRL_MSGTYPE_SYSTEM */
	unsigned char subtype;		/*< must be CTRL_MSGSYS_SUBTYPE_NETREPORT */
	unsigned int duration;		/*< sample collection duration (in microseconds) */
	unsigned int framecount;	/*< number of frames */
	unsigned int pktcount;		/*< packet count (including lost packets) */
	unsigned int pktloss;		/*< packet loss count */
	unsigned int bytecount;		/*< total received amunt of data (in bytes) */
	unsigned int capacity;		/*< measured capacity (in bits per second) */
}
END_CTRL_MESSAGE_STRUCT
typedef struct ctrlmsg_system_netreport_s ctrlmsg_system_netreport_t;

////////////////////////////////////////////////////////////////////////////

typedef void (*ctrlsys_handler_t)(ctrlmsg_system_t *);

EXPORT int ctrlsys_handle_message(unsigned char *buf, unsigned int size);
EXPORT	ctrlsys_handler_t ctrlsys_set_handler(unsigned char subtype, ctrlsys_handler_t handler);

// functions for building message data structure
EXPORT ctrlmsg_t * ctrlsys_netreport(ctrlmsg_t *msg, unsigned int duration, unsigned int framecount, unsigned int pktcount, unsigned int pktloss, unsigned int bytecount, unsigned int capacity);

#endif	/* __CTRL_MSG_H__ */
