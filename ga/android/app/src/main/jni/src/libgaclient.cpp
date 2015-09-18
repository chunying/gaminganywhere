/*
 * Copyright (c) 2013-2014 Chun-Ying Huang
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
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#include <netinet/in.h>
// GL
#include <GLES/gl.h>
#include <GLES/glext.h>

#include "ga-common.h"
#include "ga-conf.h"
#include "vconverter.h"
#include "libgaclient.h"
#include "rtspconf.h"
#include "rtspclient.h"
#include "controller.h"
#include "ctrl-sdl.h"

#include <map>
using namespace std;

//#define PRINT_LATENCY	1
#define	POOLSIZE	16

// JNI config
static JavaVM *g_vm = NULL;
static JNIEnv *g_env = NULL;
static jobject g_obj = NULL;
static jclass g_class = NULL;
//
static jmethodID g_mid_setScreenDimension = NULL;
static jmethodID g_mid_showToast = NULL;
static jmethodID g_mid_goBack = NULL;
static jmethodID g_mid_requestRender = NULL;
static jmethodID g_mid_kickWatchdog = NULL;
static jmethodID g_mid_initAudio = NULL;
static jmethodID g_mid_startAudioDecoder = NULL;
static jmethodID g_mid_decodeAudio = NULL;
static jmethodID g_mid_initVideo = NULL;
static jmethodID g_mid_videoSetByteBuffer = NULL;
static jmethodID g_mid_startVideoDecoder = NULL;
static jmethodID g_mid_decodeVideo = NULL;
//
static bool callbackInitialized = false;

// configurations
static struct RTSPConf *g_conf = NULL;
static struct RTSPThreadParam rtspThreadParam;
static pthread_t uithread;
static pthread_t ctrlthread;
static pthread_t rtspthread;

static map<pthread_t,JNIEnv*> threadEnv;

#ifdef PRINT_LATENCY
static int ptv0locked = 0;
static struct timeval ptv0;
#endif

JNIEnv *
attachThread(JNIEnv *jnienv) {
	JNIEnv *env = NULL;
	map<pthread_t,JNIEnv*>::iterator me;
	pthread_t tid = pthread_self();
	//
	if(callbackInitialized == false) {
		ga_log("attachThread: callback not initialied.\n");
		return NULL;
	}
	if(jnienv != NULL) {
		return jnienv;
	}
	if((me = threadEnv.find(tid)) != threadEnv.end()) {
		return me->second;
	}
	//
	switch(g_vm->GetEnv((void**) &env, JNI_VERSION_1_6)) {
	case JNI_EDETACHED:
		ga_log("attachThread: JNI_EDETACHED, attempt to attach\n");
		if(g_vm->AttachCurrentThread(&env, NULL) != 0) {
			ga_log("attachThread: Failed to attach");
			env = NULL;
		}
		break;
	case JNI_EVERSION:
		ga_log("attachThread: GetEnv - version not supported");
		env = NULL;
		break;
	case JNI_OK:
		break;
	default:
		ga_log("attachThread: Unexpected return value from GetEnv");
		env = NULL;
		break;
	}
	//
	do if(env != NULL) {
		pthread_key_t key;	// for the purpose of auto detach
		threadEnv[tid] = env;
		//
		if(pthread_key_create(&key, detachThread) != 0) {
			ga_log("attachThread: cannot create thread key.\n");
			g_vm->DetachCurrentThread();
			env = NULL;
			break;
		}
		if(pthread_setspecific(key, env) != 0) {
			ga_log("attachThread: cannot set thread key.\n");
			g_vm->DetachCurrentThread();
			env = NULL;
			break;
		}
		ga_log("attachThread: success (%p).\n", env);
	} while(false);
	//
	return env;
}

void
detachThread(void *jnienv) {
	pthread_t tid = pthread_self();
	//
	threadEnv.erase(tid);
	//
	g_vm->DetachCurrentThread();
	ga_log("detachThread: success (%p).\n", jnienv);
	return;
}

void
setScreenDimension(JNIEnv *jnienv, int w, int h) {
	JNIEnv *env;
	if((env = attachThread(jnienv)) == NULL)
		return;
	env->CallVoidMethod(g_obj, g_mid_setScreenDimension, (jint) w, (jint) h);
	return;
}

void
showToast(JNIEnv *jnienv, const char *fmt, ...) {
	JNIEnv *env;
	char msg[2048];
	va_list ap;
	jstring jstr;
	if((env = attachThread(jnienv)) == NULL)
		return;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	//
	jstr = env->NewStringUTF(msg);
	env->CallVoidMethod(g_obj, g_mid_showToast, jstr);
	return;
}

void
goBack(JNIEnv *jnienv, jint exitCode) {
	JNIEnv *env;
	if((env = attachThread(jnienv)) == NULL)
		return;
	env->CallVoidMethod(g_obj, g_mid_goBack, exitCode);
	return;
}

void
requestRender(JNIEnv *jnienv) {
	JNIEnv *env;
#ifdef PRINT_LATENCY
	if(ptv0locked == 0) {
		ptv0locked = 1;
		gettimeofday(&ptv0, NULL);
	}
#endif
	if((env = attachThread(jnienv)) == NULL)
		return;
	env->CallVoidMethod(g_obj, g_mid_requestRender);
	return;
}

void
kickWatchdog(JNIEnv *jnienv) {
	JNIEnv *env;
	if((env = attachThread(jnienv)) == NULL)
		return;
	env->CallVoidMethod(g_obj, g_mid_kickWatchdog);
	return;
}

jobject
initAudio(JNIEnv *jnienv, const char *mime, int sampleRate, int channelCount, bool builtinDecoder) {
	JNIEnv *env;
	jobject obj = NULL;
	jstring jmime = NULL;
	if((env = attachThread(jnienv)) == NULL)
		return NULL;
	jmime = env->NewStringUTF(mime);
	obj = env->CallObjectMethod(g_obj, g_mid_initAudio,
			jmime, (jint) sampleRate, (jint) channelCount,
			builtinDecoder ? JNI_TRUE : JNI_FALSE);
	return obj;
}

jobject
startAudioDecoder(JNIEnv *jnienv) {
	JNIEnv *env;
	jobject obj = NULL;
	if((env = attachThread(jnienv)) == NULL)
		return NULL;
	obj = env->CallObjectMethod(g_obj, g_mid_startAudioDecoder);
	return obj;
}

#define	MAX_AUDIOBUF_SIZE	262155	// 256K
static jbyteArray jaudioBuf = NULL;
int
decodeAudio(JNIEnv *jnienv, unsigned char *data, int len, struct timeval pts, int flags) {
	JNIEnv *env;
	jbyteArray jlocalbuf = NULL;
	if((env = attachThread(jnienv)) == NULL)
		return -1;
	if(jaudioBuf == NULL) {
		jlocalbuf = env->NewByteArray(MAX_AUDIOBUF_SIZE);
		jaudioBuf = reinterpret_cast<jbyteArray>(env->NewGlobalRef(jlocalbuf));
	}
	env->SetByteArrayRegion(jaudioBuf,
		(jsize) 0, (jsize) len, (jbyte*) data);
	return env->CallIntMethod(g_obj, g_mid_decodeAudio,
		jaudioBuf, (jint) len,
		(jlong) pts.tv_sec * 1000000 + pts.tv_usec,
		flags);
}

jobject
initVideo(JNIEnv *jnienv, const char *mime, int width, int height) {
	JNIEnv *env;
	jobject obj = NULL;
	jstring jmime = NULL;
	if((env = attachThread(jnienv)) == NULL)
		return NULL;
	jmime = env->NewStringUTF(mime);
	obj = env->CallObjectMethod(g_obj, g_mid_initVideo,
		jmime, (jint) width, (jint) height);
	return obj;
}

jobject
videoSetByteBuffer(JNIEnv *jnienv, const char *name, unsigned char *value, int len) {
	JNIEnv *env;
	jobject obj = NULL;
	jstring jname = NULL;
	jbyteArray jvalue = NULL;
	if((env = attachThread(jnienv)) == NULL)
		return NULL;
	jname = env->NewStringUTF(name);
	jvalue = env->NewByteArray(len);
	env->SetByteArrayRegion(jvalue,
		(jsize) 0/*start*/, (jsize) len/*len*/, (jbyte*) value);
	obj = env->CallObjectMethod(g_obj, g_mid_videoSetByteBuffer,
		jname, jvalue, (jint) len);
	env->DeleteLocalRef(jvalue);
	return obj;
}

