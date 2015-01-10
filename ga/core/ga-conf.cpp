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
 * GamingAnywhere configuration file loader: implementations
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

/** Global variables used to store loaded configurations */
static map<string,gaConfVar> ga_vars;
static map<string,gaConfVar>::iterator ga_vmi = ga_vars.begin();

/**
 * Trim a configuration string. This is an internal function.
 *
 * @param buf [in] Pointer to a loaded string from the configuration file.
 * @return Pointer to the trimmed string.
 *
 * Note that this function does not preserve the content of the input string.
 * This function pre-process a loaded string based on the following logics:
 * - Remove heading space characters
 * - Remove section header '[':
 *	Section headers is only for improving configuration file's readability.
 * - Remove comment char from head: '#', ';', and '//'
 * - Remove comment char from tail: '#'
 * - Remove space characters  and the end
 */
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

/**
 * Parse a single configuration string. This is an internal function.
 *
 * @param filename [in] The current configuration file name.
 * @param lineno [in] The current line number of the string.
 * @param buf [in] The content of the current configuration string.
 * @return 0 on success, or -1 on error.
 */
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

/**
 * Load a configuration file.
 *
 * @param filename [in] The configuration pathname
 * @return 0 on success, or -1 on error.
 *
 * The given configuration file is parsed and loaded into the system.
 * Other system components can then read the loaded parameters using
 * functions exported from this file.
 */
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

/**
 * Parse a GamingAnywere server URL.
 *
 * @param url [in] Pointer to the URL string.
 * @return 0 on success, or -1 on error.
 *
 * The server URL is in the form of rtsp://server-address:port/path.
 * The parsed URL is stored in the configuration parameter:
 * - \em server-url: Contains the full URL;
 * - \em server-name: Contains only the server-address part;
 * - \em server-port: Contains only the port number (if given)
 */
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

/**
 * Clear all loaded configuration.
 */
void
ga_conf_clear() {
	ga_vars.clear();
	ga_vmi = ga_vars.begin();
	return;
}

/**
 * Load the value of a parameter in string format.
 *
 * @param key [in] The parameter to be loaded.
 * @param store [in,out] Buffer to store the loaded value.
 * @param slen[in] Length of buffers pointed by \a store.
 * @return A pointer to the loaded value on success, or NULL on error.
 *
 * Note that if \a store is NULL, this function automatically allocates
 * spaces to store the result. You may need to release the memory by
 * calling \em free().
 */
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

/**
 * Load the value of a parameter as an integer.
 *
 * @param key [in] The parameter to be loaded.
 * @return The integer value of the parameter.
 *
 * Note that when the given parameter is not defined,
 * this function returns zero.
 */
int
ga_conf_readint(const char *key) {
	char buf[64];
	char *ptr = ga_conf_readv(key, buf, sizeof(buf));
	if(ptr == NULL)
		return 0;
	return strtol(ptr, NULL, 0);
}

/**
 * Load the value of a parameter as an double float number.
 *
 * @param key [in] The parameter to be loaded.
 * @return The double float value of the parameter.
 *
 * Note that when the given parameter is not defined,
 * this function returns 0.0.
 */
double
ga_conf_readdouble(const char *key) {
	char buf[64];
	char *ptr = ga_conf_readv(key, buf, sizeof(buf));
	if(ptr == NULL)
		return 0.0;
	return strtod(ptr, NULL);
}

/**
 * Load multiple integer values specified in a parameter.
 * This is an internal function.
 *
 * @param key [in] The parameter to be loaded.
 * @param val [out] An integer array used to store loaded integers.
 * @param n [in] The expected number of integers to be loaded.
 * @return The exact number of integers loaded into \a val.
 *	This number can be less than or equal to \a n.
 *
 * This function attempts to loads \a n numbers from the parameter and
 * store the loaded number in \a val in the order.
 * 
 * A sample configuration line is: param = 1 2 3.
 */
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

/**
 * Load multiple integer values specified in a parameter.
 *
 * @param key [in] The parameter to be loaded.
 * @param val [out] An integer array used to store loaded integers.
 * @param n [in] The expected number of integers to be loaded.
 * @return The exact number of integers loaded into \a val.
 *	This number can be less than or equal to \a n.
 *
 * This function attempts to loads \a n numbers from the parameter and
 * store the loaded number in \a val in the order.
 * 
 * A sample configuration line is: param = 1 2 3.
 */
int
ga_conf_readints(const char *key, int *val, int n) {
	char buf[1024];
	char *ptr = ga_conf_readv(key, buf, sizeof(buf));
	if(ptr == NULL)
		return 0;
	return ga_conf_multiple_int(buf, val, n);
}

/**
 * Determine whether a string represents TRUE or FALSE.
 *
 * @param ptr [in] The string to be determined.
 * @param defval [in] The default value if the string cannot be determined. 
 * @return 0 for FALSE, or 1 for TRUE.
 *
 * - The following string values are treated as TRUE: true, 1, y, yes, enabled, enable.
 * - The following string values are treated as FALSE: false, 0, n, no, disabled, disable. 
 */
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

