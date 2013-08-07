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

#include "ga-common.h"
#include "ga-win32-dfmirage.h"
#include "ga-win32-dfm.h"

#include <string>
#include <algorithm>
#include <cctype>

using namespace std;

#define	BITSPERPIXEL	32

static BITMAPINFO	bmpInfo;
static int		frameSize;
// DFM structures
static const char MINIPORT_REGISTRY_PATH[] = 
	"SYSTEM\\CurrentControlSet\\Hardware Profiles\\"
  	"Current\\System\\CurrentControlSet\\Services";
static const int EXT_DEVMODE_SIZE_MAX = 3072;
struct DFEXT_DEVMODE : DEVMODE {
	char extension[EXT_DEVMODE_SIZE_MAX];
};
//
static OSVERSIONINFO	osVersion;
static bool		m_isDriverOpened;
static bool		m_isDriverLoaded;
static bool		m_isDriverAttached;
static bool		m_isDriverConnected;
static DWORD		m_deviceNumber;
static DISPLAY_DEVICE	m_deviceInfo;
static DFEXT_DEVMODE	m_deviceMode;
static HDC		m_driverDC;
static CHANGES_BUF *	m_changesBuffer;
static void *		m_screenBuffer;
static HKEY		m_regkeyDevice;

static struct gaImage	gaimage;

static int
registrykey_open(HKEY root, const char *entry, bool createIfNotExist, HKEY *openedKey) {
	string myentry = entry;
	//
	if(entry == NULL || myentry.size() <= 0) {
		return -1;
	}
	if(myentry[myentry.size()-1] != '\\') {
		myentry += "\\";
	}
	if(RegOpenKey(root, myentry.c_str(), openedKey) != ERROR_SUCCESS) {
		if(createIfNotExist) {
			if(RegCreateKey(root, myentry.c_str(), openedKey) != ERROR_SUCCESS) {
				return 0;
			}
		} else {
			return -1;
		}
	}
	return 0;
}

static int
dfm_extractDeviceInfo(char *driverName) {
	BOOL result;
	//
	ZeroMemory(&m_deviceInfo, sizeof(m_deviceInfo));
	m_deviceInfo.cb = sizeof(m_deviceInfo);
	//
	//fprintf(stderr, "Searching for %s ...\n", driverName);
	//
	m_deviceNumber = 0;
	while (result = EnumDisplayDevices(0, m_deviceNumber, &m_deviceInfo, 0)) {
		string deviceString;
		//fprintf(stderr, "Found: %s\n", m_deviceInfo.DeviceString);
		//fprintf(stderr, "RegKey: %s\n", m_deviceInfo.DeviceKey);
		deviceString = m_deviceInfo.DeviceString;
		if (deviceString == driverName) {
			//fprintf(stderr, "%s is found\n", driverName);
			break;
		}
		m_deviceNumber++;
	}
	if (!result) {
		fprintf(stderr, "Cannot find %s.\n", driverName);
		return -1;
	}
	return 0;
}

static int
dfm_openDeviceRegKey(char *miniportName) {
	int pos;
	string deviceKey = m_deviceInfo.DeviceKey;
	string subKey = "DEVICE0";
	// deviceKey to uppercase
	transform(deviceKey.begin(), deviceKey.end(), deviceKey.begin(), std::toupper);
	if((pos = deviceKey.find("\\DEVICE")) != string::npos) {
		if(deviceKey.substr(pos).size() >= 8) {
			//str.getSubstring(&subKey, 1, 7);
			subKey = deviceKey.substr(pos+1, pos+7);
		}
	}
	//
	//fprintf(stderr, "Opening registry key '%s\\%s\\%s' ...\n",
        //	MINIPORT_REGISTRY_PATH,
        //	miniportName,
        //	subKey.c_str());
	// open registry keys
	do {
		HKEY regkeyService;
		HKEY regkeyDriver;
		if(registrykey_open(HKEY_LOCAL_MACHINE, MINIPORT_REGISTRY_PATH, true, &regkeyService) < 0) {
			fprintf(stderr, "Cannot open registry key '%s'.\n",
				MINIPORT_REGISTRY_PATH);
			return -1;
		}
		if(registrykey_open(regkeyService, miniportName, true, &regkeyDriver) < 0) {
			RegCloseKey(regkeyService);
			fprintf(stderr, "Cannot open registry key '%s\\%s'\n",
				MINIPORT_REGISTRY_PATH, miniportName);
			return -1;
		}
		if(registrykey_open(regkeyDriver, subKey.c_str(), true, &m_regkeyDevice) < 0) {
			RegCloseKey(regkeyDriver);
			RegCloseKey(regkeyService);
			fprintf(stderr, "Cannot open registry key '%s\\%s\\%s'\n",
				MINIPORT_REGISTRY_PATH, miniportName, subKey.c_str());
			return -1;
		}
		RegCloseKey(regkeyDriver);
		RegCloseKey(regkeyService);
	} while(0);
	//
	return 0;
}