jobject
startVideoDecoder(JNIEnv *jnienv) {
	JNIEnv *env;
	jobject obj = NULL;
	if((env = attachThread(jnienv)) == NULL)
		return NULL;
	obj = env->CallObjectMethod(g_obj, g_mid_startVideoDecoder);
	return obj;
}

#define	MAX_VIDEOBUF_SIZE	262155	// 256K
static jbyteArray jvideoBuf = NULL;
int
decodeVideo(JNIEnv *jnienv, unsigned char *data, int len, struct timeval pts, bool marker, int flags) {
	JNIEnv *env;
	jbyteArray jlocalbuf = NULL;
	if((env = attachThread(jnienv)) == NULL)
		return -1;
	if(jvideoBuf == NULL) {
		//jvideoBuf = env->NewByteArray(MAX_VIDEOBUF_SIZE);
		jlocalbuf = env->NewByteArray(MAX_VIDEOBUF_SIZE);
		jvideoBuf = reinterpret_cast<jbyteArray>(env->NewGlobalRef(jlocalbuf));
	}
	env->SetByteArrayRegion(jvideoBuf,
		(jsize) 0, (jsize) len, (jbyte*) data);
	return env->CallIntMethod(g_obj, g_mid_decodeVideo,
		jvideoBuf, (jint) len,
		(jlong) pts.tv_sec * 1000000 + pts.tv_usec,
		(jboolean) marker ? JNI_TRUE : JNI_FALSE,
		flags);
}

