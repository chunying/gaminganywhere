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

#include <stdarg.h>
#include <string.h>

#include <pthread.h>
#ifdef GA_EMCC
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#else
#include <SDL2/SDL.h>
#ifndef ANDROID
#include <SDL2/SDL_ttf.h>
#endif /* ! ANDROID */
#endif /* GA_EMCC */
#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif /* ! WIN32 */
#if ! defined WIN32 && ! defined __APPLE__ && ! defined ANDROID
#include <X11/Xlib.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#ifdef __cplusplus
}
#endif

#include "rtspconf.h"
#include "rtspclient.h"

#include "controller.h"
#include "ctrl-sdl.h"

#include "ga-common.h"
#include "ga-conf.h"
#include "ga-avcodec.h"

#define	POOLSIZE	16

#define	IDLE_MAXIMUM_THRESHOLD		3600000	/* us */
#define	IDLE_DETECTION_THRESHOLD	 600000 /* us */

pthread_mutex_t watchdogMutex;
struct timeval watchdogTimer = {0LL, 0LL};

static RTSPThreadParam rtspThreadParam;

static int relativeMouseMode = 0;
static int showCursor = 1;
static int windowSizeX[IMAGE_SOURCE_CHANNEL_MAX];
static int windowSizeY[IMAGE_SOURCE_CHANNEL_MAX];

#ifndef ANDROID
#define	DEFAULT_FONT		"FreeSans.ttf"
#define	DEFAULT_FONTSIZE	24
static TTF_Font *defFont = NULL;
#endif

