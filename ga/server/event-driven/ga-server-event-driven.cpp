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
#include <stdlib.h>

#include "ga-common.h"
#include "ga-conf.h"

int
hook_and_launch(const char *ga_root, const char *config_path, const char *app_exe) {
	PROCESS_INFORMATION procInfo;
	STARTUPINFO startupInfo;
	HINSTANCE hDLL;
	int (*install_hook)(const char *, const char *, const char *);
	int (*uninstall_hook)();

	// load ga-hook.dll
	if((hDLL = LoadLibrary("ga-hook.dll")) == NULL) {
		fprintf(stderr, "LoadLibrary(ga-hook.dll) failed: 0x%08x\n", GetLastError());
		return -1;
	}
	fprintf(stderr, "LoadLibrary(ga-hook.dll) success (0x%p).\n", hDLL);

	if((install_hook = (int (*)(const char *, const char *, const char *))
			GetProcAddress(hDLL, "install_hook")) == NULL) {
		fprintf(stderr, "GetProcAddress(install_hook) failed: 0x%08x\n", GetLastError());
		return -1;
	}
	if((uninstall_hook = (int (*)())
			GetProcAddress(hDLL, "uninstall_hook")) == NULL) {
		fprintf(stderr, "GetProcAddress(uninstall_hook) failed: 0x%08x\n", GetLastError());
		return -1;
	}
	fprintf(stderr, "GetProcAddress(install_hook) success (0x%p).\n", install_hook);

	install_hook(ga_root, config_path, app_exe);

	// launch the app
	ZeroMemory(&procInfo, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&startupInfo, sizeof(STARTUPINFO));

	if (CreateProcess(app_exe, NULL, NULL, NULL, FALSE,
		NORMAL_PRIORITY_CLASS, NULL, NULL, 
		&startupInfo, &procInfo) == 0) {
		//
		fprintf(stderr, "CreateProcess failed: 0x%08x\n", GetLastError());
		return -1;
	}

	fprintf(stderr, "Waiting for app termination ...\n");

	WaitForSingleObject(procInfo.hProcess, INFINITE);

	uninstall_hook();

	return 0;
}

int
main(int argc, char *argv[]) {
	int ret;
	char *ptr, buf[8192];
	char app_dir[1024];
	char app_exe[1024];
	char loader_exe[1024];
	char loader_dir[1024];
	char config_path[1024];
	char s_drive[_MAX_DRIVE], s_dir[_MAX_DIR], s_fname[_MAX_FNAME];
#ifdef WIN32
	if(CoInitializeEx(NULL, COINIT_MULTITHREADED) < 0) {
		fprintf(stderr, "cannot initialize COM.\n");
		return -1;
	}
#endif
	//
	if(argc < 2) {
		fprintf(stderr, "usage: %s config-file\n", argv[0]);
		return -1;
	}
	//
	if(ga_init(argv[1], NULL) < 0) {
		fprintf(stderr, "GA initialization failed.\n");
		return -1;
	}
	//
	app_dir[0] = '\0';
	if((ptr = ga_conf_readv("game-dir", app_dir, sizeof(app_dir))) != NULL) {
		fprintf(stderr, "Use user-defined game-dir: %s\n", app_dir);
	}
	//
	if((ptr = ga_conf_readv("game-exe", app_exe, sizeof(app_exe))) == NULL) {
		fprintf(stderr, "no game executable provided.\n");
		return -1;
	}
	// get loader's info
	GetModuleFileName(NULL, loader_exe, sizeof(loader_exe));
	_splitpath(loader_exe, s_drive, s_dir, s_fname, NULL);
	snprintf(loader_dir, sizeof(loader_dir), "%s%s", s_drive, s_dir);
	fprintf(stderr, "Loader: %s (in %s)\n", loader_exe, loader_dir);

	// get app's info
	if(app_dir[0] == '\0') {
		_splitpath(app_exe, s_drive, s_dir, s_fname, NULL);
		snprintf(app_dir, sizeof(app_dir), "%s%s", s_drive, s_dir);
		fprintf(stderr, "app: %s (in %s)\n", app_exe, app_dir);
	}

	// full config path - assume placed in the same dir as ga-loader.exe
	if(argv[1][0] == '\\' || argv[1][0] == '/' || argv[1][1] == ':') {
		// absolute path
		strncpy(config_path, argv[1], sizeof(config_path));
	} else {
		// relative path
		snprintf(config_path, sizeof(config_path), "%s%s", loader_dir, argv[1]);
	}
	fprintf(stderr, "Configuration file: %s\n", config_path);

	// change CWD to app directory
	if (SetCurrentDirectory(app_dir) == 0) {
		fprintf(stderr, "Change to app directory failed (0x%08x)\n",
			GetLastError());
		return -1;
	}

	// XXX: assume DLL's can be found in app's directory
	//	- we will not copy them

	ret = hook_and_launch(loader_dir, config_path, app_exe);

	return ret;
}