/**
 * Load the value of a parameter as boolean.
 *
 * @param ptr [in] The parameter to be loaded.
 * @param defval [in] The returned value if the parameter
 *	is not defined in the configuration file.
 * @return 0 for FALSE, or 1 for TRUE.
 *
 * See the definitions defined in \em ga_conf_boolval function:
 * - The following string values are treated as TRUE: true, 1, y, yes, enabled, enable.
 * - The following string values are treated as FALSE: false, 0, n, no, disabled, disable. 
 */
int
ga_conf_readbool(const char *key, int defval) {
	char buf[64];
	char *ptr = ga_conf_readv(key, buf, sizeof(buf));
	if(ptr == NULL)
		return defval;
	return ga_conf_boolval(ptr, defval);
}

/**
 * Add a parameter value into system runtime configuration.
 *
 * @param key [in] The parameter name.
 * @param value [in] The parameter value.
 *
 * Note again, this function DOES NOT MODIFY the configuration file.
 * It only adds/changes the parameters in the runtime configuration.
 */
int
ga_conf_writev(const char *key, const char *value) {
	ga_vars[key] = value;
	return 0;
}

/**
 * Delete a parameter value from system runtime configuration.
 *
 * @param key [in] The parameter to be deleted.
 *
 * Note again, this function DOES NOT MODIFY the configuration file.
 * It only deletes the parameters in the runtime configuration.
 */
void
ga_conf_erase(const char *key) {
	ga_vars.erase(key);
	return;
}

/**
 * Test if a given parameter is defined as a 'parameter map'.
 *
 * @param key [in] The parameter to be tested.
 * @return 1 if the parameter is defined as a map, or 0 if not.
 *
 * A 'parameter map' is defined like: param[key] = value.\n
 * in the configuration file.
 */
int
ga_conf_ismap(const char *key) {
	return ga_conf_mapsize(key) > 0 ? 1 : 0;
}

/**
 * Determine if a 'parameter map' contains a given \a key.
 *
 * @param mapname [in] The name of the parameter map.
 * @param key [in] The key to be tested.
 * @return 0 if the key is not found, or non-zero if found.
 */
int
ga_conf_haskey(const char *mapname, const char *key) {
	map<string,gaConfVar>::iterator mi;
	if((mi = ga_vars.find(mapname)) == ga_vars.end())
		return 0;
	return mi->second.haskey(key);
}

/**
 * Get the number of keys defined in a parameter map
 *
 * @param mapname [in] The name of the parameter map.
 * @return The number of keys defined in the parameter map.
 */
int
ga_conf_mapsize(const char *mapname) {
	map<string,gaConfVar>::iterator mi;
	if((mi = ga_vars.find(mapname)) == ga_vars.end())
		return 0;
	return mi->second.msize();
}

/**
 * Read a string value from a key of a parameter map.
 *
 * @param mapname [in] The name of the parameter map.
 * @param key [in] The key to be retrieved.
 * @param store [out] Buffer to store the loaded value.
 * @param slen [in] Length of buffers pointed by \a store.
 * @return Pointer to the loaded value on success, or NULL on error.
 *
 * Note that if \a store is NULL, this function automatically allocates
 * spaces to store the result. You may need to release the memory by
 * calling \em free().
 */
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

/**
 * Read an integer value from a key of a parameter map.
 *
 * @param mapname [in] The name of the parameter map.
 * @param key [in] The key to be retrieved.
 * @return The loaded integer value.
 *
 * Note that when the given key is not defined,
 * this function returns zero.
 */
int
ga_conf_mapreadint(const char *mapname, const char *key) {
	char buf[64];
	char *ptr = ga_conf_mapreadv(mapname, key, buf, sizeof(buf));
	if(ptr == NULL)
		return 0;
	return strtol(ptr, NULL, 0);
}

/**
 * Read multiple integer values from a key of a parameter map.
 *
 * @param mapname [in] The name of the parameter map.
 * @param key [in] The key to be retrieved.
 * @param val [out] The array used to store loaded integers.
 * @param n [in] The expected number of integers to be loaded.
 * @return The exact number of integers loaded into \a val.
 *	This number can be less than or equal to \a n.
 *
 * This function attempts to loads \a n numbers from
 * the key of the parameter map and store the loaded number in
 * \a val in the order.
 * 
 * A sample configuration line is: param[key] = 1 2 3.
 */
int
ga_conf_mapreadints(const char *mapname, const char *key, int *val, int n) {
	char buf[1024];
	char *ptr = ga_conf_mapreadv(mapname, key, buf, sizeof(buf));
	if(ptr == NULL)
		return 0;
	return ga_conf_multiple_int(buf, val, n);
}