static void
create_overlay(struct RTSPThreadParam *rtspParam, int ch) {
	int w, h;
	PixelFormat format;
#if SDL_VERSION_ATLEAST(2,0,0)
	unsigned int renderer_flags = SDL_RENDERER_SOFTWARE;
	SDL_Window *surface = NULL;
	SDL_Renderer *renderer = NULL;
	SDL_Texture *overlay = NULL;
#else
	SDL_Surface *surface = NULL;
	SDL_Overlay *overlay = NULL;
#endif
	struct SwsContext *swsctx = NULL;
	pipeline *pipe = NULL;
	struct pooldata *data = NULL;
	char windowTitle[64];
	//
	pthread_mutex_lock(&rtspParam->surfaceMutex[ch]);
	if(rtspParam->surface[ch] != NULL) {
		pthread_mutex_unlock(&rtspParam->surfaceMutex[ch]);
		rtsperror("ga-client: duplicated create window request - image comes too fast?\n");
		return;
	}
	w = rtspParam->width[ch];
	h = rtspParam->height[ch];
	format = rtspParam->format[ch];
	pthread_mutex_unlock(&rtspParam->surfaceMutex[ch]);
	// swsctx
	if((swsctx = sws_getContext(w, h, format, w, h, PIX_FMT_YUV420P,
			SWS_FAST_BILINEAR, NULL, NULL, NULL)) == NULL) {
		rtsperror("ga-client: cannot create swsscale context.\n");
		exit(-1);
	}
	// pipeline
	if((pipe = new pipeline()) == NULL) {
		rtsperror("ga-client: cannot create pipeline.\n");
		exit(-1);
	}
	if((data = pipe->datapool_init(POOLSIZE, sizeof(AVPicture))) == NULL) {
		rtsperror("ga-client: cannot allocate data pool.\n");
		exit(-1);
	}
	for(; data != NULL; data = data->next) {
		bzero(data->ptr, sizeof(AVPicture));
		if(avpicture_alloc((AVPicture*) data->ptr, PIX_FMT_YUV420P, w, h) != 0) {
			rtsperror("ga-client: per frame initialization failed.\n");
			exit(-1);
		}
	}
	// sdl
	int wflag = 0;
#if SDL_VERSION_ATLEAST(2,0,0)
#ifdef	ANDROID
	wflag = SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS;
#else
	if(ga_conf_readbool("fullscreen", 0) != 0) {
		wflag |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS;
	}
#endif
	snprintf(windowTitle, sizeof(windowTitle), "Player Channel #%d", ch);
	surface = SDL_CreateWindow(windowTitle,
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			w, h, wflag);
	windowSizeX[ch] = w;
	windowSizeY[ch] = h;
#else
	if(ga_conf_readbool("fullscreen", 0) != 0) {
		wflag |= SDL_FULLSCREEN;
	}
	surface = SDL_SetVideoMode(w, h, 32, wflag);
#endif
	if(surface == NULL) {
		rtsperror("ga-client: set video mode (create window) failed.\n");
		exit(-1);
	}
	// move mouse to center
#if SDL_VERSION_ATLEAST(2,0,0)
	SDL_WarpMouseInWindow(surface, w/2, h/2);
#endif
	if(relativeMouseMode != 0) {
		SDL_SetRelativeMouseMode(SDL_TRUE);
		showCursor = 0;
		//SDL_ShowCursor(0);
		ga_error("ga-client: relative mouse mode enabled.\n");
	}
	//
#if SDL_VERSION_ATLEAST(2,0,0)
	do {	// choose SW or HW renderer?
		// XXX: Windows crashed if there is not a HW renderer!
		int i, n = SDL_GetNumRenderDrivers();
		SDL_RendererInfo info;
		for(i = 0; i < n; i++) {
			if(SDL_GetRenderDriverInfo(i, &info) < 0)
				continue;
			rtsperror("ga-client: renderer#%d - %s (%s%s%s%s)\n",
				i, info.name,
				info.flags & SDL_RENDERER_SOFTWARE ? "SW" : "",
				info.flags & SDL_RENDERER_ACCELERATED? "HW" : "",
				info.flags & SDL_RENDERER_PRESENTVSYNC ? ",vsync" : "",
				info.flags & SDL_RENDERER_TARGETTEXTURE ? ",texture" : "");
			if(info.flags & SDL_RENDERER_ACCELERATED)
				renderer_flags = SDL_RENDERER_ACCELERATED;
		}
	} while(0);
	//
	renderer = SDL_CreateRenderer(surface, -1,
			rtspconf->video_renderer_software ?
				SDL_RENDERER_SOFTWARE : renderer_flags);
	if(renderer == NULL) {
		rtsperror("ga-client: create renderer failed.\n");
		exit(-1);
	}
	//
	overlay = SDL_CreateTexture(renderer,
			SDL_PIXELFORMAT_YV12,
			SDL_TEXTUREACCESS_STREAMING,
			w, h);
#else
	overlay = SDL_CreateYUVOverlay(w, h, SDL_YV12_OVERLAY, surface);
#endif
	if(overlay == NULL) {
		rtsperror("ga-client: create overlay (textuer) failed.\n");
		exit(-1);
	}
	//
	pthread_mutex_lock(&rtspParam->surfaceMutex[ch]);
	rtspParam->pipe[ch] = pipe;
	rtspParam->swsctx[ch] = swsctx;
	rtspParam->overlay[ch] = overlay;
#if SDL_VERSION_ATLEAST(2,0,0)
	rtspParam->renderer[ch] = renderer;
	rtspParam->windowId[ch] = SDL_GetWindowID(surface);
#endif
	rtspParam->surface[ch] = surface;
	pthread_mutex_unlock(&rtspParam->surfaceMutex[ch]);
	//
	rtsperror("ga-client: window created successfully (%dx%d).\n", w, h);
	// initialize watchdog
	pthread_mutex_lock(&watchdogMutex);
	gettimeofday(&watchdogTimer, NULL);
	pthread_mutex_unlock(&watchdogMutex);
	//
	return;
}

