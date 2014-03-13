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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <map>
#include <vector>
#include <string>
#ifdef WIN32
#else
#include <libgen.h>
#endif

#include "ga-common.h"
#include "ga-conf.h"
#include "ga-confvar.h"

using namespace std;

//

static map<string,gaConfVar> ga_vars;
static map<string,gaConfVar>::iterator ga_vmi = ga_vars.begin();

static char *
ga_conf_trim(char *buf) {
	char *ptr;
	// remove head spaces
	while(*buf && isspace(*buf))
		buf++;
	// remove section
	if(buf[0] == '[') {
		buf[0] = '\0';
		return buf;
	}
	// remove comments
	if((ptr = strchr(buf, '#')) != NULL)
		*ptr = '\0';
	if((ptr = strchr(buf, ';')) != NULL)
		*ptr = '\0';
	if((ptr = strchr(buf, '/')) != NULL) {
		if(*(ptr+1) == '/')
			*ptr = '\0';
	}
	// move ptr to the end, again
	for(ptr = buf; *ptr; ptr++)
		;
	--ptr;
	// remove comments
	while(ptr >= buf) {
		if(*ptr == '#')
			*ptr = '\0';
		ptr--;
	}
	// move ptr to the end, again
	for(ptr = buf; *ptr; ptr++)
		;
	--ptr;
	// remove tail spaces
	while(ptr >= buf && isspace(*ptr))
		*ptr-- = '\0';
	//
	return buf;
}

static int
ga_conf_parse(const char *filename, int lineno, char *buf) {
	char *option, *token; //, *saveptr;
	char *leftbracket, *rightbracket;
	gaConfVar gcv;
	//
	option = buf;
	if((token = strchr(buf, '=')) == NULL) {
		return 0;
	}
	if(*(token+1) == '\0') {
		return 0;
	}
	*token++ = '\0';
	//
	option = ga_conf_trim(option);
	if(*option == '\0')
		return 0;
	//
	token = ga_conf_trim(token);
	if(token[0] == '\0')
		return 0;
	// check if its a include
	if(strcmp(option, "include") == 0) {
#ifdef WIN32
		char incfile[_MAX_PATH];
		char tmpdn[_MAX_DIR];
		char drive[_MAX_DRIVE], tmpfn[_MAX_FNAME];
		char *ptr = incfile;
		if(token[0] == '/' || token[0] == '\\' || token[1] == ':') {
			strncpy(incfile, token, sizeof(incfile));
		} else {
			_splitpath(filename, drive, tmpdn, tmpfn, NULL);
			_makepath(incfile, drive, tmpdn, token, NULL);
		}
		// replace '/' with '\\'
		while(*ptr) {
			if(*ptr == '/')
				*ptr = '\\';
			ptr++;
		}
#else
		char incfile[PATH_MAX];
		char tmpdn[PATH_MAX];
		//
		strncpy(tmpdn, filename, sizeof(tmpdn));
		if(token[0]=='/') {
			strncpy(incfile, token, sizeof(incfile));
		} else {
			snprintf(incfile, sizeof(incfile), "%s/%s", dirname(tmpdn), token);
		}
#endif
		ga_error("# include: %s\n", incfile);
		return ga_conf_load(incfile);
	}
	// check if its a map
	if((leftbracket = strchr(option, '[')) != NULL) {
		rightbracket = strchr(leftbracket+1, ']');
		if(rightbracket == NULL) {
			ga_error("# %s:%d: malformed option (%s without right bracket).\n",
				filename, lineno, option);
			return -1;
		}
		// no key specified
		if(leftbracket + 1 == rightbracket) {
			ga_error("# %s:%d: malformed option (%s without a key).\n",
				filename, lineno, option);
			return -1;
		}
		// garbage after rightbracket?
		if(*(rightbracket+1) != '\0') {
			ga_error("# %s:%d: malformed option (%s?).\n",
				filename, lineno, option);
			return -1;
		}
		*leftbracket = '\0';
		leftbracket++;
		*rightbracket = '\0';
	}
	// its a map
	if(leftbracket != NULL) {
		//ga_error("%s[%s] = %s\n", option, leftbracket, token);
		ga_vars[option][leftbracket] = token;
	} else {
		//ga_error("%s = %s\n", option, token);
		ga_vars[option] = token;
	}
	return 0;
}

