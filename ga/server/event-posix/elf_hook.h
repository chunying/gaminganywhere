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
 *	https://github.com/shoumikhin/ELF-Hook
 * The author is shoumikhin. For the details, please read
 *	http://www.codeproject.com/KB/library/elf-redirect.aspx
 */
#ifndef __ELF_HOOK_H__
#define __ELF_HOOK_H__

#define	LIBRARY_ADDRESS_BY_HANDLE(dlhandle)	\
		((NULL == dlhandle) ? NULL : (void*) *(size_t const*)(dlhandle)) 

#ifdef __cplusplus
extern "C"
{
#endif

int get_module_base_address(char const *module_filename, void *handle, void **base);
void *elf_hook(char const *library_filename, void const *library_address, char const *function_name, void const *substitution_address);

#ifdef __cplusplus
}
#endif
#endif	/* __ELF_HOOK_H__ */