static void
open_audio(struct RTSPThreadParam *rtspParam, AVCodecContext *adecoder) {
	SDL_AudioSpec wanted, spec;
	//
	wanted.freq = rtspconf->audio_samplerate;
	wanted.format = -1;
	if(rtspconf->audio_device_format == AV_SAMPLE_FMT_S16) {
		wanted.format = AUDIO_S16SYS;
	} else {
		rtsperror("ga-client: open audio - unsupported audio device format.\n");
		return;
	}
	wanted.channels = rtspconf->audio_channels;
	wanted.silence = 0;
	wanted.samples = SDL_AUDIO_BUFFER_SIZE;
	wanted.callback = audio_buffer_fill_sdl;
	wanted.userdata = adecoder;
	//
	pthread_mutex_lock(&rtspParam->audioMutex);
	if(rtspParam->audioOpened == true) {
		pthread_mutex_unlock(&rtspParam->audioMutex);
		return;
	}
	if(SDL_OpenAudio(&wanted, &spec) < 0) {
		pthread_mutex_unlock(&rtspParam->audioMutex);
		rtsperror("ga-client: open audio failed - %s\n", SDL_GetError());
		return;
	}
	//
	rtspParam->audioOpened = true;
	//
	SDL_PauseAudio(0);
	pthread_mutex_unlock(&rtspParam->audioMutex);
	rtsperror("ga-client: audio device opened.\n");
	return;
}

// negative x or y means centering-x and centering-y, respectively
static void
render_text(SDL_Renderer *renderer, SDL_Window *window, int x, int y, int line, const char *text) {
#ifdef ANDROID
	// not supported
#else
	SDL_Color textColor = {255, 255, 255};
	SDL_Surface *textSurface = TTF_RenderText_Solid(defFont, text, textColor);
	SDL_Rect dest = {0, 0, 0, 0}, boxRect;
	SDL_Texture *texture;
	int ww, wh;
	//
	if(window == NULL || renderer == NULL) {
		rtsperror("render_text: Invalid window(%p) or renderer(%p) received.\n",
			window, renderer);
		return;
	}
	//
	SDL_GetWindowSize(window, &ww, &wh);
	// centering X/Y?
	if(x >= 0) {	dest.x = x; }
	else {		dest.x = (ww - textSurface->w)/2; }
	if(y >= 0) {	dest.y = y; }
	else {		dest.y = (wh - textSurface->h)/2; }
	//
	dest.y += line * textSurface->h;
	dest.w = textSurface->w;
	dest.h = textSurface->h;
	//
	boxRect.x = dest.x - 6;
	boxRect.y = dest.y - 6;
	boxRect.w = dest.w + 12;
	boxRect.h = dest.h + 12;
	//
	if((texture = SDL_CreateTextureFromSurface(renderer, textSurface)) != NULL) {
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_RenderFillRect(renderer, &boxRect);
		SDL_RenderCopy(renderer, texture, NULL, &dest);
		SDL_DestroyTexture(texture);
	} else {
		rtsperror("render_text: failed on creating text texture: %s\n", SDL_GetError());
	}
	//
	SDL_FreeSurface(textSurface);
#endif
	return;
}