//

int
ga_conf_load(const char *filename) {
	FILE *fp;
	char buf[8192];
	int lineno = 0;
	//
	if(filename == NULL)
		return -1;
	if((fp = fopen(filename, "rt")) == NULL) {
		return -1;
	}
	while(fgets(buf, sizeof(buf), fp) != NULL) {
		lineno++;
		if(ga_conf_parse(filename, lineno, buf) < 0) {
			fclose(fp);
			return -1;
		}
	}
	fclose(fp);
	return lineno;
}

int
ga_url_parse(const char *url) {
	char *ptr, servername[1024], serverport[64];
	//
	if(url == NULL)
		return -1;
	if(strncasecmp("rtsp://", url, 7) != 0)
		return -1;
	strncpy(servername, url+7, sizeof(servername));
	for(ptr = servername; *ptr; ptr++) {
		if(*ptr == '/') {
			*ptr = '\0';
			break;
		}
		if(*ptr == ':') {
			unsigned i;
			*ptr = '\0';
			for(	++ptr, i = 0;
				isdigit(*ptr) && i < sizeof(serverport)-1;
				i++) {
				//
				serverport[i] = *ptr++;
			}
			serverport[i] = '\0';
			ga_conf_writev("server-port", serverport);
			break;
		}
	}
	ga_conf_writev("server-url", url);
	ga_conf_writev("server-name", servername);
	return 0;
}

void
ga_conf_clear() {
	ga_vars.clear();
	ga_vmi = ga_vars.begin();
	return;
}

char *
ga_conf_readv(const char *key, char *store, int slen) {
	map<string,gaConfVar>::iterator mi;
	if((mi = ga_vars.find(key)) == ga_vars.end())
		return NULL;
	if(mi->second.value().c_str() == NULL)
		return NULL;
	if(store == NULL)
		return strdup(mi->second.value().c_str());
	strncpy(store, mi->second.value().c_str(), slen);
	return store;
}

int
ga_conf_readint(const char *key) {
	char buf[64];
	char *ptr = ga_conf_readv(key, buf, sizeof(buf));
	if(ptr == NULL)
		return INT_MIN;
	return strtol(ptr, NULL, 0);
}

static int
ga_conf_multiple_int(char *buf, int *val, int n) {
	int reads = 0;
	char *endptr, *ptr = buf;
	while(reads < n) {
		val[reads] = strtol(ptr, &endptr, 0);
		if(ptr == endptr)
			break;
		ptr = endptr;
		reads++;
	}
	return reads;
}

int
ga_conf_readints(const char *key, int *val, int n) {
	char buf[1024];
	char *ptr = ga_conf_readv(key, buf, sizeof(buf));
	if(ptr == NULL)
		return 0;
	return ga_conf_multiple_int(buf, val, n);
}

int
ga_conf_boolval(const char *ptr, int defval) {
	if(strcasecmp(ptr, "true") == 0
	|| strcasecmp(ptr, "1") ==0
	|| strcasecmp(ptr, "y") ==0
	|| strcasecmp(ptr, "yes") == 0
	|| strcasecmp(ptr, "enabled") == 0
	|| strcasecmp(ptr, "enable") == 0)
		return 1;
	if(strcasecmp(ptr, "false") == 0
	|| strcasecmp(ptr, "0") ==0
	|| strcasecmp(ptr, "n") ==0
	|| strcasecmp(ptr, "no") == 0
	|| strcasecmp(ptr, "disabled") == 0
	|| strcasecmp(ptr, "disable") == 0)
		return 0;
	return defval;
}

int
ga_conf_readbool(const char *key, int defval) {
	char buf[64];
	char *ptr = ga_conf_readv(key, buf, sizeof(buf));
	if(ptr == NULL)
		return defval;
	return ga_conf_boolval(ptr, defval);
}

int
ga_conf_writev(const char *key, const char *value) {
	ga_vars[key] = value;
	return 0;
}

void
ga_conf_erase(const char *key) {
	ga_vars.erase(key);
	return;
}

int
ga_conf_ismap(const char *key) {
	return ga_conf_mapsize(key) > 0 ? 1 : 0;
}