static int
dfm_open() {
	if(m_isDriverOpened)
		return 0;
	if(dfm_extractDeviceInfo("Mirage Driver") < 0)
		return 0;
	if(dfm_openDeviceRegKey("dfmirage") < 0)
		return 0;
	m_isDriverOpened = true;
	return 0;
}

static int
dfm_close() {
	if(m_isDriverOpened) {
		RegCloseKey(m_regkeyDevice);
		m_isDriverOpened = false;
		//fprintf(stderr, "Mirror driver closed.\n");
	}
	return 0;
}

static int
dfm_setAttachToDesktop(bool attach) {
	int value = (int) attach;
	if(m_regkeyDevice == 0) {
		fprintf(stderr, "Registry key has not been opened.\n");
		return -1;
	}
	if(RegSetValueEx(m_regkeyDevice, "Attach.ToDesktop", 0, REG_DWORD, (BYTE*) &value, sizeof(value)) != ERROR_SUCCESS) {
		fprintf(stderr, "Registry key set value failed.\n");
		return -1;
	}

	m_isDriverAttached = attach;

	return 0;
}

static int
dfm_commitDisplayChanges(DEVMODE *pdm) {
	//fprintf(stderr, "change display mode for '%s'\n", m_deviceInfo.DeviceName);
	if(pdm != NULL) {
		LONG code = ChangeDisplaySettingsEx(m_deviceInfo.DeviceName, pdm, 0, CDS_UPDATEREGISTRY, 0);
		if (code < 0) {
			fprintf(stderr, "change display mode failed with code %d\n", code);
			return -1;
		}
		//m_log->info(_T("CommitDisplayChanges(2): \"%s\""), m_deviceInfo.DeviceName);
		code = ChangeDisplaySettingsEx(m_deviceInfo.DeviceName, pdm, 0, 0, 0);
		if (code < 0) {
			fprintf(stderr, "2nd change display mode failed with code %d\n", code);
			return -1;
		}
	} else {
		LONG code = ChangeDisplaySettingsEx(m_deviceInfo.DeviceName, 0, 0, 0, 0);
		if (code < 0) {
			fprintf(stderr, "change display mode w/o devmode failed with code %d\n", code);
			return -1;
		}
	}
	//fprintf(stderr, "change display mode successfully.\n");
	return 0;
}

static int
dfm_load() {
	WORD drvExtraSaved;
	//
	if(m_isDriverOpened == false) {
		return -1;
	}
	if(m_isDriverLoaded) {
		return 0;
	}
	//
	//fprintf(stderr, "Loading driver ...\n");
	//
	drvExtraSaved = m_deviceMode.dmDriverExtra;
	// IMPORTANT: we dont touch extension data and size
	ZeroMemory(&m_deviceMode, sizeof(DEVMODE));
	m_deviceMode.dmSize = sizeof(DEVMODE);
	m_deviceMode.dmDriverExtra = drvExtraSaved;

	m_deviceMode.dmPelsWidth = bmpInfo.bmiHeader.biWidth;
	m_deviceMode.dmPelsHeight = bmpInfo.bmiHeader.biHeight;
	m_deviceMode.dmBitsPerPel = bmpInfo.bmiHeader.biBitCount;
	m_deviceMode.dmPosition.x = 0;
	m_deviceMode.dmPosition.y = 0;

	m_deviceMode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH |
				DM_PELSHEIGHT | DM_POSITION;
	m_deviceMode.dmDeviceName[0] = '\0';

	if(dfm_setAttachToDesktop(true) < 0) {
		return -1;
	}
	if(dfm_commitDisplayChanges(&m_deviceMode) < 0) {
		return -1;
	}

	if((m_driverDC = CreateDC(m_deviceInfo.DeviceName, 0, 0, 0)) == 0) {
		fprintf(stderr, "Cannot create device context for mirror driver.\n");
		return -1;
	}

	//fprintf(stderr, "Mirror driver loaded successfully.\n");
	m_isDriverLoaded = true;

	return 0;
}