/**
 * Read a double float value from a key of a parameter map.
 *
 * @param mapname [in] The name of the parameter map.
 * @param key [in] The key to be retrieved.
 * @return The loaded double float value.
 *
 * Note that when the given key is not defined,
 * this function returns 0.0.
 */
double
ga_conf_mapreaddouble(const char *mapname, const char *key) {
	char buf[64];
	char *ptr = ga_conf_mapreadv(mapname, key, buf, sizeof(buf));
	if(ptr == NULL)
		return 0.0;
	return strtod(ptr, NULL);
}

/**
 * Read a boolean value from a key of a parameter map.
 *
 * @param mapname [in] The name of the parameter map.
 * @param key [in] The key to be retrieved.
 * @param defval [in] The returned default value if \a key is not defined.
 * @return 0 for FALSE, or 1 for TRUE.
 *
 * See the definitions defined in \em ga_conf_boolval function:
 * - The following string values are treated as TRUE: true, 1, y, yes, enabled, enable.
 * - The following string values are treated as FALSE: false, 0, n, no, disabled, disable. 
 */
int
ga_conf_mapreadbool(const char *mapname, const char *key, int defval) {
	char buf[64];
	char *ptr = ga_conf_mapreadv(mapname, key, buf, sizeof(buf));
	if(ptr == NULL)
		return defval;
	return ga_conf_boolval(ptr, defval);
}

/**
 * Add a parameter map key's value into system runtime configuration.
 *
 * @param mapname [in] The parameter map name.
 * @param key [in] The key name.
 * @param value [in] The parameter value.
 * @return This function always returns zero.
 *
 * Note again, this function DOES NOT MODIFY the configuration file.
 * It only adds/changes the parameters in the runtime configuration.
 */
int
ga_conf_mapwritev(const char *mapname, const char *key, const char *value) {
	ga_vars[mapname][key] = value;
	return 0;
}

/**
 * Delete a parameter map key's value into system runtime configuration.
 *
 * @param mapname [in] The parameter map name.
 * @param key [in] The key name.
 *
 * Note again, this function DOES NOT MODIFY the configuration file.
 * It only deletes the parameters in the runtime configuration.
 */
void
ga_conf_maperase(const char *mapname, const char *key) {
	map<string,gaConfVar>::iterator mi;
	if((mi = ga_vars.find(mapname)) == ga_vars.end())
		return;
	ga_vars.erase(mi);
	return;
}

/**
 * Reset the iteration pointer of a parameter map.
 *
 * @param mapname [in] The name of the parameter map.
 *
 * This function is usually called before you attempt to enumerate
 * key/values in a parameter map.
 */
void
ga_conf_mapreset(const char *mapname) {
	map<string,gaConfVar>::iterator mi;
	if((mi = ga_vars.find(mapname)) == ga_vars.end())
		return;
	mi->second.mreset();
	return;
}

/**
 * Get the current key from a parameter map.
 *
 * @param mapname [in] The name of the parameter map.
 * @param keystore [out] The buffer used to stora the current key.
 * @param klen [in] The buffer lenfgth of \a keystore.
 * @return Pointer to the key string, or NULL on error.
 *
 * Note that if \a keystore is NULL, this function automatically allocates
 * spaces to store the key. You may need to release the memory by
 * calling \em free().
 */
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

/**
 * Get the current value from a parameter map.
 *
 * @param mapname [in] The name of the parameter map.
 * @param valstore [out] The buffer used to stora the current key.
 * @param vlen [in] The buffer lenfgth of \a valstore.
 * @return Pointer to the value string, or NULL on error.
 *
 * Note that if \a valstore is NULL, this function automatically allocates
 * spaces to store the key. You may need to release the memory by
 * calling \em free().
 */
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

/**
 * Advance the iteration pointer of a parameter map and get its key value.
 *
 * @param mapname [in] The name of the parameter map.
 * @param keystore [out] The buffer used to stora the next key.
 * @param klen [in] The buffer lenfgth of \a keystore.
 *
 * This function is uaually called during an enumeration process.
 * Note that if \a keystore is NULL, this function automatically allocates
 * spaces to store the key. You may need to release the memory by
 * calling \em free().
 */
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

/**
 * Reset the global runtime configuration iteration pointer.
 *
 * This function is used to enumerate all runtime configurations.
 */
void ga_conf_reset() {
	ga_vmi = ga_vars.begin();
}

/**
 * Get the current key of the gloabl runtime configuration.
 *
 * @return The current key value.
 *
 * This function is used to enumerate all runtime configurations.
 */
const char *ga_conf_key() {
	if(ga_vmi == ga_vars.end())
		return NULL;
	return ga_vmi->first.c_str();
}

/**
 * Advance and get the key of the global runtime configuratin.
 *
 * @return The next key value, or NULL if it reaches the end.
 *
 * This function is used to enumerate all runtime configurations.
 */
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