// GL
#define	TEXTURE_WIDTH	2048
#define	TEXTURE_HEIGHT	2048

static int gl_width = 0;
static int gl_height = 0;
static int img_width = 0;
static int img_height = 0;
static GLuint img_texture = 0;

void
gl_resize(int width, int height) {
	/* store the actual width of the screen */
	if(width > 0 && height > 0) {
		gl_width = width;
		gl_height = height;
	}
	glEnable(GL_TEXTURE_2D);
	if(img_width <= 0
	|| img_height <= 0
	|| gl_width <= 0
	|| gl_height <= 0) {
		glClearColor(0, 0, 0, 1);
		return;
	}
	//
	glGenTextures(1, &img_texture);
	glBindTexture(GL_TEXTURE_2D, img_texture);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST/*GL_LINEAR*/);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST/*GL_LINEAR*/);
	glShadeModel(GL_FLAT);
	glColor4x(0x10000, 0x10000, 0x10000, 0x10000);
	int rect[4] = {0, img_height, img_width, -img_height};
	glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, rect);
	glTexImage2D(GL_TEXTURE_2D,		/* target */
			0,			/* level */
			GL_RGB,			/* internal format */
			TEXTURE_WIDTH,		/* width */
			TEXTURE_HEIGHT,		/* height */
			0,			/* border */
			GL_RGB,			/* format */
			GL_UNSIGNED_SHORT_5_6_5, /* type */
			NULL);			/* pixels */
	ga_log("GLresize: img [%dx%d]; gl[%dx%d]; texture[%dx%d]\n",
		img_width, img_height, gl_width, gl_height, TEXTURE_WIDTH, TEXTURE_HEIGHT);
	return;
}