static int
dfm_unload() {
	if(m_driverDC != 0) {
		DeleteDC(m_driverDC);
		m_driverDC = 0;
	}

	if (m_isDriverAttached) {
		//fprintf(stderr, "Unloading mirror driver...\n");
		dfm_setAttachToDesktop(false);

		m_deviceMode.dmPelsWidth = 0;
		m_deviceMode.dmPelsHeight = 0;

		// IMPORTANT: Windows 2000 fails to unload the driver
		// if the mode passed to ChangeDisplaySettingsEx() contains DM_POSITION set.
		DEVMODE *pdm = NULL;
		if(osVersion.dwMajorVersion == 5
		&& osVersion.dwMinorVersion == 0 /* Windows 2000*/) {
			pdm = &m_deviceMode;
		}

		if(dfm_commitDisplayChanges(pdm) < 0) {
			fprintf(stderr, "Failed to unload the mirror driver.\n");
			return -1;
		}
		//fprintf(stderr, "Mirror driver is unloaded.\n");
	}

	// NOTE: extension data and size is also reset
	ZeroMemory(&m_deviceMode, sizeof(m_deviceMode));
	m_deviceMode.dmSize = sizeof(DEVMODE);

	m_isDriverLoaded = false;

	return 0;
}

static int
dfm_connect() {
	int ret;
	GETCHANGESBUF buf = {0};

	if(m_isDriverConnected)
		return 0;

	ret = ExtEscape(m_driverDC, dmf_esc_usm_pipe_map, 0, 0, sizeof(buf), (LPSTR)&buf);
	if(ret <= 0) {
		fprintf(stderr, "Cannot connect to the mirror driver: ret = %d\n", ret);
		return -1;
	}

	m_changesBuffer = buf.buffer;
	m_screenBuffer = buf.Userbuffer;

	m_isDriverConnected = true;
	//fprintf(stderr, "Connected to the mirror driver.\n");

	return 0;
}

static int
dfm_disconnect() {
	int ret;
	GETCHANGESBUF buf;

	if(m_isDriverConnected == false)
		return 0;

	buf.buffer = m_changesBuffer;
	buf.Userbuffer = m_screenBuffer;

	ret = ExtEscape(m_driverDC, dmf_esc_usm_pipe_unmap, sizeof(buf), (LPSTR)&buf, 0, 0);
	if(ret <= 0) {
		fprintf(stderr, "Cannot unmap buffer: ret = %d\n", ret);
	}

	m_isDriverConnected = false;
	return 0;
}

int
ga_win32_DFM_init(struct gaImage *image) {
	//
	ZeroMemory(&osVersion, sizeof(osVersion));
	if(GetVersionEx(&osVersion) == 0) {
		// failed
		ZeroMemory(&osVersion, sizeof(osVersion));
	}
	//
	ga_win32_fill_bitmap_info(
		&bmpInfo,
		GetSystemMetrics(SM_CXSCREEN),
		GetSystemMetrics(SM_CYSCREEN),
		BITSPERPIXEL);
	frameSize = bmpInfo.bmiHeader.biSizeImage;
	//
	image->width = bmpInfo.bmiHeader.biWidth;
	image->height = bmpInfo.bmiHeader.biHeight;
	image->bytes_per_line = (BITSPERPIXEL>>3) * image->width;
	CopyMemory(&gaimage, image, sizeof(gaimage));
	// DF Mirage driver setup
	if(dfm_open() < 0)	return -1;
	if(dfm_load() < 0)	return -1;
	if(dfm_connect() < 0)	return -1;
	//
	return 0;
}

void
ga_win32_DFM_deinit() {
	if(m_isDriverConnected) {
		dfm_disconnect();
	}
	if(m_isDriverLoaded) {
		dfm_unload();
	}
	if(m_isDriverOpened) {
		dfm_close();
	}
	return;
}

int
ga_win32_DFM_capture(char *buf, int buflen, struct gaRect *grect) {
	if(m_isDriverConnected == false)
		return -1;
	if(grect == NULL) {
		if(buflen < frameSize)
			return -1;
		CopyMemory(buf, m_screenBuffer, frameSize);
	} else {
		int i;
		char *src, *dst;
		if(buflen < grect->size)
			return -1;
		src = (char *) m_screenBuffer;
		src += gaimage.bytes_per_line * grect->top;
		src += RGBA_SIZE * grect->left;
		dst = (char*) buf;
		//
		for(i = 0; i < grect->height; i++) {
			CopyMemory(dst, src, grect->linesize);
			src += gaimage.bytes_per_line;
			dst += grect->linesize;
		}
	}
	return grect==NULL ? frameSize : grect->size;
}