int
ga_conf_haskey(const char *mapname, const char *key) {
	map<string,gaConfVar>::iterator mi;
	if((mi = ga_vars.find(mapname)) == ga_vars.end())
		return 0;
	return mi->second.haskey(key);
}

int
ga_conf_mapsize(const char *mapname) {
	map<string,gaConfVar>::iterator mi;
	if((mi = ga_vars.find(mapname)) == ga_vars.end())
		return 0;
	return mi->second.msize();
}

char *
ga_conf_mapreadv(const char *mapname, const char *key, char *store, int slen) {
	map<string,gaConfVar>::iterator mi;
	if((mi = ga_vars.find(mapname)) == ga_vars.end())
		return NULL;
	if((mi->second)[key] == "")
		return NULL;
	if(store == NULL)
		return strdup((mi->second)[key].c_str());
	strncpy(store, (mi->second)[key].c_str(), slen);
	return store;
}

int
ga_conf_mapreadint(const char *mapname, const char *key) {
	char buf[64];
	char *ptr = ga_conf_mapreadv(mapname, key, buf, sizeof(buf));
	if(ptr == NULL)
		return INT_MIN;
	return strtol(ptr, NULL, 0);
}

int
ga_conf_mapreadints(const char *mapname, const char *key, int *val, int n) {
	char buf[1024];
	char *ptr = ga_conf_mapreadv(mapname, key, buf, sizeof(buf));
	if(ptr == NULL)
		return 0;
	return ga_conf_multiple_int(buf, val, n);
}

int
ga_conf_mapreadbool(const char *mapname, const char *key, int defval) {
	char buf[64];
	char *ptr = ga_conf_mapreadv(mapname, key, buf, sizeof(buf));
	if(ptr == NULL)
		return defval;
	return ga_conf_boolval(ptr, defval);
}

int
ga_conf_mapwritev(const char *mapname, const char *key, const char *value) {
	ga_vars[mapname][key] = value;
	return 0;
}

void
ga_conf_maperase(const char *mapname, const char *key) {
	map<string,gaConfVar>::iterator mi;
	if((mi = ga_vars.find(mapname)) == ga_vars.end())
		return;
	ga_vars.erase(mi);
	return;
}

void
ga_conf_mapreset(const char *mapname) {
	map<string,gaConfVar>::iterator mi;
	if((mi = ga_vars.find(mapname)) == ga_vars.end())
		return;
	mi->second.mreset();
	return;
}

char *
ga_conf_mapkey(const char *mapname, char *keystore, int klen) {
	map<string,gaConfVar>::iterator mi;
	if((mi = ga_vars.find(mapname)) == ga_vars.end())
		return NULL;
	if(mi->second.mkey() == "")
		return NULL;
	if(keystore == NULL)
		return strdup(mi->second.mkey().c_str());
	strncpy(keystore, mi->second.mkey().c_str(), klen);
	return keystore;
}

char *
ga_conf_mapvalue(const char *mapname, char *valstore, int vlen) {
	map<string,gaConfVar>::iterator mi;
	if((mi = ga_vars.find(mapname)) == ga_vars.end())
		return NULL;
	if(mi->second.mkey() == "")
		return NULL;
	if(valstore == NULL)
		return strdup(mi->second.mvalue().c_str());
	strncpy(valstore, mi->second.mvalue().c_str(), vlen);
	return valstore;
}

char *
ga_conf_mapnextkey(const char *mapname, char *keystore, int klen) {
	map<string,gaConfVar>::iterator mi;
	string k = "";
	//
	if((mi = ga_vars.find(mapname)) == ga_vars.end())
		return NULL;
	k = mi->second.mnextkey();
	if(k == "")
		return NULL;
	if(keystore == NULL)
		return strdup(k.c_str());
	strncpy(keystore, k.c_str(), klen);
	return keystore;
}

void ga_conf_reset() {
	ga_vmi = ga_vars.begin();
}

const char *ga_conf_key() {
	if(ga_vmi == ga_vars.end())
		return NULL;
	return ga_vmi->first.c_str();
}

const char *ga_conf_nextkey() {
	if(ga_vmi == ga_vars.end())
		return NULL;
	// move forward
	ga_vmi++;
	//
	if(ga_vmi == ga_vars.end())
		return NULL;
	return ga_vmi->first.c_str();
}