int
gl_render() {
	extern int image_rendered;
	dpipe_buffer_t *data = NULL;
	AVPicture *vframe = NULL;
#ifdef PRINT_LATENCY
	struct timeval ptv1;
#endif
	//
	//ga_log("XXX: img=%dx%d; pipeline=0x%p\n",
	//	img_width, img_height, rtspThreadParam.pipe[0]);
	if(img_width <= 0 || img_height <= 0)
		return -1;
	if(rtspThreadParam.pipe[0] == NULL)
		return -1;
	//
	if((data = dpipe_load_nowait(rtspThreadParam.pipe[0])) == NULL)
		return -1;
	vframe = (AVPicture*) data->pointer;
	//
	//glClear(GL_COLOR_BUFFER_BIT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST/*GL_LINEAR*/);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST/*GL_LINEAR*/);
	int rect[4] = {0, img_height, img_width, -img_height};
	glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, rect);
	glTexImage2D(GL_TEXTURE_2D,		/* target */
			0,			/* level */
			GL_RGB,			/* internal format */
			TEXTURE_WIDTH,		/* width */
			TEXTURE_HEIGHT,		/* height */
			0,			/* border */
			GL_RGB,			/* format */
			GL_UNSIGNED_SHORT_5_6_5, /* type */
			NULL);			/* pixels */
	glTexSubImage2D(GL_TEXTURE_2D,		/* target */
			0,			/* level */
			0,			/* xoffset */
			0,			/* yoffset */
			img_width,		/* width */
			img_height,		/* height */
			GL_RGB,			/* format */
			GL_UNSIGNED_SHORT_5_6_5, /* type */
			vframe->data[0]);	/* pixels */
	glDrawTexiOES(0, 0, 0, gl_width, gl_height);
	dpipe_put(rtspThreadParam.pipe[0], data);
	image_rendered = 1;
#ifdef PRINT_LATENCY
	if(ptv0locked != 0) {
		gettimeofday(&ptv1, NULL);
		ga_aggregated_print(0x8005, 619, tvdiff_us(&ptv1, &ptv0));
		ptv0locked = 0;
	}
#endif
	return 0;
}

int
create_overlay(int ch, int w, int h, PixelFormat format) {
	struct SwsContext *swsctx = NULL;
	dpipe_t *pipe = NULL;
	dpipe_buffer_t *data = NULL;
	char pipename[64];
#ifdef PRINT_LATENCY
	ptv0locked = 0;
#endif
	//
	setScreenDimension(rtspThreadParam.jnienv, w, h);
	// XXX: assume surfaceMutex[ch] locked
	//if((swsctx = sws_getContext(w, h, format, w, h, PIX_FMT_RGB565,
	//		SWS_BICUBIC, NULL, NULL, NULL)) == NULL) {
	if((swsctx = create_frame_converter(w, h, format, w, h, PIX_FMT_RGB565)) == NULL) {
		rtsperror("ga-client: cannot create swsscale context.\n");
		rtspThreadParam.quitLive555 = 1;
		return -1;
	}
	// pipeline
	snprintf(pipename, sizeof(pipename), "channel-%d", ch);
	pipe = dpipe_create(ch, pipename, POOLSIZE, sizeof(AVPicture));
	if(pipe == NULL) {
		rtsperror("ga-client: cannot create pipeline.\n");
		rtspThreadParam.quitLive555 = 1;
		return -1;
	}
	for(data = pipe->in; data != NULL; data = data->next) {
		bzero(data->pointer, sizeof(AVPicture));
		if(avpicture_alloc((AVPicture*) data->pointer, PIX_FMT_RGB565, w, h) != 0) {
			rtsperror("ga-client: per frame initialization failed.\n");
			rtspThreadParam.quitLive555 = 1;
			return -1;
		}
	}
	//
	rtspThreadParam.pipe[ch] = pipe;
	rtspThreadParam.swsctx[ch] = swsctx;
	img_width = w;
	img_height = h;
	gl_resize(-1, -1);
	ga_log("overlay created [%dx%d]\n", w, h);
	return 0;
}

static void
initConfig() {
	int i;
	//
	if(g_conf == NULL) {
		g_conf = rtspconf_global();
		bzero(g_conf, sizeof(struct RTSPConf));
	}
	if(g_conf->servername != NULL)
		free(g_conf->servername);
	for(i = 0; i < RTSPCONF_CODECNAME_SIZE+1; i++) {
		if(g_conf->audio_encoder_name[i] != NULL)
			free(g_conf->audio_encoder_name[i]);
		if(g_conf->video_encoder_name[i] != NULL)
			free(g_conf->video_encoder_name[i]);
	}
	if(g_conf->video_decoder_codec != NULL) {
	}
	if(g_conf->audio_decoder_codec != NULL) {
	}
	if(g_conf->vso != NULL)
		delete g_conf->vso;
	bzero(g_conf, sizeof(struct RTSPConf));
	rtspconf_init(g_conf);
	// default android configuration
	g_conf->audio_device_format = AV_SAMPLE_FMT_S16;
	g_conf->audio_device_channel_layout = AV_CH_LAYOUT_STEREO;
	//
	return;
}

