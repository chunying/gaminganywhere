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

#ifndef __LIBGACLIENT_H__
#define __LIBGACLIENT_H__

#include <jni.h>
#include "ga-common.h"
#include "ga-avcodec.h"

JNIEnv * attachThread(JNIEnv *jnienv);
void detachThread(void *jnienv);
void setScreenDimension(JNIEnv *jnienv, int w, int h);
void showToast(JNIEnv *jnienv, const char *fmt, ...);
void goBack(JNIEnv *jnienv, jint exitCode);
void requestRender(JNIEnv *jnienv);
void kickWatchdog(JNIEnv *jnienv);
jobject initAudio(JNIEnv *jnienv, const char *mime, int sampleRate, int channelCount, bool builtinDecoder);
jobject startAudioDecoder(JNIEnv *jnienv);
int decodeAudio(JNIEnv *jnienv, unsigned char *data, int len, struct timeval pts, int flags);
void stopAudioDecoder(JNIEnv *jnienv);
jobject initVideo(JNIEnv *jnienv, const char *mime, int width, int height);
jobject videoSetByteBuffer(JNIEnv *jnienv, const char *name, unsigned char *value, int len);
jobject startVideoDecoder(JNIEnv *jnienv);
int decodeVideo(JNIEnv *jnienv, unsigned char *data, int len, struct timeval pts, bool marker, int flags);
void stopVideoDecoder(JNIEnv *jnienv);

int create_overlay(int ch, int w, int h, PixelFormat format);

#ifdef __cplusplus
extern "C" {
#endif

// private static native boolean initGAClient(Object weak_this);
JNIEXPORT jboolean JNICALL Java_org_gaminganywhere_gaclient_GAClient_initGAClient(
		JNIEnv *env, jobject thisObj, jobject weak_this);
//jint JNI_OnLoad(JavaVM* vm, void* reserved);
JNIEXPORT void JNICALL Java_org_gaminganywhere_gaclient_GAClient_resetConfig(
		JNIEnv *env, jobject thisObj);
JNIEXPORT void JNICALL Java_org_gaminganywhere_gaclient_GAClient_setProtocol(
		JNIEnv *env, jobject thisObj, jstring proto);
JNIEXPORT void JNICALL Java_org_gaminganywhere_gaclient_GAClient_setHost(
		JNIEnv *env, jobject thisObj, jstring host);
JNIEXPORT void JNICALL Java_org_gaminganywhere_gaclient_GAClient_setPort(
		JNIEnv *env, jobject thisObj, jint port);
JNIEXPORT void JNICALL Java_org_gaminganywhere_gaclient_GAClient_setObjectPath(
		JNIEnv *env, jobject thisObj, jstring objpath);
JNIEXPORT void JNICALL Java_org_gaminganywhere_gaclient_GAClient_setRTPOverTCP(
		JNIEnv *env, jobject thisObj, jboolean enabled);
JNIEXPORT void JNICALL Java_org_gaminganywhere_gaclient_GAClient_setCtrlEnable(
		JNIEnv *env, jobject thisObj, jboolean enabled);
JNIEXPORT void JNICALL Java_org_gaminganywhere_gaclient_GAClient_setCtrlProtocol(
		JNIEnv *env, jobject thisObj, jboolean tcp);
JNIEXPORT void JNICALL Java_org_gaminganywhere_gaclient_GAClient_setCtrlPort(
		JNIEnv *env, jobject thisObj, jint port);
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_setBuiltinAudioInternal(
		JNIEnv *env, jobject thisObj, jboolean enable);
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_setBuiltinVideoInternal(
		JNIEnv *env, jobject thisObj, jboolean enable);
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_setAudioCodec(
		JNIEnv *env, jobject thisObj,
		/*jstring codecname,*/ jint samplerate, jint channels);
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_setDropLateVideoFrame(JNIEnv *env, jobject thisObj, jint ms);
////////////////////////////////////////////////////////////////////////
JNIEXPORT void JNICALL Java_org_gaminganywhere_gaclient_GAClient_sendKeyEvent(
		JNIEnv *env, jobject thisObj, jboolean pressed, jint scancode, jint sym, jint mod, jint unicode);
JNIEXPORT void JNICALL Java_org_gaminganywhere_gaclient_GAClient_sendMouseKey(
		JNIEnv *env, jobject thisObj, jboolean pressed, jint button, jint x, jint y);
JNIEXPORT void JNICALL Java_org_gaminganywhere_gaclient_GAClient_sendMouseMotion(
		JNIEnv *env, jobject thisObj, jint x, jint y, jint xrel, jint yrel, jint state, jboolean relative);
JNIEXPORT void JNICALL Java_org_gaminganywhere_gaclient_GAClient_sendMouseWheel(
		JNIEnv *env, jobject thisObj, jint dx, jint dy);
// GL
JNIEXPORT void JNICALL
Java_org_gaminganywhere_gaclient_GAClient_GLresize(
		JNIEnv *env, jobject thisObj, jint width, jint height);
JNIEXPORT jboolean JNICALL
Java_org_gaminganywhere_gaclient_GAClient_GLrenderInternal(
		JNIEnv *env, jobject thisObj);
JNIEXPORT jint JNICALL
Java_org_gaminganywhere_gaclient_GAClient_audioBufferFill(
		JNIEnv *env, jobject thisObj, jbyteArray stream, jint size);
//
JNIEXPORT jboolean JNICALL Java_org_gaminganywhere_gaclient_GAClient_rtspConnect(
		JNIEnv *env, jobject thisObj);
JNIEXPORT void JNICALL Java_org_gaminganywhere_gaclient_GAClient_rtspDisconnect(
		JNIEnv *env, jobject thisObj);

#ifdef __cplusplus
}
#endif

#endif
