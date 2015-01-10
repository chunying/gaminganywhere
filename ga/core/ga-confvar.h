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
 * Class used to store a single GamingAnywhere parameter: header files
 */

#ifndef __GA_CONFVAR_H__
#define	__GA_CONFVAR_H__

#include <map>
#include <string>

/**
 * Class used to store a single GamingAnywhere parameter.
 */
class gaConfVar {
private:
	std::string data;	/**< Store the plain-text value */
	std::map<std::string,std::string> mapdata;	/**< Used to store the parameter map's key and value */
	std::map<std::string,std::string>::iterator mi;	/**< Used for iterate throughout the parameter map */
	void clear();
public:
	gaConfVar();
	std::string value();
	gaConfVar& operator=(const char *value);
	gaConfVar& operator=(const std::string value);
	gaConfVar& operator=(const gaConfVar var);
	std::string& operator[](const char *key);
	std::string& operator[](const std::string key);
	bool haskey(const char *key);
	// iteratively access to the map
	int msize();
	void mreset();
	std::string mkey();
	std::string mvalue();
	std::string mnextkey();
};

#endif /* __GA_CONFVAR_H__ */