#if 1
static void
render_image(struct RTSPThreadParam *rtspParam, int ch) {
	struct pooldata *data;
	AVPicture *vframe;
	SDL_Rect rect;
#if SDL_VERSION_ATLEAST(2,0,0)
	unsigned char *pixels;
	int pitch;
#endif
	//
	if((data = rtspParam->pipe[ch]->load_data()) == NULL) {
		return;
	}
	vframe = (AVPicture*) data->ptr;
	//
#if SDL_VERSION_ATLEAST(2,0,0)
	if(SDL_LockTexture(rtspParam->overlay[ch], NULL, (void**) &pixels, &pitch) == 0) {
		bcopy(vframe->data[0], pixels, rtspParam->width[ch] * rtspParam->height[ch]);
		bcopy(vframe->data[1], pixels+((pitch*rtspParam->height[ch]*5)>>2), rtspParam->width[ch] * rtspParam->height[ch] / 4);
		bcopy(vframe->data[2], pixels+pitch*rtspParam->height[ch], rtspParam->width[ch] * rtspParam->height[ch] / 4);
		SDL_UnlockTexture(rtspParam->overlay[ch]);
	} else {
		rtsperror("ga-client: lock textture failed - %s\n", SDL_GetError());
	}
#else
	bcopy(vframe->data[0], rtspParam->overlay[ch]->pixels[0], rtspParam->width[ch] * rtspParam->height[ch]);
	bcopy(vframe->data[1], rtspParam->overlay[ch]->pixels[2], rtspParam->width[ch] * rtspParam->height[ch] / 4);
	bcopy(vframe->data[2], rtspParam->overlay[ch]->pixels[1], rtspParam->width[ch] * rtspParam->height[ch] / 4);
	SDL_LockYUVOverlay(rtspParam->overlay[ch]);
	SDL_UnlockYUVOverlay(rtspParam->overlay[ch]);
#endif
	rtspParam->pipe[ch]->release_data(data);
	rect.x = 0;
	rect.y = 0;
	rect.w = rtspParam->width[ch];
	rect.h = rtspParam->height[ch];
#if SDL_VERSION_ATLEAST(2,0,0)
	SDL_RenderCopy(rtspParam->renderer[ch], rtspParam->overlay[ch], NULL, NULL);
	SDL_RenderPresent(rtspParam->renderer[ch]);
#else
	SDL_DisplayYUVOverlay(rtspParam->overlay[ch], &rect);
#endif
	//
	image_rendered = 1;
	//
	return;
}
#endif

