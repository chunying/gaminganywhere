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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>

#include "ga-common.h"
#include "ga-conf.h"

#include <string>
using namespace std;

int
main(int argc, char *argv[]) {
	char *ptr, buf[8192];
	char app_dir[1024]	= "";
	char app_exe[1024]	= "";
	char *app_arg[64]	= { NULL, NULL, NULL };
	char hook_type[64]	= "";
	char hook_audio[64]	= "";
	char launch_dir[1024]	= "";
	char config_path[1024]	= "";
	char current_path[1024]	= "";
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
	//
	if((ptr = ga_conf_readv("hook-type", hook_type, sizeof(hook_type))) == NULL) {
		fprintf(stderr, "no hook type provided.\n");
	}
	//
	if((ptr = ga_conf_readv("hook-audio", hook_audio, sizeof(hook_audio))) == NULL) {
		hook_audio[0] = '\0';
	}
	//
	ga_error("Game exe: %s\n", app_exe);
	//
	if(ga_conf_mapsize("game-argv") > 0) {
		int i = 1;
		ga_conf_mapreset("game-argv");
		for(	ptr = ga_conf_mapkey("game-argv", buf, sizeof(buf));
			ptr != NULL && i+1 < sizeof(app_arg)/sizeof(char*);
			ptr = ga_conf_mapnextkey("game-argv", buf, sizeof(buf))) {
			//
			char *val, valbuf[1024];
			val = ga_conf_mapvalue("game-argv", valbuf, sizeof(valbuf));
			if(val == NULL)
				continue;
			ga_error("Game arg: %s\n", val);
			app_arg[i++] = strdup(val);
		}
		app_arg[i] = NULL;
	}
	// get loader's info
	getcwd(current_path, sizeof(current_path));
	snprintf(launch_dir, sizeof(launch_dir), "%s/%s",
		current_path, dirname(argv[0]));

	// get app's info
	if(app_dir[0] == '\0') {
		snprintf(app_dir, sizeof(app_dir), "%s",
				dirname(strdup(app_exe)));
	}

	// full config path - assume placed in the same dir as ga-loader.exe
	if(argv[1][0] == '\\' || argv[1][0] == '/' || argv[1][1] == ':') {
		// absolute path
		strncpy(config_path, argv[1], sizeof(config_path));
	} else {
		// relative path
		snprintf(config_path, sizeof(config_path), "%s/%s", launch_dir, argv[1]);
	}
	fprintf(stderr, "Configuration file: %s\n", config_path);

	// change CWD to app directory
	if (chdir(app_dir) < 0) {
		perror("chdir failed");
		return -1;
	}
	ga_error("Game dir: switched to %s\n", app_dir);
	// launch with injection
	do {
		pid_t pid;
		char env[2048];
		//
		if((pid = fork()) == 0) {
			int i;
			string cmd = "";
			// setting up extra envs
			if(ga_conf_mapsize("game-env") == 0)
				goto no_load_env;
			ga_conf_mapreset("game-env");
			for(	ptr = ga_conf_mapkey("game-env", buf, sizeof(buf));
				ptr != NULL;
				ptr = ga_conf_mapnextkey("game-env", buf, sizeof(buf))) {
				//
				char *val, valbuf[2048];
				val = ga_conf_mapvalue("game-env", valbuf, sizeof(valbuf));
				if(val == NULL)
					continue;
				ga_error("Game env: %s\n", val);
				cmd += val;
				cmd += ' ';
			}
no_load_env:
			//
#ifdef __APPLE__
			// env: DYLD_FORCE_FLAT_NAMESPACE=1
			cmd += "DYLD_FORCE_FLAT_NAMESPACE=1 ";
#endif
			// env: LD_LIBRARY_PATH
			snprintf(env, sizeof(env),
#ifdef __APPLE__
				"DYLD_LIBRARY_PATH=%s",
#else
				"LD_LIBRARY_PATH=%s",
#endif
				launch_dir);
			cmd += env;
			cmd += " ";
			// env: LD_PRELOAD
			snprintf(env, sizeof(env),
#ifdef __APPLE__
				"DYLD_INSERT_LIBRARIES=\"%s/ga-hook-%s.dylib",
#else
				"LD_PRELOAD=\"%s/ga-hook-%s.so",
#endif
				launch_dir, hook_type);
			cmd += env;
			if(hook_audio[0] != '\0' && strcmp(hook_audio, hook_type) != 0) {
				snprintf(env, sizeof(env),
#ifdef __APPLE__
					":%s/ga-hook-%s.dylib",
#else
					" %s/ga-hook-%s.so",
#endif
					launch_dir, hook_audio);
				cmd += env;
			}
			cmd += "\" ";
			// env: GA_ROOT
			snprintf(env, sizeof(env), "GA_ROOT=%s", launch_dir);
			cmd += env;
			cmd += " ";
			// env: GA_CONFIG
			snprintf(env, sizeof(env), "GA_CONFIG=%s", config_path);
			cmd += env;
			cmd += " ";
			//
			app_arg[0] = app_exe;
			cmd += app_exe;
			//
			for(i = 1; app_arg[i] != NULL; i++) {
				cmd += " \"";
				cmd += app_arg[i];
				cmd += "\"";
			}
			ga_error("CMD: %s\n", cmd.c_str());
			fprintf(stderr, "===================================================\n\n");
			system(cmd.c_str());
			exit(0);
		}
	} while(0);

	return 0;
}

