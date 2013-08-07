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

/*
 * This file was retrieved from:
 *	https://github.com/shoumikhin/Mach-O-Hook
 * The author is shoumikhin. For the details, please read
 *	http://www.codeproject.com/KB/recipes/dynamic_linking-in-mach-o.aspx
 *	http://www.codeproject.com/KB/recipes/redirection-in-mach-o.aspx
 */
#ifndef __MACH_HOOK_H__
#define __MACH_HOOK_H__

#include <stdint.h>

typedef uint64_t mach_substitution;  //64-bit value to store relocation data, can be just a function address

#ifdef __cplusplus
extern "C"
{
#endif
/*
 * mach_hook_init(library_filename, library_address)
 * Prepares library for hooking by reading and parsing all necessary info from a target Mach-O file.
 * Returns an opaque handle to refer this library for future redirection.
 * library_filename: a path to the library in filesystem
 * library_address: an address of the loaded library image in memory
 */
void *mach_hook_init(char const *library_filename, void const *library_address);

/*
 * mach_hook(handle, function_name, substitution)
 * Redirects a particular function call in a particular library to another address.
 * Returns some opaque data, which can be used for future restoration with this function.
 * handle: an opaque handle of the library returned by mach_hook_init()
 * function_name: a name of the function being hooked
 * substitution: 64-bit data, which can contatin a function's address (for the first time) or a return value of previous redirection
 */
mach_substitution mach_hook(void const *handle, char const *function_name, mach_substitution substitution);

/*
 * mach_hook_free(handle)
 * Releases a library handle, returned by mach_hook_init()
 */
void mach_hook_free(void *handle);

#ifdef __cplusplus
}
#endif

#endif	/* __MACH_HOOK_H__ */