void
ProcessEvent(SDL_Event *event) {
	struct sdlmsg m;
	switch(event->type) {
	case SDL_KEYUP:
		if(event->key.keysym.sym == SDLK_BACKQUOTE
		&& relativeMouseMode != 0) {
			showCursor = 1 - showCursor;
			//SDL_ShowCursor(showCursor);
#if 1
			if(showCursor)
				SDL_SetRelativeMouseMode(SDL_FALSE);
			else
				SDL_SetRelativeMouseMode(SDL_TRUE);
#endif
		}
		//
		if(rtspconf->ctrlenable) {
		sdlmsg_keyboard(&m, 0,
			event->key.keysym.scancode,
			event->key.keysym.sym,
			event->key.keysym.mod,
			0/*event->key.keysym.unicode*/);
		ctrl_client_sendmsg(&m, sizeof(m));
		}
		break;
	case SDL_KEYDOWN:
		if(rtspconf->ctrlenable) {
		sdlmsg_keyboard(&m, 1,
			event->key.keysym.scancode,
			event->key.keysym.sym,
			event->key.keysym.mod,
			0/*event->key.keysym.unicode*/);
		ctrl_client_sendmsg(&m, sizeof(m));
		}
		break;
	case SDL_MOUSEBUTTONUP:
		if(rtspconf->ctrlenable) {
		sdlmsg_mousekey(&m, 0, event->button.button, event->button.x, event->button.y);
		ctrl_client_sendmsg(&m, sizeof(m));
		}
		break;
	case SDL_MOUSEBUTTONDOWN:
		if(rtspconf->ctrlenable) {
		sdlmsg_mousekey(&m, 1, event->button.button, event->button.x, event->button.y);
		ctrl_client_sendmsg(&m, sizeof(m));
		}
		break;
	case SDL_MOUSEMOTION:
		if(rtspconf->ctrlenable && rtspconf->sendmousemotion) {
			sdlmsg_mousemotion(&m,
				event->motion.x,
				event->motion.y,
				event->motion.xrel,
				event->motion.yrel,
				event->motion.state,
				relativeMouseMode == 0 ? 0 : 1);
			ctrl_client_sendmsg(&m, sizeof(m));
		}
		break;
#if SDL_VERSION_ATLEAST(2,0,0)
	case SDL_MOUSEWHEEL:
		if(rtspconf->ctrlenable && rtspconf->sendmousemotion) {
			sdlmsg_mousewheel(&m, event->motion.x, event->motion.y);
			ctrl_client_sendmsg(&m, sizeof(m));
		}
		break;
#ifdef ANDROID
#define	DEBUG_FINGER(etf)	\
	rtsperror("XXX DEBUG: finger-event(%d) - x=%d y=%d dx=%d dy=%d p=%d\n",\
		(etf).type, (etf).x, (etf).y, (etf).dx, (etf).dy, (etf).pressure);
	case SDL_FINGERDOWN:
		// window size has not been registered
		if(windowSizeX[0] == 0)
			break;
		//DEBUG_FINGER(event->tfinger);
		if(rtspconf->ctrlenable) {
		unsigned short mapx, mapy;
		mapx = (unsigned short) (1.0 * (windowSizeX[0]-1) * event->tfinger.x / 32767.0);
		mapy = (unsigned short) (1.0 * (windowSizeY[0]-1) * event->tfinger.y / 32767.0);
		sdlmsg_mousemotion(&m, mapx, mapy, 0, 0, 0, 0);
		ctrl_client_sendmsg(&m, sizeof(m));
		//
		sdlmsg_mousekey(&m, 1, SDL_BUTTON_LEFT, mapx, mapy);
		ctrl_client_sendmsg(&m, sizeof(m));
		}
		break;
	case SDL_FINGERUP:
		// window size has not been registered
		if(windowSizeX[0] == 0)
			break;
		//DEBUG_FINGER(event->tfinger);
		if(rtspconf->ctrlenable) {
		unsigned short mapx, mapy;
		mapx = (unsigned short) (1.0 * (windowSizeX[0]-1) * event->tfinger.x / 32767.0);
		mapy = (unsigned short) (1.0 * (windowSizeY[0]-1) * event->tfinger.y / 32767.0);
		sdlmsg_mousemotion(&m, mapx, mapy, 0, 0, 0, 0);
		ctrl_client_sendmsg(&m, sizeof(m));
		//
		sdlmsg_mousekey(&m, 0, SDL_BUTTON_LEFT, mapx, mapy);
		ctrl_client_sendmsg(&m, sizeof(m));
		}
		break;
	case SDL_FINGERMOTION:
		// window size has not been registered
		if(windowSizeX[0] == 0)
			break;
		//DEBUG_FINGER(event->tfinger);
		if(rtspconf->ctrlenable) {
		unsigned short mapx, mapy;
		mapx = (unsigned short) (1.0 * (windowSizeX[0]-1) * event->tfinger.x / 32767.0);
		mapy = (unsigned short) (1.0 * (windowSizeY[0]-1) * event->tfinger.y / 32767.0);
		sdlmsg_mousemotion(&m, mapx, mapy, 0, 0, 0, 0);
		ctrl_client_sendmsg(&m, sizeof(m));
		}
		break;
#undef	DEBUG_FINGER
#endif	/* ANDROID */
	case SDL_WINDOWEVENT:
		if(event->window.event == SDL_WINDOWEVENT_CLOSE) {
			rtspThreadParam.running = false;
			return;
		} else if(event->window.event == SDL_WINDOWEVENT_RESIZED) {
			rtsperror("event video resize w=%d h=%d\n",
				event->window.data1, event->window.data2);
		}
		break;
	case SDL_USEREVENT:
		if(event->user.code == SDL_USEREVENT_RENDER_IMAGE) {
			long long ch = (long long) event->user.data2;
			render_image((struct RTSPThreadParam*) event->user.data1, (int) ch & 0x0ffffffff);
			break;
		}
		if(event->user.code == SDL_USEREVENT_CREATE_OVERLAY) {
			long long ch = (long long) event->user.data2;
			create_overlay((struct RTSPThreadParam*) event->user.data1, (int) ch & 0x0ffffffff);
			break;
		}
		if(event->user.code == SDL_USEREVENT_OPEN_AUDIO) {
			open_audio(
				(struct RTSPThreadParam*) event->user.data1,
				(AVCodecContext*) event->user.data2);
			break;
		}
		if(event->user.code == SDL_USEREVENT_RENDER_TEXT) {
			//SDL_SetAlpha()
			SDL_SetRenderDrawColor(rtspThreadParam.renderer[0], 0, 0, 0, 192/*SDL_ALPHA_OPAQUE/2*/);
			//SDL_RenderFillRect(rtspThreadParam.renderer[0], NULL);
			render_text(rtspThreadParam.renderer[0],
				rtspThreadParam.surface[0],
				-1, -1, 0, (const char *) event->user.data1);
			SDL_RenderPresent(rtspThreadParam.renderer[0]);
			break;
		}
		break;
#else /* DL_VERSION_ATLEAST(2,0,0) */
	case SDL_VIDEORESIZE:
		rtsperror("event video resize w=%d h=%d\n",
			event->resize.w, event->resize.h);
		break;
#endif /* DL_VERSION_ATLEAST(2,0,0) */
	case SDL_QUIT:
		rtspThreadParam.running = false;
		return;
	default:
		// do nothing
		break;
	}
	return;
}

