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
 * Class used to store a single GamingAnywhere parameter: implementations
 */

#include <stdio.h>
#include "ga-confvar.h"

using namespace std;

/**
 * Reset everything.
 */
void
gaConfVar::clear() {
	this->data = "";
	this->mapdata.clear();
	this->mi = this->mapdata.begin();
}

gaConfVar::gaConfVar() {
	this->clear();
}

/**
 * Get the stored value.
 */
string
gaConfVar::value() {
	return this->data;
}

/**
 * Implement the = operator for assign plain-text value.
 */
gaConfVar&
gaConfVar::operator=(const char *value) {
	this->data = value;
	this->mapdata.clear();
	this->mi = this->mapdata.begin();
	return *this;
}

/**
 * Implement the = operator for assign plain-text value.
 */
gaConfVar&
gaConfVar::operator=(const string value) {
	this->data = value;
	this->mapdata.clear();
	this->mi = this->mapdata.begin();
	return *this;
}

/**
 * Implement the = operator for assign from another \em gaConfVar class.
 */
gaConfVar&
gaConfVar::operator=(const gaConfVar var) {
	this->data = var.data;
	this->mapdata = var.mapdata;
	this->mi = this->mapdata.begin();
	return *this;
}

/**
 * Implement the [] operator for reading from parameter map.
 */
string&
gaConfVar::operator[](const char *key) {
	return mapdata[key];
}

/**
 * Implement the [] operator for reading from parameter map.
 */
string&
gaConfVar::operator[](const string key) {
	return mapdata[key];
}

/**
 * Determine if the parameter map contains a given key.
 *
 * @param key [in] The key to be tested.
 * @return true on success, or false on error.
 */
bool
gaConfVar::haskey(const char *key) {
	return (mapdata.find(key) != mapdata.end());
}

/**
 * Return the number of key/value pairs stored in the parameter map.
 *
 * @return The number of stored key/value pairs.
 */
int
gaConfVar::msize() {
	return this->mapdata.size();
}

/**
 * Reset the iteration counter for the parameter map.
 */
void
gaConfVar::mreset() {
	this->mi = this->mapdata.begin();
	return;
}

/**
 * Get the current key from the parameter map.
 *
 * @return A non-empty key string, or an empty string if it reaches the end.
 */
string
gaConfVar::mkey() {
	if(this->mi == this->mapdata.end())
		return "";
	return mi->first;
}

/**
 * Get the current value from the parameter map.
 *
 * @return A non-empty value string, or an empty string if it reaches the end.
 */
string
gaConfVar::mvalue() {
	if(this->mi == this->mapdata.end())
		return "";
	return mi->second;
}

/**
 * Advance the iteration pointer and get the key from the parameter map.
 *
 * @return A non-empty key string, or an empty string if it reaches the end.
 */
string
gaConfVar::mnextkey() {
	if(this->mi == this->mapdata.end())
		return "";
	// move forward
	this->mi++;
	//
	if(this->mi == this->mapdata.end())
		return "";
	return mi->first;
}