jint
JNI_OnLoad(JavaVM* vm, void* reserved) {
	g_vm = vm;
	return JNI_VERSION_1_6;
}

#define	LOAD_METHOD(id, name, desc)	\
	g_mid_##id = env->GetMethodID(g_class, name, desc); \
	if(g_mid_##id == NULL)	return JNI_FALSE;

// private static native boolean initGAClient(Object thiz);
JNIEXPORT jboolean JNICALL
Java_org_gaminganywhere_gaclient_GAClient_initGAClient(JNIEnv *env, jobject thisObj, jobject thiz) {
	jclass localClass;
	//
	uithread = pthread_self();
	srand(0);
#ifndef ANDROID_NO_FFMPEG
	av_register_all();
	avcodec_register_all();
	avformat_network_init();
#endif
	initConfig();
	//
	if((localClass = env->FindClass("org/gaminganywhere/gaclient/GAClient")) == NULL) {
		ga_log("initGAClient: failed to find class.\n");
		return JNI_FALSE;
	}
	if((g_class = reinterpret_cast<jclass>(env->NewGlobalRef(localClass))) == NULL) {
		ga_log("initGAClient: Failed to new global ref (class).\n");
		return JNI_FALSE;
	}
	if((g_obj = env->NewGlobalRef(thiz)) == NULL) {
		ga_log("initGAClient: Failed to new global ref (this).\n");
		return JNI_FALSE;
	}
	//
	LOAD_METHOD(setScreenDimension, "setScreenDimension", "(II)V");
	LOAD_METHOD(showToast, "showToast", "(Ljava/lang/String;)V");
	LOAD_METHOD(goBack, "goBack", "(I)V");
	LOAD_METHOD(requestRender, "requestRender", "()V");
	LOAD_METHOD(kickWatchdog, "kickWatchdog", "()V");
	LOAD_METHOD(initAudio, "initAudio",
		"(Ljava/lang/String;IIZ)Ljava/lang/Object;");
	LOAD_METHOD(startAudioDecoder, "startAudioDecoder",
		"()Landroid/media/MediaCodec;");
	LOAD_METHOD(decodeAudio, "decodeAudio", "([BIJI)I");
	LOAD_METHOD(initVideo, "initVideo",
		"(Ljava/lang/String;II)Landroid/media/MediaFormat;");
	LOAD_METHOD(videoSetByteBuffer, "videoSetByteBuffer",
		"(Ljava/lang/String;[BI)Landroid/media/MediaFormat;");
	LOAD_METHOD(startVideoDecoder, "startVideoDecoder",
		"()Landroid/media/MediaCodec;");
	LOAD_METHOD(decodeVideo, "decodeVideo", "([BIJZI)I");
	callbackInitialized = true;
	//
	return JNI_TRUE;
}

#undef LOAD_METHOD

//public native void resetConfig();
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_resetConfig(
	JNIEnv *env, jobject thisObj) {
	initConfig();
	return;
}

//public native void setProtocol(String proto);
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_setProtocol(
	JNIEnv *env, jobject thisObj, jstring proto) {
	const char *sproto = env->GetStringUTFChars(proto, NULL);
	if(sproto == NULL) {
		ga_log("setProtocol: no protocol given.\n");
		return;
	}
	ga_log("setProtocol: only RTSP is supported (set:%s).\n", sproto);
	env->ReleaseStringUTFChars(proto, sproto);
	return;
}

//public native void setHost(String proto;
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_setHost(
	JNIEnv *env, jobject thisObj, jstring host) {
	const char *shost = env->GetStringUTFChars(host, NULL);
	if(shost == NULL) {
		ga_log("setHost: no host given.\n");
		return;
	}
	if(g_conf->servername != NULL)
		free(g_conf->servername);
	g_conf->servername = strdup(shost);
	ga_log("setHost: %s\n", g_conf->servername);
	env->ReleaseStringUTFChars(host, shost);
	return;
}

//public native void setPort(int port);
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_setPort(
	JNIEnv *env, jobject thisObj, jint port) {
	if(port < 1 || port > 65535) {
		ga_log("setPort: invalid port number %d\n", port);
	}
	g_conf->serverport = port;
	ga_log("setPort: %d\n", g_conf->serverport);
	return;
}

//public native void setObjectPath(String objpath);
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_setObjectPath(
	JNIEnv *env, jobject thisObj, jstring objpath) {
	const char *sobjpath = env->GetStringUTFChars(objpath, NULL);
	if(sobjpath == NULL) {
		ga_log("setObjectPath: no objpath given.\n");
		return;
	}
	strncpy(g_conf->object, sobjpath, RTSPCONF_OBJECT_SIZE);
	ga_log("setObjectPath: %s\n", g_conf->object);
	env->ReleaseStringUTFChars(objpath, sobjpath);
	return;
}

//public native void setRTPOverTCP(boolean enabled);
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_setRTPOverTCP(
	JNIEnv *env, jobject thisObj, jboolean enabled) {
	g_conf->proto = (enabled != JNI_FALSE) ? IPPROTO_TCP : IPPROTO_UDP;
	ga_log("setRTPOverTCP: %s\n", g_conf->proto == IPPROTO_TCP ? "true" : "false");
	return;
}

//public native void setCtrlEnable(boolean enabled);
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_setCtrlEnable(
	JNIEnv *env, jobject thisObj, jboolean enabled) {
	g_conf->ctrlenable = (enabled != JNI_FALSE) ? 1 : 0;
	ga_log("setCtrlEnable: %s\n", g_conf->ctrlenable? "true" : "false");
	return;
}

//public native void setCtrlProtocol(boolean tcp);
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_setCtrlProtocol(
	JNIEnv *env, jobject thisObj, jboolean tcp) {
	g_conf->ctrlproto = (tcp != JNI_FALSE) ? IPPROTO_TCP : IPPROTO_UDP;
	ga_log("setCtrlProtocol: %s\n", g_conf->ctrlproto == IPPROTO_TCP ? "tcp" : "udp");
	return;
}

// public native void setCtrlPort(int port);
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_setCtrlPort(
	JNIEnv *env, jobject thisObj, jint port) {
	if(port < 1 || port > 65535) {
		ga_log("setCtrlPort: invalid control port number %d\n", port);
	}
	g_conf->ctrlport = port;
	ga_log("setCtrlPort: %d\n", g_conf->ctrlport);
	return;
}

//public native void setuiltinAudio(boolean enable);
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_setBuiltinAudioInternal(
	JNIEnv *env, jobject thisObj, jboolean enable) {
	if(enable != JNI_FALSE) {
		g_conf->builtin_audio_decoder = 1;
		ga_log("setBuiltinAudio: true\n");
	} else {
		g_conf->builtin_audio_decoder = 0;
		ga_log("setBuiltinAudio: false\n");
	}
	return;
}

//public native void setBuiltinVideo(boolean enable);
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_setBuiltinVideoInternal(
	JNIEnv *env, jobject thisObj, jboolean enable) {
	if(enable != JNI_FALSE) {
		g_conf->builtin_video_decoder = 1;
		ga_log("setBuiltinVideo: true\n");
	} else {
		g_conf->builtin_video_decoder = 0;
		ga_log("setBuiltinVideo: false\n");
	}
	return;
}

//public native void setAudioCodec(String name, int samplerate, int channels);
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_setAudioCodec(
	JNIEnv *env, jobject thisObj,
	/*jstring codecname,*/ jint samplerate, jint channels) {
	//
#if 0
	const char *scodec = env->GetStringUTFChars(codecname, NULL);
	if(scodec== NULL) {
		ga_log("setAudioCodec: no codec name given.\n");
		return;
	}
#endif
	if(g_conf->audio_decoder_name[0] != NULL) {
		free(g_conf->audio_decoder_name[0]);
		g_conf->audio_decoder_name[0] = NULL;
	}
	//g_conf->audio_decoder_name[0] = strdup(scodec);
	g_conf->audio_samplerate = samplerate;
	g_conf->audio_channels = channels;
	ga_log("setAudioCodec: %s, samplerate=%d, channels=%d\n",
		"codec auto-detect",
		g_conf->audio_samplerate,
		g_conf->audio_channels);
#if 0
	env->ReleaseStringUTFChars(codecname, scodec);
#endif
	return;
}

//public native void setDropLateVideoFrame(int ms);
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_setDropLateVideoFrame(JNIEnv *env, jobject thisObj, jint ms) {
	char value[16] = "-1";
	if(ms > 0) {
		snprintf(value, sizeof(value), "%d", ms * 1000);
	}
	ga_conf_writev("max-tolerable-video-delay", value);
	ga_error("libgaclient: configured max-tolerable-video-delay = %s\n", value);
	return;
}

//// control methods
//public native void sendKeyEvent(boolean pressed, int scancode, int sym, int mod, int unicode);
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_sendKeyEvent(
	JNIEnv *env, jobject thisObj, jboolean pressed, jint scancode, jint sym, jint mod, jint unicode) {
	sdlmsg_t m;
	if(g_conf == NULL || g_conf->ctrlenable == 0)
		return;
	sdlmsg_keyboard(&m, pressed == JNI_FALSE ? 0 : 1, scancode, sym, mod, unicode);
	ctrl_client_sendmsg(&m, sizeof(sdlmsg_keyboard_t));
	return;
}

//public native void sendMouseKey(boolean pressed, int button, int x, int y);
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_sendMouseKey(
	JNIEnv *env, jobject thisObj, jboolean pressed, jint button, jint x, jint y) {
	sdlmsg_t m;
	if(g_conf == NULL || g_conf->ctrlenable == 0)
		return;
	sdlmsg_mousekey(&m, pressed == JNI_FALSE ? 0 : 1, button, x, y);
	ctrl_client_sendmsg(&m, sizeof(sdlmsg_mouse_t));
	return;
}

//public native void sendMouseMotion(int x, int y, int xrel, int yrel, int state, boolean relative);
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_sendMouseMotion(
	JNIEnv *env, jobject thisObj, jint x, jint y, jint xrel, jint yrel, jint state, jboolean relative) {
	sdlmsg_t m;
	if(g_conf == NULL || g_conf->ctrlenable == 0)
		return;
	sdlmsg_mousemotion(&m, x, y, xrel, yrel, state, relative == JNI_FALSE ? 0 : 1);
	ctrl_client_sendmsg(&m, sizeof(sdlmsg_mouse_t));
	return;
}

//public native void sendMouseWheel(int dx, int dy);
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_sendMouseWheel(
	JNIEnv *env, jobject thisObj, jint dx, jint dy) {
	sdlmsg_t m;
	if(g_conf == NULL || g_conf->ctrlenable == 0)
		return;
	sdlmsg_mousewheel(&m, dx, dy);
	ctrl_client_sendmsg(&m, sizeof(sdlmsg_mouse_t));
	return;
}

static int
launch_controller_client(JNIEnv *env) {
	if(g_conf->ctrlenable == 0)
		return 0;
	if(ctrl_queue_init(32768, sizeof(sdlmsg_t)) < 0) {
		showToast(env, "Err: Controller disabled (no queue)");
		ga_log("Cannot initialize controller queue, controller disabled.\n");
		g_conf->ctrlenable = 0;
		return -1;
	}
	if(pthread_create(&ctrlthread, NULL, ctrl_client_thread, g_conf) != 0) {
		showToast(env, "Err: Controller disabled (no thread)");
		ga_log("Cannot create controller thread, controller disabled.\n");
		g_conf->ctrlenable = 0;
		return -1;
	}
	pthread_detach(ctrlthread);
	return 0;
}

static void
disconnect(JNIEnv *env) {
	//
	ctrl_client_sendmsg(NULL, 0);
	rtspThreadParam.running = false;
	rtspThreadParam.quitLive555 = 1;
	bzero(&ctrlthread, sizeof(ctrlthread));
	bzero(&rtspthread, sizeof(rtspthread));
	usleep(1500000);
	//ctrl_queue_free();
	//
	return;
}

static void
rtspConnect_cleanup(JNIEnv *env) {
	int i;
	if(jaudioBuf != NULL) {
		env->DeleteGlobalRef(jaudioBuf);
		jaudioBuf = NULL;
	}
	//
	initConfig();
	return;
}

//public native void GLresize(int width, int height);
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_GLresize(
	JNIEnv *env, jobject thisObj, jint width, jint height) {
	gl_resize(width, height);
	return;
}

//private native void GLrenderInternal();
JNIEXPORT jboolean JNICALL
Java_org_gaminganywhere_gaclient_GAClient_GLrenderInternal(
	JNIEnv *env, jobject thisObj) {
	if(gl_render() < 0)
		return JNI_FALSE;
	return JNI_TRUE;
}

//private native boolean audioBufferFill(byte[] stream, int size);
JNIEXPORT jint JNICALL
Java_org_gaminganywhere_gaclient_GAClient_audioBufferFill(
	JNIEnv *env, jobject thisObj, jbyteArray stream, jint size) {
	static unsigned char tmpbuf[4096];
	int fillsize = size > sizeof(tmpbuf) ? sizeof(tmpbuf) : size;
	if(size <= 0)
		return 0;
	fillsize = audio_buffer_fill(NULL, tmpbuf, fillsize);
	if(fillsize > 0) {
		env->SetByteArrayRegion(stream,
			(jsize) 0, (jsize) fillsize, (jbyte*) tmpbuf);
	}
	return fillsize;
}

//public native boolean connect();
JNIEXPORT jboolean JNICALL
Java_org_gaminganywhere_gaclient_GAClient_rtspConnect(
	JNIEnv *env, jobject thisObj) {
	char urlbuf[2048];
	//
	if(g_conf->servername[0] == '\0'
	|| g_conf->object[0] == '\0') {
		ga_log("connect: No host or objpath given.\n");
		return JNI_FALSE;
	}
	snprintf(urlbuf, sizeof(urlbuf), "rtsp://%s:%d%s",
		g_conf->servername, g_conf->serverport, g_conf->object);
	ga_log("Target URL[%s]\n", urlbuf);
	//
#if 0
	if(g_conf->builtin_video_decoder == 0) {
		g_conf->video_decoder_codec =
			ga_avcodec_find_decoder((const char **) g_conf->video_decoder_name, AV_CODEC_ID_NONE);
		if(g_conf->video_decoder_codec == NULL) {
			showToast(env, "codec %s not found", 
				g_conf->video_decoder_name[0] == NULL ? 
				"(null)" : g_conf->video_decoder_name[0]);
			ga_log("rtsp client: no available video codec: %s\n",
				g_conf->video_decoder_name[0]);
			return JNI_FALSE;
		}
		ga_log("rtspConnect: found software video codec %s\n",
				g_conf->video_decoder_name[0]);
	}
	if(g_conf->builtin_audio_decoder == 0) {
		g_conf->audio_decoder_codec =
			ga_avcodec_find_decoder((const char **) g_conf->audio_decoder_name, AV_CODEC_ID_NONE);
		if(g_conf->audio_decoder_codec == NULL) {
			showToast(env, "codec %s not found", 
				g_conf->video_decoder_name[0] == NULL ? 
				"(null)" : g_conf->video_decoder_name[0]);
			ga_log("rtsp client: no available audio codec: %s\n",
				g_conf->audio_decoder_name[0]);
			return JNI_FALSE;
		}
		ga_log("rtspConnect: found software audio codec %s\n",
				g_conf->audio_decoder_name[0]);
	}
#endif
	//
	launch_controller_client(env);
	//
	bzero(&rtspThreadParam, sizeof(rtspThreadParam));
	rtspThreadParam.url = strdup(urlbuf);
	rtspThreadParam.running = true;
	rtspThreadParam.rtpOverTCP = (g_conf->proto == IPPROTO_TCP) ? true : false;
	rtspThreadParam.jnienv = env;
	for(int i = 0; i < VIDEO_SOURCE_CHANNEL_MAX; i++) {
		pthread_mutex_init(&rtspThreadParam.surfaceMutex[i], NULL);
	}
	//
	rtsp_thread(&rtspThreadParam);
	if(rtspThreadParam.url != NULL)
		free((void*) rtspThreadParam.url);
	//
	ctrl_client_sendmsg(NULL, 0);
	rtspConnect_cleanup(env);
	//
	return JNI_TRUE;
}

//public native void disconnect();
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_rtspDisconnect(
	JNIEnv *env, jobject thisObj) {
	//
	disconnect(env);
	//
	return;
}