static void *
watchdog_thread(void *args) {
	static char idlemsg[128];
	struct timeval tv;
	SDL_Event evt;
	//
	rtsperror("watchdog: launched, waiting for audio/video frames ...\n");
	//
	while(true) {
#ifdef WIN32
		Sleep(1000);
#else
		sleep(1);
#endif
		pthread_mutex_lock(&watchdogMutex);
		gettimeofday(&tv, NULL);
		if(watchdogTimer.tv_sec != 0) {
			long long d;
			d = tvdiff_us(&tv, &watchdogTimer);
			if(d > IDLE_MAXIMUM_THRESHOLD) {
				rtspThreadParam.running = false;
				break;
			} else if(d > IDLE_DETECTION_THRESHOLD) {
				// update message and show
				snprintf(idlemsg, sizeof(idlemsg),
					"Audio/video stall detected, waiting for %d second(s) to terminate ...",
					(int) (IDLE_MAXIMUM_THRESHOLD - d) / 1000000);
				//
				bzero(&evt, sizeof(evt));
				evt.user.type = SDL_USEREVENT;
#if SDL_VERSION_ATLEAST(2,0,0)
				evt.user.timestamp = time(0);
#endif
				evt.user.code = SDL_USEREVENT_RENDER_TEXT;
				evt.user.data1 = idlemsg;
				evt.user.data2 = NULL;
				SDL_PushEvent(&evt);
				//
				rtsperror("watchdog: %s\n", idlemsg);
			} else {
				// do nothing
			}
		} else {
			rtsperror("watchdog: initialized, but no frames received ...\n");
		}
		pthread_mutex_unlock(&watchdogMutex);
	}
	//
	rtsperror("watchdog: terminated.\n");
	exit(-1);
	//
	return NULL;
}

