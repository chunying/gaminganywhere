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

/**
 * @file
 * GamingAnywhere configuration file loader: header files
 */

#ifndef __GA_CONF_H__
#define __GA_CONF_H__

#include "ga-common.h"

// laod config from file
EXPORT int ga_conf_load(const char *filename);
EXPORT int ga_url_parse(const char *url);
EXPORT void ga_conf_clear();

// operations with key/value pair
EXPORT char * ga_conf_readv(const char *key, char *store, int slen);
EXPORT int ga_conf_readint(const char *key);
EXPORT double ga_conf_readdouble(const char *key);
EXPORT int ga_conf_readbool(const char *key, int defval);
EXPORT int ga_conf_boolval(const char *ptr, int defval);
EXPORT int ga_conf_readints(const char *key, int *vals, int n);
EXPORT int ga_conf_writev(const char *key, const char *value);
EXPORT void ga_conf_erase(const char *key);

// operations with map
EXPORT int ga_conf_ismap(const char *key);
EXPORT int ga_conf_haskey(const char *mapname, const char *key);
EXPORT int ga_conf_mapsize(const char *mapname);
EXPORT char * ga_conf_mapreadv(const char *mapname, const char *key, char *store, int slen);
EXPORT int ga_conf_mapreadint(const char *mapname, const char *key);
EXPORT int ga_conf_mapreadints(const char *mapname, const char *key, int *vals, int n);
EXPORT double ga_conf_mapreaddouble(const char *mapname, const char *key);
EXPORT int ga_conf_mapreadbool(const char *mapname, const char *key, int defval);
EXPORT int ga_conf_mapwritev(const char *mapname, const char *key, const char *value);
EXPORT void ga_conf_maperase(const char *mapname, const char *key);
//
EXPORT void ga_conf_mapreset(const char *mapname);
EXPORT char *ga_conf_mapkey(const char *mapname, char *keystore, int klen);
EXPORT char *ga_conf_mapvalue(const char *mapname, char *valstore, int vlen);
EXPORT char *ga_conf_mapnextkey(const char *mapname, char *keystore, int klen);

// operations with whole map
EXPORT void ga_conf_reset();
EXPORT const char *ga_conf_key();
EXPORT const char *ga_conf_nextkey();

#endif /* __GA_CONF_H__ */
