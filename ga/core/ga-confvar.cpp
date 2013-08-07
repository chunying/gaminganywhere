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

#include <stdio.h>
#include "ga-confvar.h"

using namespace std;

void
gaConfVar::clear() {
	this->data = "";
	this->mapdata.clear();
	this->mi = this->mapdata.begin();
}

gaConfVar::gaConfVar() {
	this->clear();
}

string
gaConfVar::value() {
	return this->data;
}

gaConfVar&
gaConfVar::operator=(const char *value) {
	this->data = value;
	this->mapdata.clear();
	this->mi = this->mapdata.begin();
	return *this;
}

gaConfVar&
gaConfVar::operator=(const string value) {
	this->data = value;
	this->mapdata.clear();
	this->mi = this->mapdata.begin();
	return *this;
}

gaConfVar&
gaConfVar::operator=(const gaConfVar var) {
	this->data = var.data;
	this->mapdata = var.mapdata;
	this->mi = this->mapdata.begin();
	return *this;
}

string&
gaConfVar::operator[](const char *key) {
	return mapdata[key];
}

string&
gaConfVar::operator[](const string key) {
	return mapdata[key];
}

bool
gaConfVar::haskey(const char *key) {
	return (mapdata.find(key) != mapdata.end());
}

int
gaConfVar::msize() {
	return this->mapdata.size();
}

void
gaConfVar::mreset() {
	this->mi = this->mapdata.begin();
	return;
}

string
gaConfVar::mkey() {
	if(this->mi == this->mapdata.end())
		return "";
	return mi->first;
}

string
gaConfVar::mvalue() {
	if(this->mi == this->mapdata.end())
		return "";
	return mi->second;
}

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