int
main(int argc, char *argv[]) {
	int i;
	SDL_Event event;
	pthread_t rtspthread;
	pthread_t ctrlthread;
	pthread_t watchdog;
	//
#ifdef ANDROID
	if(ga_init("/sdcard/ga/android.conf", NULL) < 0) {
		rtsperror("cannot load configuration file '%s'\n", argv[1]);
		return -1;
	}
#else
	if(argc < 3) {
		rtsperror("usage: %s config url\n", argv[0]);
		return -1;
	}
	//
	if(ga_init(argv[1], argv[2]) < 0) {
		rtsperror("cannot load configuration file '%s'\n", argv[1]);
		return -1;
	}
#endif
	//
	if(ga_conf_readbool("control-relative-mouse-mode", 0) != 0) {
		rtsperror("*** Relative mouse mode enabled.\n");
		relativeMouseMode = 1;
	}
	//
	rtspconf = rtspconf_global();
	if(rtspconf_parse(rtspconf) < 0) {
		rtsperror("parse configuration failed.\n");
		return -1;
	}
	//
#if ! defined WIN32 && ! defined __APPLE__ && ! defined ANDROID
	if(XInitThreads() == 0) {
		rtsperror("XInitThreads() failed, client terminated.\n");
		return -1;
	}
#endif
#ifndef ANDROID
	// init fonts
	if(TTF_Init() != 0) {
		rtsperror("cannot initialize SDL_ttf: %s\n", SDL_GetError());
		return -1;
	}
	if((defFont = TTF_OpenFont(DEFAULT_FONT, DEFAULT_FONTSIZE)) == NULL) {
		rtsperror("open font '%s' failed: %s\n",
			DEFAULT_FONT, SDL_GetError());
		return -1;
	}
#endif
	//
	rtspconf_resolve_server(rtspconf, rtspconf->servername);
	rtsperror("Remote server @ %s[%s]:%d\n",
		rtspconf->servername,
		inet_ntoa(rtspconf->sin.sin_addr),
		rtspconf->serverport);
	//
	if(SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		rtsperror("SDL init failed: %s\n", SDL_GetError());
		return -1;
	}
#if ! SDL_VERSION_ATLEAST(2,0,0)
	// enable keyboard repeat?
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
#endif
	// launch controller?
	do if(rtspconf->ctrlenable) {
		if(ctrl_queue_init(32768, sizeof(struct sdlmsg)) < 0) {
			rtsperror("Cannot initialize controller queue, controller disabled.\n");
			rtspconf->ctrlenable = 0;
			break;
		}
		if(pthread_create(&ctrlthread, NULL, ctrl_client_thread, rtspconf) != 0) {
			rtsperror("Cannot create controller thread, controller disabled.\n");
			rtspconf->ctrlenable = 0;
			break;
		}
		pthread_detach(ctrlthread);
	} while(0);
	// launch watchdog
	pthread_mutex_init(&watchdogMutex, NULL);
	if(ga_conf_readbool("enable-watchdog", 1) == 1) {
		if(pthread_create(&watchdog, NULL, watchdog_thread, NULL) != 0) {
			rtsperror("Cannot create watchdog thread.\n");
			return -1;
		}
		pthread_detach(watchdog);
	} else {
		ga_error("watchdog disabled.\n");
	}
	//
	bzero(&rtspThreadParam, sizeof(rtspThreadParam));
	for(i = 0; i < IMAGE_SOURCE_CHANNEL_MAX; i++) {
		pthread_mutex_init(&rtspThreadParam.surfaceMutex[i], NULL);
	}
	pthread_mutex_init(&rtspThreadParam.audioMutex, NULL);
	rtspThreadParam.url = strdup(argv[2]);
	rtspThreadParam.running = true;
	if(pthread_create(&rtspthread, NULL, rtsp_thread, &rtspThreadParam) != 0) {
		rtsperror("Cannot create rtsp client thread.\n");
		return -1;
	}
	pthread_detach(rtspthread);
	//
	while(rtspThreadParam.running) {
		if(SDL_WaitEvent(&event)) {
			ProcessEvent(&event);
		}
	}
	//
	rtspThreadParam.quitLive555 = 1;
	rtsperror("terminating ...\n");
	//
#ifndef ANDROID
	pthread_cancel(rtspthread);
	if(rtspconf->ctrlenable)
		pthread_cancel(ctrlthread);
	pthread_cancel(watchdog);
#endif
	//SDL_WaitThread(thread, &status);
	//
	SDL_Quit();
	ga_deinit();
	exit(0);
	//
	return 0;
}

