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

#include <stdio.h>
#include <stdlib.h>

#include "ga-common.h"
#include "ga-conf.h"

#include "easyhook.h"

int
hook_and_launch(const char *ga_root, const char *config_path, const char *app_exe) {
	PROCESS_INFORMATION procInfo;
	STARTUPINFO startupInfo;
	HINSTANCE hDLL;
	int (*install_hook)(const char *, const char *, const char *);
	int (*uninstall_hook)();
	char cmdline[2048];
	char buf[2048], *ptr;
	int cmdpos, cmdspace = sizeof(cmdline);
	//
	int r;
	unsigned long pid = 0;
	wchar_t app_exe_w[1024];
	wchar_t cmdline_w[2048], *ptr_cmdline_w = NULL;
	wchar_t dllpath_w[1024];

	if(ga_root == NULL || config_path == NULL) {
		ga_error("[hook_and_launch] no ga-root nor configuration were specified.\n");
		return -1;
	}

	// handle environment variables
	do {
		char s_drive[_MAX_DRIVE], s_dir[_MAX_DIR], s_fname[_MAX_FNAME];
		_splitpath(app_exe, s_drive, s_dir, s_fname, NULL);
		_putenv_s("GA_APPEXE", s_fname);
		_putenv_s("GA_ROOT", ga_root);
		_putenv_s("GA_CONFIG", config_path);
		// additional custom variables?
		if(ga_conf_mapsize("game-env") == 0)
			break;
		ga_conf_mapreset("game-env");
		for(	ptr = ga_conf_mapkey("game-env", buf, sizeof(buf));
			ptr != NULL;
			ptr = ga_conf_mapnextkey("game-env", buf, sizeof(buf))) {
			//
			char *val, *envval, valbuf[2048];
			val = ga_conf_mapvalue("game-env", valbuf, sizeof(valbuf));
			if(val == NULL)
				continue;
			for(envval = val; *envval && *envval != '='; envval++)
				;
			if(*envval != '=')
				break;
			*envval++ = '\0';
			ga_error("Game env: %s=%s\n", val, envval);
			_putenv_s(val, envval);
		}
	} while(0);

	cmdline[0] = '\0';
	cmdpos = 0;
	if(ga_conf_mapsize("game-argv") > 0) {
		int n;
		ga_conf_mapreset("game-argv");
		cmdpos = snprintf(cmdline, cmdspace, "\"%s\"", app_exe);
		for(	ptr = ga_conf_mapkey("game-argv", buf, sizeof(buf));
			ptr != NULL && cmdpos < cmdspace;
			ptr = ga_conf_mapnextkey("game-argv", buf, sizeof(buf))) {
			//
			char *val, valbuf[1024];
			val = ga_conf_mapvalue("game-argv", valbuf, sizeof(valbuf));
			if(val == NULL)
				continue;
			ga_error("Game arg: %s\n", val);
			n = snprintf(cmdline+cmdpos, cmdspace-cmdpos, " \"%s\"",
				val);
			cmdpos += n;
		}
		fprintf(stderr, "cmdline: %s\n", cmdline);
	}

	snprintf(buf, sizeof(buf), "%s%s", ga_root, "ga-hook.dll");
	if(MultiByteToWideChar(CP_UTF8, 0, buf, -1, dllpath_w, sizeof(dllpath_w)/sizeof(wchar_t)) <= 0) {
		fprintf(stderr, "error converting dllpath to wchar_t.\n");
		return -1;
	}
	fwprintf(stderr, L"dllpath: %s\n", dllpath_w);

	if(MultiByteToWideChar(CP_UTF8, 0, app_exe, -1, app_exe_w, sizeof(app_exe_w)/sizeof(wchar_t)) <= 0) {
		fprintf(stderr, "error converting app_exe to wchar_t.\n");
		return -1;
	}
	fwprintf(stderr, L"appexe: %s\n", app_exe_w);

	if(cmdpos > 0) {
		if(MultiByteToWideChar(CP_UTF8, 0, cmdline, -1, cmdline_w, sizeof(cmdline_w)/sizeof(wchar_t)) <= 0) {
			fprintf(stderr, "error converting arguments to wchar_t.\n");
			return -1;
		}
		ptr_cmdline_w = cmdline_w;
	} else {
		ptr_cmdline_w = NULL;
	}

	r = RhCreateAndInject(app_exe_w, /* command */
			ptr_cmdline_w,		/* command line arguments */
			0,	/* process creation flags */
			EASYHOOK_INJECT_DEFAULT,/* hook options */
#ifdef _WIN64
			NULL,			/* x86 dll */
			dllpath_w,		/* x64 dll */
#else
			dllpath_w,		/* x86 dll */
			NULL,			/* x64 dll */
#endif
			NULL, 0,		/* passthrough buffer and size */
			&pid);
	if(r == 0) {
		fprintf(stderr, "launch success (pid=%u).\n", pid);
	} else {
		fprintf(stderr, "launch failed, err=%08x.\n", r);
	}


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

