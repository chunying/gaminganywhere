/*
 * Copyright (c) 2013-2015 Chun-Ying Huang
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
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>

#ifdef NO_LIBGA
////////
#include <list>
using namespace std;
#define ga_error	printf
////////
#else
#include "ga-common.h"
#include "ga-conf.h"
#endif

#include "omx-streamer.h"

#define CAM_DEVICE_NUMBER               0

#if 0	// replaced with user configurations
#define CAM_SHARPNESS                   0                       // -100 .. 100
#define CAM_CONTRAST                    0                       // -100 .. 100
#define CAM_BRIGHTNESS                  50                      // 0 .. 100
#define CAM_SATURATION                  0                       // -100 .. 100
#define CAM_EXPOSURE_VALUE_COMPENSTAION 0
#define CAM_EXPOSURE_ISO_SENSITIVITY    100
#define CAM_EXPOSURE_AUTO_SENSITIVITY   OMX_FALSE
#define CAM_FRAME_STABILISATION         OMX_TRUE
#define CAM_WHITE_BALANCE_CONTROL       OMX_WhiteBalControlOff  // OMX_WHITEBALCONTROLTYPE
#define CAM_IMAGE_FILTER                OMX_ImageFilterNoise    // OMX_IMAGEFILTERTYPE
#define CAM_FLIP_HORIZONTAL             OMX_FALSE
#define CAM_FLIP_VERTICAL               OMX_FALSE
#endif

#define OMX_INIT_STRUCTURE(a) \
		bzero(&(a), sizeof(a)); \
		(a).nSize = sizeof(a); \
		(a).nVersion.nVersion = OMX_VERSION; \
		(a).nVersion.s.nVersionMajor = OMX_VERSION_MAJOR; \
		(a).nVersion.s.nVersionMinor = OMX_VERSION_MINOR; \
		(a).nVersion.s.nRevision = OMX_VERSION_REVISION; \
		(a).nVersion.s.nStep = OMX_VERSION_STEP

static pthread_mutex_t reconf_mutex = PTHREAD_MUTEX_INITIALIZER;

static void
block_until_state_changed(OMX_HANDLETYPE hComponent, OMX_STATETYPE wanted_eState) {
	OMX_STATETYPE eState;
	int i = 0;
	while(i++ == 0 || eState != wanted_eState) {
		OMX_GetState(hComponent, &eState);
		if(eState != wanted_eState) {
			usleep(10000);
		}
	}
	return;
}

static void
block_until_port_changed(OMX_HANDLETYPE hComponent, OMX_U32 nPortIndex, OMX_BOOL bEnabled) {
	OMX_ERRORTYPE r;
	OMX_PARAM_PORTDEFINITIONTYPE portdef;
	OMX_INIT_STRUCTURE(portdef);
	portdef.nPortIndex = nPortIndex;
	OMX_U32 i = 0;
	while(i++ == 0 || portdef.bEnabled != bEnabled) {
		if((r = OMX_GetParameter(hComponent, OMX_IndexParamPortDefinition, &portdef)) != OMX_ErrorNone) {
			ga_error("streamer: failed to get port definition (err=%x)\n", r);
			exit(-1);
		}
		if(portdef.bEnabled != bEnabled) {
			usleep(10000);
		}
	}
	return;
}

static void
block_until_flushed(omx_streamer_t *ctx) {
	int quit;
	while(!quit) {
		vcos_semaphore_wait(&ctx->handler_lock);
		if(ctx->flushed) {
			ctx->flushed = 0;
			quit = 1;
		}
		vcos_semaphore_post(&ctx->handler_lock);
		if(!quit) {
			usleep(10000);
		}
	}
	return;
}

static void
init_component_handle(const char *name, OMX_HANDLETYPE* hComponent, OMX_PTR pAppData, OMX_CALLBACKTYPE* callbacks) {
	OMX_ERRORTYPE r;
	char fullname[32];

	// Get handle
	snprintf(fullname, sizeof(fullname), "OMX.broadcom.%s", name);
	if((r = OMX_GetHandle(hComponent, fullname, pAppData, callbacks)) != OMX_ErrorNone) {
		ga_error("streamer: failed to get handle for component %s (err=%x)\n", fullname, r);
		exit(-1);
	}

	// Disable ports
	OMX_INDEXTYPE types[] = {
		OMX_IndexParamAudioInit,
		OMX_IndexParamVideoInit,
		OMX_IndexParamImageInit,
		OMX_IndexParamOtherInit
	};
	OMX_PORT_PARAM_TYPE ports;
	OMX_INIT_STRUCTURE(ports);
	OMX_GetParameter(*hComponent, OMX_IndexParamVideoInit, &ports);

	int i;
	for(i = 0; i < 4; i++) {
		if(OMX_GetParameter(*hComponent, types[i], &ports) == OMX_ErrorNone) {
			OMX_U32 nPortIndex;
			for(nPortIndex = ports.nStartPortNumber; nPortIndex < ports.nStartPortNumber + ports.nPorts; nPortIndex++) {
				//ga_error("streamer: disabling port %d of component %s\n", nPortIndex, fullname);
				if((r = OMX_SendCommand(*hComponent, OMX_CommandPortDisable, nPortIndex, NULL)) != OMX_ErrorNone) {
					ga_error("streamer: failed to disable port %d of component %s\n", nPortIndex, fullname);
					exit(-1);
				}
				block_until_port_changed(*hComponent, nPortIndex, OMX_FALSE);
			}
		}
	}
	return;
}

static void
dump_event(OMX_HANDLETYPE hComponent, OMX_EVENTTYPE eEvent, OMX_U32 nData1, OMX_U32 nData2) {
	const char *e;
	switch(eEvent) {
	case OMX_EventCmdComplete:          e = "command complete";                   break;
	case OMX_EventError:                e = "error";                              break;
	case OMX_EventParamOrConfigChanged: e = "parameter or configuration changed"; break;
	case OMX_EventPortSettingsChanged:  e = "port settings changed";              break;
					    /* That's all I've encountered during hacking so let's not bother with the rest... */
	default:
					    e = "(no description)";
	}
	ga_error("*** Received event 0x%08x %s, hComponent:0x%08x, nData1:0x%08x, nData2:0x%08x\n",
			eEvent, e, hComponent, nData1, nData2);
	return;
}

static OMX_ERRORTYPE
event_handler(
        OMX_HANDLETYPE hComponent,
        OMX_PTR pAppData,
        OMX_EVENTTYPE eEvent,
        OMX_U32 nData1,
        OMX_U32 nData2,
	OMX_PTR pEventData) {

	//dump_event(hComponent, eEvent, nData1, nData2);

	omx_streamer_t *ctx = (omx_streamer_t*) pAppData;

	switch(eEvent) {
	case OMX_EventCmdComplete:
		vcos_semaphore_wait(&ctx->handler_lock);
		if(nData1 == OMX_CommandFlush) {
			ctx->flushed = 1;
		}
		vcos_semaphore_post(&ctx->handler_lock);
		break;
	case OMX_EventParamOrConfigChanged:
		vcos_semaphore_wait(&ctx->handler_lock);
		if(nData2 == OMX_IndexParamCameraDeviceNumber) {
			ctx->camera_ready = 1;
		}
		vcos_semaphore_post(&ctx->handler_lock);
		break;
	case OMX_EventError:
		ga_error("error event received (err=%x)\n", nData1);
		exit(-1);
		break;
	default:
		break;
	}
	return OMX_ErrorNone;
}

// the output buffer with captured video data
static OMX_ERRORTYPE
fill_output_buffer_done_handler(
        OMX_HANDLETYPE hComponent,
        OMX_PTR pAppData,
	OMX_BUFFERHEADERTYPE* pBuffer) {
	//
	omx_streamer_t *ctx = ((omx_streamer_t*) pAppData);
	vcos_semaphore_wait(&ctx->handler_lock);
	// The main loop can now flush the buffer to output file
	ctx->encoder_output_buffer_available = 1;
	vcos_semaphore_post(&ctx->handler_lock);
	return OMX_ErrorNone;
}

static int
omx_streamer_deinit_streamer(omx_streamer_t *ctx) {
	OMX_ERRORTYPE r;
	/////////////////////////////////////
	// Return the last full buffer back to the encoder component
	if(ctx->outbuf == NULL)
		goto flush_port_73;
	ctx->outbuf->nFlags = OMX_BUFFERFLAG_EOS;
	if((r = OMX_FillThisBuffer(ctx->encoder, ctx->outbuf)) != OMX_ErrorNone) {
		ga_error("streamer: failed to request filling of the output buffer on encoder output port 201\n");
	}
	// Flush the buffers on each component
flush_port_73:
	if((r = OMX_SendCommand(ctx->camera, OMX_CommandFlush, 73, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to flush buffers of camera input port 73\n");
		goto flush_port_70;
	}
	block_until_flushed(ctx);
flush_port_70:
	if((r = OMX_SendCommand(ctx->camera, OMX_CommandFlush, 70, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to flush buffers of camera preview output port 70\n");
		goto flush_port_71;
	}
	block_until_flushed(ctx);
flush_port_71:
	if((r = OMX_SendCommand(ctx->camera, OMX_CommandFlush, 71, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to flush buffers of camera video output port 71\n");
		goto flush_port_200;
	}
	block_until_flushed(ctx);
flush_port_200:
	if((r = OMX_SendCommand(ctx->encoder, OMX_CommandFlush, 200, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to flush buffers of encoder input port 200\n");
		goto flush_port_201;
	}
	block_until_flushed(ctx);
flush_port_201:
	if((r = OMX_SendCommand(ctx->encoder, OMX_CommandFlush, 201, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to flush buffers of encoder output port 201\n");
		goto flush_port_240;
	}
	block_until_flushed(ctx);
flush_port_240:
	if((r = OMX_SendCommand(ctx->null_sink, OMX_CommandFlush, 240, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to flush buffers of null sink input port 240\n");
		goto disable_port_73;
	}
	block_until_flushed(ctx);
	// Disable all the ports
disable_port_73:
	if((r = OMX_SendCommand(ctx->camera, OMX_CommandPortDisable, 73, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to disable camera input port 73\n");
		goto disable_port_70;
	}
	block_until_port_changed(ctx->camera, 73, OMX_FALSE);
disable_port_70:
	if((r = OMX_SendCommand(ctx->camera, OMX_CommandPortDisable, 70, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to disable camera preview output port 70\n");
		goto disable_port_71;
	}
	block_until_port_changed(ctx->camera, 70, OMX_FALSE);
disable_port_71:
	if((r = OMX_SendCommand(ctx->camera, OMX_CommandPortDisable, 71, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to disable camera video output port 71\n");
		goto disable_port_200;
	}
	block_until_port_changed(ctx->camera, 71, OMX_FALSE);
disable_port_200:
	if((r = OMX_SendCommand(ctx->encoder, OMX_CommandPortDisable, 200, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to disable encoder input port 200\n");
		goto disable_port_201;
	}
	block_until_port_changed(ctx->encoder, 200, OMX_FALSE);
disable_port_201:
	if((r = OMX_SendCommand(ctx->encoder, OMX_CommandPortDisable, 201, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to disable encoder output port 201\n");
		goto disable_port_240;
	}
	block_until_port_changed(ctx->encoder, 201, OMX_FALSE);
disable_port_240:
	if((r = OMX_SendCommand(ctx->null_sink, OMX_CommandPortDisable, 240, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to disable null sink input port 240\n");
		goto free_inbuf;
	}
	block_until_port_changed(ctx->null_sink, 240, OMX_FALSE);
	// Free all the buffers
free_inbuf:
	if(ctx->inbuf == NULL)
		goto free_outbuf;
	if((r = OMX_FreeBuffer(ctx->camera, 73, ctx->inbuf)) != OMX_ErrorNone) {
		ga_error("streamer: failed to free buffer for camera input port 73\n");
	}
free_outbuf:
	if(ctx->outbuf == NULL)
		goto switch_camera_to_idle;
	if((r = OMX_FreeBuffer(ctx->encoder, 201, ctx->outbuf)) != OMX_ErrorNone) {
		ga_error("streamer: failed to free buffer for encoder output port 201\n");
	}
	// Transition all the components to idle and then to loaded states
switch_camera_to_idle:
	if((r = OMX_SendCommand(ctx->camera, OMX_CommandStateSet, OMX_StateIdle, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to switch state of the camera component to idle\n");
		goto switch_encoder_to_idle;
	}
	block_until_state_changed(ctx->camera, OMX_StateIdle);
switch_encoder_to_idle:
	if((r = OMX_SendCommand(ctx->encoder, OMX_CommandStateSet, OMX_StateIdle, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to switch state of the encoder component to idle\n");
		goto switch_null_sink_to_idle;
	}
	block_until_state_changed(ctx->encoder, OMX_StateIdle);
switch_null_sink_to_idle:
	if((r = OMX_SendCommand(ctx->null_sink, OMX_CommandStateSet, OMX_StateIdle, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to switch state of the null sink component to idle\n");
		goto switch_camera_to_loaded;
	}
	block_until_state_changed(ctx->null_sink, OMX_StateIdle);
switch_camera_to_loaded:
	if((r = OMX_SendCommand(ctx->camera, OMX_CommandStateSet, OMX_StateLoaded, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to switch state of the camera component to loaded\n");
		goto switch_encoder_to_loaded;
	}
	block_until_state_changed(ctx->camera, OMX_StateLoaded);
switch_encoder_to_loaded:
	if((r = OMX_SendCommand(ctx->encoder, OMX_CommandStateSet, OMX_StateLoaded, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to switch state of the encoder component to loaded\n");
		goto switch_null_sink_to_loaded;
	}
	block_until_state_changed(ctx->encoder, OMX_StateLoaded);
switch_null_sink_to_loaded:
	if((r = OMX_SendCommand(ctx->null_sink, OMX_CommandStateSet, OMX_StateLoaded, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to switch state of the null sink component to loaded\n");
		goto free_camera;
	}
	block_until_state_changed(ctx->null_sink, OMX_StateLoaded);
	// Free the component handles
free_camera:
	if(ctx->camera == NULL)
		goto free_encoder;
	if((r = OMX_FreeHandle(ctx->camera)) != OMX_ErrorNone) {
		ga_error("streamer: failed to free camera component handle\n");
	}
free_encoder:
	if(ctx->encoder == NULL)
		goto free_null_sink;
	if((r = OMX_FreeHandle(ctx->encoder)) != OMX_ErrorNone) {
		ga_error("streamer: failed to free encoder component handle\n");
	}
free_null_sink:
	if(ctx->null_sink == NULL)
		goto delete_semaphore;
	if((r = OMX_FreeHandle(ctx->null_sink)) != OMX_ErrorNone) {
		ga_error("streamer: failed to free null sink component handle\n");
	}
delete_semaphore:
	vcos_semaphore_delete(&ctx->handler_lock);
	//
	if(ctx->buffer != NULL)
		free(ctx->buffer);
	//
	bzero(ctx, sizeof(omx_streamer_t));
	//
	return 0;
}

static void
omx_streamer_default_config(omx_streamer_t *ctx) {
	if(ctx == NULL)
		return;
	ctx->config.camera_sharpness	= OSCAM_DEF_SHARPNESS;
	ctx->config.camera_contrast	= OSCAM_DEF_CONTRAST;
	ctx->config.camera_brightness	= OSCAM_DEF_BRIGHTNESS;
	ctx->config.camera_saturation	= OSCAM_DEF_SATURATION;
	ctx->config.camera_ev		= OSCAM_DEF_EXPOSURE_VALUE_COMPENSATION;
	ctx->config.camera_iso		= OSCAM_DEF_EXPOSURE_ISO_SENSITIVITY;
	ctx->config.camera_iso_auto	= OSCAM_DEF_EXPOSURE_AUTO_SENSITIVITY;
	ctx->config.camera_frame_stabilisation	= OSCAM_DEF_FRAME_STABILISATION;
	ctx->config.camera_flip_horizon		= OSCAM_DEF_FLIP_HORIZONTAL;
	ctx->config.camera_flip_vertical	= OSCAM_DEF_FLIP_VERTICAL;
	ctx->config.camera_whitebalance		= OSCAM_DEF_WHITE_BALANCE_CONTROL;
	ctx->config.camera_filter		= OSCAM_DEF_IMAGE_FILTER;
	return;
}

static int
omx_streamer_init_streamer(omx_streamer_t *ctx, omx_streamer_config_t *config,
		int width, int height, int fps_n, int fps_d, int bitrate, int gopsize) {
	OMX_ERRORTYPE r;
	//
	bzero(ctx, sizeof(omx_streamer_t));
	if(config == NULL)
		omx_streamer_default_config(ctx);
	else
		bcopy(config, &ctx->config, sizeof(omx_streamer_config_t));
	//
	ctx->width = width;
	ctx->height = height;
	ctx->fps_n = fps_n;
	ctx->fps_d = fps_d;
	ctx->bitrate = bitrate;
	ctx->gopsize = gopsize;
	ctx->bufsize = width * height * 6;
	ctx->buffer = (unsigned char*) malloc(ctx->bufsize);
	if(ctx->buffer == NULL) {
		ga_error("streamer: buffer allocation failed (%d byets).\n", ctx->bufsize);
		bzero(ctx, sizeof(omx_streamer_t));
		return -1;
	}
	//
	if(vcos_semaphore_create(&ctx->handler_lock, "handler_lock", 1) != VCOS_SUCCESS) {
		ga_error("streamer: failed to create handler lock semaphore\n");
		free(ctx->buffer);
		bzero(ctx, sizeof(omx_streamer_t));
		return -1;
	}
	// Init component handles
	OMX_CALLBACKTYPE callbacks;
	bzero(&callbacks, sizeof(callbacks));
	callbacks.EventHandler    = event_handler;
	callbacks.FillBufferDone  = fill_output_buffer_done_handler;
	//
	init_component_handle("camera", &ctx->camera , ctx, &callbacks);
	init_component_handle("video_encode", &ctx->encoder, ctx, &callbacks);
	init_component_handle("null_sink", &ctx->null_sink, ctx, &callbacks);
#if 1	//////// configure camera
	// changed signaling that the camera device is ready for use.
	OMX_CONFIG_REQUESTCALLBACKTYPE cbtype;
	OMX_INIT_STRUCTURE(cbtype);
	cbtype.nPortIndex = OMX_ALL;
	cbtype.nIndex     = OMX_IndexParamCameraDeviceNumber;
	cbtype.bEnable    = OMX_TRUE;
	if((r = OMX_SetConfig(ctx->camera, OMX_IndexConfigRequestCallback, &cbtype)) != OMX_ErrorNone) {
		ga_error("streamer: failed to request camera device number parameter change callback for camera\n");
		goto failed;
	}
	// Set device number, this triggers the callback configured just above
	OMX_PARAM_U32TYPE device;
	OMX_INIT_STRUCTURE(device);
	device.nPortIndex = OMX_ALL;
	device.nU32 = CAM_DEVICE_NUMBER;
	if((r = OMX_SetParameter(ctx->camera, OMX_IndexParamCameraDeviceNumber, &device)) != OMX_ErrorNone) {
		ga_error("streamer: failed to set camera parameter device number\n");
		goto failed;
	}
	// Configure video format emitted by camera preview output port
	OMX_PARAM_PORTDEFINITIONTYPE camera_portdef;
	OMX_INIT_STRUCTURE(camera_portdef);
	camera_portdef.nPortIndex = 70;
	if((r = OMX_GetParameter(ctx->camera, OMX_IndexParamPortDefinition, &camera_portdef)) != OMX_ErrorNone) {
		ga_error("streamer: failed to get port definition for camera preview output port 70\n");
		goto failed;
	}
	camera_portdef.format.video.nFrameWidth  = width;
	camera_portdef.format.video.nFrameHeight = height;
	camera_portdef.format.video.xFramerate   = (fps_n << 16) | (fps_d - 1);
	camera_portdef.format.video.nStride      = (camera_portdef.format.video.nFrameWidth + camera_portdef.nBufferAlignment - 1) & (~(camera_portdef.nBufferAlignment - 1));
	camera_portdef.format.video.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
	if((r = OMX_SetParameter(ctx->camera, OMX_IndexParamPortDefinition, &camera_portdef)) != OMX_ErrorNone) {
		ga_error("streamer: failed to set port definition for camera preview output port 70\n");
		goto failed;
	}
	// Configure video format emitted by camera video output port
	// Use configuration from camera preview output as basis for
	// camera video output configuration
	OMX_INIT_STRUCTURE(camera_portdef);
	camera_portdef.nPortIndex = 70;
	if((r = OMX_GetParameter(ctx->camera, OMX_IndexParamPortDefinition, &camera_portdef)) != OMX_ErrorNone) {
		ga_error("streamer: failed to get port definition for camera preview output port 70\n");
		goto failed;
	}
	camera_portdef.nPortIndex = 71;
	if((r = OMX_SetParameter(ctx->camera, OMX_IndexParamPortDefinition, &camera_portdef)) != OMX_ErrorNone) {
		ga_error("streamer: failed to set port definition for camera video output port 71\n");
		goto failed;
	}
	// Configure frame rate
	OMX_CONFIG_FRAMERATETYPE framerate;
	OMX_INIT_STRUCTURE(framerate);
	framerate.nPortIndex = 70;
	framerate.xEncodeFramerate = camera_portdef.format.video.xFramerate;
	if((r = OMX_SetConfig(ctx->camera, OMX_IndexConfigVideoFramerate, &framerate)) != OMX_ErrorNone) {
		ga_error("streamer: failed to set framerate configuration for camera preview output port 70\n");
		goto failed;
	}
	framerate.nPortIndex = 71;
	if((r = OMX_SetConfig(ctx->camera, OMX_IndexConfigVideoFramerate, &framerate)) != OMX_ErrorNone) {
		ga_error("streamer: failed to set framerate configuration for camera video output port 71\n");
		goto failed;
	}
	// Configure sharpness
	OMX_CONFIG_SHARPNESSTYPE sharpness;
	OMX_INIT_STRUCTURE(sharpness);
	sharpness.nPortIndex = OMX_ALL;
	sharpness.nSharpness = ctx->config.camera_sharpness;	//CAM_SHARPNESS;
	if((r = OMX_SetConfig(ctx->camera, OMX_IndexConfigCommonSharpness, &sharpness)) != OMX_ErrorNone) {
		ga_error("streamer: failed to set camera sharpness configuration\n");
		goto failed;
	}
	// Configure contrast
	OMX_CONFIG_CONTRASTTYPE contrast;
	OMX_INIT_STRUCTURE(contrast);
	contrast.nPortIndex = OMX_ALL;
	contrast.nContrast = ctx->config.camera_contrast;	//CAM_CONTRAST;
	if((r = OMX_SetConfig(ctx->camera, OMX_IndexConfigCommonContrast, &contrast)) != OMX_ErrorNone) {
		ga_error("streamer: failed to set camera contrast configuration\n");
		goto failed;
	}
	// Configure saturation
	OMX_CONFIG_SATURATIONTYPE saturation;
	OMX_INIT_STRUCTURE(saturation);
	saturation.nPortIndex = OMX_ALL;
	saturation.nSaturation = ctx->config.camera_saturation;	//CAM_SATURATION;
	if((r = OMX_SetConfig(ctx->camera, OMX_IndexConfigCommonSaturation, &saturation)) != OMX_ErrorNone) {
		ga_error("streamer: failed to set camera saturation configuration\n");
		goto failed;
	}
	// Configure brightness
	OMX_CONFIG_BRIGHTNESSTYPE brightness;
	OMX_INIT_STRUCTURE(brightness);
	brightness.nPortIndex = OMX_ALL;
	brightness.nBrightness = ctx->config.camera_brightness;	//CAM_BRIGHTNESS;
	if((r = OMX_SetConfig(ctx->camera, OMX_IndexConfigCommonBrightness, &brightness)) != OMX_ErrorNone) {
		ga_error("streamer: failed to set camera brightness configuration\n");
		goto failed;
	}
	// Configure exposure value
	OMX_CONFIG_EXPOSUREVALUETYPE exposure_value;
	OMX_INIT_STRUCTURE(exposure_value);
	exposure_value.nPortIndex = OMX_ALL;
	exposure_value.xEVCompensation = ctx->config.camera_ev;		//CAM_EXPOSURE_VALUE_COMPENSTAION;
	exposure_value.bAutoSensitivity = ctx->config.camera_iso_auto;	//CAM_EXPOSURE_AUTO_SENSITIVITY;
	exposure_value.nSensitivity = ctx->config.camera_iso;		//CAM_EXPOSURE_ISO_SENSITIVITY;
	if((r = OMX_SetConfig(ctx->camera, OMX_IndexConfigCommonExposureValue, &exposure_value)) != OMX_ErrorNone) {
		ga_error("streamer: failed to set camera exposure value configuration\n");
		goto failed;
	}
	// Configure frame frame stabilisation
	OMX_CONFIG_FRAMESTABTYPE frame_stabilisation_control;
	OMX_INIT_STRUCTURE(frame_stabilisation_control);
	frame_stabilisation_control.nPortIndex = OMX_ALL;
	frame_stabilisation_control.bStab = ctx->config.camera_frame_stabilisation;	//CAM_FRAME_STABILISATION;
	if((r = OMX_SetConfig(ctx->camera, OMX_IndexConfigCommonFrameStabilisation, &frame_stabilisation_control)) != OMX_ErrorNone) {
		ga_error("streamer: failed to set camera frame frame stabilisation control configuration\n");
		goto failed;
	}
	// Configure frame white balance control
	OMX_CONFIG_WHITEBALCONTROLTYPE white_balance_control;
	OMX_INIT_STRUCTURE(white_balance_control);
	white_balance_control.nPortIndex = OMX_ALL;
	white_balance_control.eWhiteBalControl = ctx->config.camera_whitebalance;	//CAM_WHITE_BALANCE_CONTROL;
	if((r = OMX_SetConfig(ctx->camera, OMX_IndexConfigCommonWhiteBalance, &white_balance_control)) != OMX_ErrorNone) {
		ga_error("streamer: failed to set camera frame white balance control configuration\n");
		goto failed;
	}
	// Configure image filter
	OMX_CONFIG_IMAGEFILTERTYPE image_filter;
	OMX_INIT_STRUCTURE(image_filter);
	image_filter.nPortIndex = OMX_ALL;
	image_filter.eImageFilter = ctx->config.camera_filter;	//CAM_IMAGE_FILTER;
	if((r = OMX_SetConfig(ctx->camera, OMX_IndexConfigCommonImageFilter, &image_filter)) != OMX_ErrorNone) {
		ga_error("streamer: failed to set camera image filter configuration\n");
		goto failed;
	}
	// Configure mirror
	OMX_MIRRORTYPE eMirror;
	eMirror = OMX_MirrorNone;
	if(ctx->config.camera_flip_horizon/*CAM_FLIP_HORIZONTAL*/
	&&!ctx->config.camera_flip_vertical/*!CAM_FLIP_VERTICAL*/) {
		eMirror = OMX_MirrorHorizontal;
	} else	if(!ctx->config.camera_flip_horizon/*!CAM_FLIP_HORIZONTAL*/
	     	&& ctx->config.camera_flip_vertical/*CAM_FLIP_VERTICAL*/) {
		eMirror = OMX_MirrorVertical;
	} else	if(ctx->config.camera_flip_horizon/*CAM_FLIP_HORIZONTAL*/
		&& ctx->config.camera_flip_vertical/*CAM_FLIP_VERTICAL*/) {
		eMirror = OMX_MirrorBoth;
	}
	OMX_CONFIG_MIRRORTYPE mirror;
	OMX_INIT_STRUCTURE(mirror);
	mirror.nPortIndex = 71;
	mirror.eMirror = eMirror;
	if((r = OMX_SetConfig(ctx->camera, OMX_IndexConfigCommonMirror, &mirror)) != OMX_ErrorNone) {
		ga_error("streamer: failed to set mirror configuration for camera video output port 71\n");
		goto failed;
	}
	// Ensure camera is ready
	while(!ctx->camera_ready) {
		usleep(10000);
	}
	ga_error("streamer: camera ready!\n");
#endif
#if 1	//////// configure encoder
	// Configure video format emitted by encoder output port
	OMX_PARAM_PORTDEFINITIONTYPE encoder_portdef;
	OMX_INIT_STRUCTURE(encoder_portdef);
	encoder_portdef.nPortIndex = 201;
	if((r = OMX_GetParameter(ctx->encoder, OMX_IndexParamPortDefinition, &encoder_portdef)) != OMX_ErrorNone) {
		ga_error("streamer: failed to get port definition for encoder output port 201\n");
		goto failed;
	}
	// Copy some of the encoder output port configuration
	// from camera output port
	encoder_portdef.format.video.nFrameWidth  = camera_portdef.format.video.nFrameWidth;
	encoder_portdef.format.video.nFrameHeight = camera_portdef.format.video.nFrameHeight;
	encoder_portdef.format.video.xFramerate   = camera_portdef.format.video.xFramerate;
	encoder_portdef.format.video.nStride      = camera_portdef.format.video.nStride;
	// Which one is effective, this or the configuration just below?
	encoder_portdef.format.video.nBitrate     = bitrate;
	if((r = OMX_SetParameter(ctx->encoder, OMX_IndexParamPortDefinition, &encoder_portdef)) != OMX_ErrorNone) {
		ga_error("streamer: failed to set port definition for encoder output port 201\n");
		goto failed;
	}
	// Configure bitrate
	OMX_VIDEO_PARAM_BITRATETYPE bitratetype;
	OMX_INIT_STRUCTURE(bitratetype);
	bitratetype.eControlRate = OMX_Video_ControlRateVariable;
	bitratetype.nTargetBitrate = encoder_portdef.format.video.nBitrate;
	bitratetype.nPortIndex = 201;
	if((r = OMX_SetParameter(ctx->encoder, OMX_IndexParamVideoBitrate, &bitratetype)) != OMX_ErrorNone) {
		ga_error("streamer: failed to set bitrate for encoder output port 201\n");
		goto failed;
	}
	// Configure format
	OMX_VIDEO_PARAM_PORTFORMATTYPE format;
	OMX_INIT_STRUCTURE(format);
	format.nPortIndex = 201;
	format.eCompressionFormat = OMX_VIDEO_CodingAVC;
	if((r = OMX_SetParameter(ctx->encoder, OMX_IndexParamVideoPortFormat, &format)) != OMX_ErrorNone) {
		ga_error("streamer: failed to set video format for encoder output port 201\n");
		goto failed;
	}
	// get avctype
	OMX_VIDEO_PARAM_AVCTYPE avctype;
	OMX_INIT_STRUCTURE(avctype);
	avctype.nPortIndex = 201;
	if((r = OMX_GetParameter(ctx->encoder, OMX_IndexParamVideoAvc, &avctype)) != OMX_ErrorNone) {
		ga_error("streamer: failed to get avctype for encoder output port 201\n");
		goto failed;
	}
	// update and set avctype
	avctype.nPFrames = gopsize-1;
	avctype.nBFrames = 0; /* disable B-Frame */
	avctype.nRefFrames = 1;
	avctype.eProfile = OMX_VIDEO_AVCProfileMain;
	//avctype.eLevel = OMX_VIDEO_AVCLevel42; // -- init failed with this option
	avctype.nAllowedPictureTypes = OMX_VIDEO_PictureTypeI | OMX_VIDEO_PictureTypeP;
	avctype.eLoopFilterMode = OMX_VIDEO_AVCLoopFilterDisable;
	if((r = OMX_SetParameter(ctx->encoder, OMX_IndexParamVideoAvc, &avctype)) != OMX_ErrorNone) {
		ga_error("streamer: failed to set avctype for encoder output port 201\n");
		goto failed;
	}
	// get again and then show
	if((r = OMX_GetParameter(ctx->encoder, OMX_IndexParamVideoAvc, &avctype)) != OMX_ErrorNone) {
		ga_error("streamer: failed to get avctype for encoder output port 201 (again)\n");
		goto failed;
	}
	ga_error("streamer: AVC config - nSliceHeaderSpacing=%d nPFrames=%d nBFrames=%d nRefFrames=%d UEP=%d FMO=%d ASO=%d RS=%d Profile=0x%x Level=0x%x AllowedFrames=0x%x\n",
			avctype.nSliceHeaderSpacing,
			avctype.nPFrames,
			avctype.nBFrames,
			avctype.nRefFrames,
			avctype.bEnableUEP,
			avctype.bEnableFMO,
			avctype.bEnableASO,
			avctype.bEnableRS,
			avctype.eProfile,
			avctype.eLevel,
			avctype.nAllowedPictureTypes);
	//// enable intra-refresh?
	OMX_VIDEO_PARAM_INTRAREFRESHTYPE intrarefreshType;
	bzero(&intrarefreshType, sizeof(intrarefreshType));
	intrarefreshType.nSize = sizeof(intrarefreshType);
	intrarefreshType.nVersion.nVersion = OMX_VERSION;
	intrarefreshType.nPortIndex = 201;
	intrarefreshType.eRefreshMode = OMX_VIDEO_IntraRefreshCyclic;
	intrarefreshType.nCirMBs = 64;
	// XXX: intra-refresh not enabled
#endif
#if 1	//////// configure tunnels
	if((r = OMX_SetupTunnel(ctx->camera, 70, ctx->null_sink, 240)) != OMX_ErrorNone) {
		ga_error("streamer: failed to setup tunnel between camera preview output port 70 and null sink input port 240\n");
		goto failed;
	}
	if((r = OMX_SetupTunnel(ctx->camera, 71, ctx->encoder, 200)) != OMX_ErrorNone) {
		ga_error("streamer: failed to setup tunnel between camera video output port 71 and encoder input port 200\n");
		goto failed;
	}
#endif
#if 1	//////// switch states
	if((r = OMX_SendCommand(ctx->camera, OMX_CommandStateSet, OMX_StateIdle, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to switch state of the camera component to idle\n");
		goto failed;
	}
	block_until_state_changed(ctx->camera, OMX_StateIdle);
	if((r = OMX_SendCommand(ctx->encoder, OMX_CommandStateSet, OMX_StateIdle, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to switch state of the encoder component to idle\n");
		goto failed;
	}
	block_until_state_changed(ctx->encoder, OMX_StateIdle);
	if((r = OMX_SendCommand(ctx->null_sink, OMX_CommandStateSet, OMX_StateIdle, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to switch state of the null sink component to idle\n");
		goto failed;
	}
	block_until_state_changed(ctx->null_sink, OMX_StateIdle);
#endif
#if 1	//////// enabling ports
	if((r = OMX_SendCommand(ctx->camera, OMX_CommandPortEnable, 73, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to enable camera input port 73\n");
		goto failed;
	}
	block_until_port_changed(ctx->camera, 73, OMX_TRUE);
	if((r = OMX_SendCommand(ctx->camera, OMX_CommandPortEnable, 70, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to enable camera preview output port 70\n");
		goto failed;
	}
	block_until_port_changed(ctx->camera, 70, OMX_TRUE);
	if((r = OMX_SendCommand(ctx->camera, OMX_CommandPortEnable, 71, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to enable camera video output port 71\n");
		goto failed;
	}
	block_until_port_changed(ctx->camera, 71, OMX_TRUE);
	if((r = OMX_SendCommand(ctx->encoder, OMX_CommandPortEnable, 200, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to enable encoder input port 200\n");
		goto failed;
	}
	block_until_port_changed(ctx->encoder, 200, OMX_TRUE);
	if((r = OMX_SendCommand(ctx->encoder, OMX_CommandPortEnable, 201, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to enable encoder output port 201\n");
		goto failed;
	}
	block_until_port_changed(ctx->encoder, 201, OMX_TRUE);
	if((r = OMX_SendCommand(ctx->null_sink, OMX_CommandPortEnable, 240, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to enable null sink input port 240\n");
		goto failed;
	}
	block_until_port_changed(ctx->null_sink, 240, OMX_TRUE);
#endif
#if 1	//////// alloc buffers
	OMX_INIT_STRUCTURE(camera_portdef);
	camera_portdef.nPortIndex = 73;
	if((r = OMX_GetParameter(ctx->camera, OMX_IndexParamPortDefinition, &camera_portdef)) != OMX_ErrorNone) {
		ga_error("streamer: failed to get port definition for camera input port 73\n");
		goto failed;
	}
	if((r = OMX_AllocateBuffer(ctx->camera, &ctx->inbuf, 73, NULL, camera_portdef.nBufferSize)) != OMX_ErrorNone) {
		ga_error("streamer: failed to allocate buffer for camera input port 73\n");
		goto failed;
	}
	OMX_INIT_STRUCTURE(encoder_portdef);
	encoder_portdef.nPortIndex = 201;
	if((r = OMX_GetParameter(ctx->encoder, OMX_IndexParamPortDefinition, &encoder_portdef)) != OMX_ErrorNone) {
		ga_error("streamer: failed to get port definition for encoder output port 201\n");
		goto failed;
	}
	if((r = OMX_AllocateBuffer(ctx->encoder, &ctx->outbuf, 201, NULL, encoder_portdef.nBufferSize)) != OMX_ErrorNone) {
		ga_error("streamer: failed to allocate buffer for encoder output port 201\n");
		goto failed;
	}
#endif
#if 1	/////// switch to executing state
	if((r = OMX_SendCommand(ctx->camera, OMX_CommandStateSet, OMX_StateExecuting, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to switch state of the camera component to executing\n");
		goto failed;
	}
	block_until_state_changed(ctx->camera, OMX_StateExecuting);
	if((r = OMX_SendCommand(ctx->encoder, OMX_CommandStateSet, OMX_StateExecuting, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to switch state of the encoder component to executing\n");
		goto failed;
	}
	block_until_state_changed(ctx->encoder, OMX_StateExecuting);
	if((r = OMX_SendCommand(ctx->null_sink, OMX_CommandStateSet, OMX_StateExecuting, NULL)) != OMX_ErrorNone) {
		ga_error("streamer: failed to switch state of the null sink component to executing\n");
		goto failed;
	}
	block_until_state_changed(ctx->null_sink, OMX_StateExecuting);
#endif
	//
	ga_error("streamer: initialized\n");
	//
	return 0;
failed:
	omx_streamer_deinit_streamer(ctx);
	return -1;
}

// ensure that streamer can be safely stopped without hanging
int
omx_streamer_prepare_stop(omx_streamer_t *ctx) {
	OMX_ERRORTYPE r;
	int prepare_stop = 0;
	int one_more_syncframe = 0;
	int frame_out = 0;
	//
	ga_error("streamer: prepare stop - wating for SYNC frame ...\n");
	while(1) {
		if(ctx->encoder_output_buffer_available) {
			// end-of-frame
			if(ctx->outbuf->nFlags & OMX_BUFFERFLAG_ENDOFFRAME) {
				frame_out++;
			}
			// key frame
			if(prepare_stop == 0) {
				one_more_syncframe = ctx->outbuf->nFlags & OMX_BUFFERFLAG_SYNCFRAME;
				prepare_stop = 1;
			}
			if(prepare_stop && (one_more_syncframe ^ (ctx->outbuf->nFlags & OMX_BUFFERFLAG_SYNCFRAME))) {
				ga_error("streamer: prepare stop - SYNC frame detected (%d), terminating ...\n", frame_out);
				break;
			}
		}
		if(ctx->encoder_output_buffer_available || ctx->buffer_filled==0) {
			ctx->buffer_filled = 1;
			ctx->encoder_output_buffer_available = 0;
			if((r = OMX_FillThisBuffer(ctx->encoder, ctx->outbuf)) != OMX_ErrorNone) {
				ga_error("streamer: prepare stop - failed to request filling of the output buffer on encoder output port 201\n");
				return -1;
			}
		}
		usleep(2000);
	}
	return 0;
}

static int
omx_streamer_dummy_encode(omx_streamer_t *ctx) {
	OMX_ERRORTYPE r;
	//
	if(omx_streamer_start(ctx) < 0) {
		ga_error("streamer: start streamer failed (dummy encode)\n");
		return -1;
	}
	//
	while((ctx->spslen == 0 || ctx->ppslen==0) && ctx->frame_out < 16) {
		if(ctx->encoder_output_buffer_available) {
			// end-of-frame
			if(ctx->outbuf->nFlags & OMX_BUFFERFLAG_ENDOFFRAME) {
				ctx->frame_out++;
			}
			// retrieve codec config
			if(ctx->outbuf->nFlags & OMX_BUFFERFLAG_CODECCONFIG) {
				int offset = 0;
				unsigned char *dat = ctx->outbuf->pBuffer + ctx->outbuf->nOffset;
				if(dat[0] == 0 && dat[1] == 0) {
					if(dat[2] == 1) {
						offset = 3;
					} else if(dat[2] == 0 && dat[3] == 1) {
						offset = 4;
					}
				}
				if(offset > 0) {
					if((dat[offset] & 0x1f) == 7) {
						// sps
						ctx->spslen = ctx->outbuf->nFilledLen;
						if(ctx->spslen < sizeof(ctx->sps)) {
							bcopy(dat, ctx->sps, ctx->spslen);
						} else {
							ga_error("streamer: sps too long (%d)\n", ctx->spslen);
							ctx->spslen = 0;
						}
					} else if((dat[offset] & 0x1f) == 8) {
						// pps
						ctx->ppslen = ctx->outbuf->nFilledLen;
						if(ctx->ppslen < sizeof(ctx->pps)) {
							bcopy(dat, ctx->pps, ctx->ppslen);
						} else {
							ga_error("streamer: pps too long (%d)\n", ctx->ppslen);
							ctx->ppslen = 0;
						}
					}
				}
			}
		}
		if(ctx->encoder_output_buffer_available || ctx->buffer_filled==0) {
			ctx->buffer_filled = 1;
			ctx->encoder_output_buffer_available = 0;
			if((r = OMX_FillThisBuffer(ctx->encoder, ctx->outbuf)) != OMX_ErrorNone) {
				ga_error("streamer: failed to request filling of the output buffer on encoder output port 201 (dummy encode)\n");
				return -1;
			}
		}
		usleep(2000);
	}
	//
	if(omx_streamer_prepare_stop(ctx) < 0) {
		ga_error("streamer: prepare stop failed.\n");
	}
	//
	if(omx_streamer_stop(ctx) < 0) {
		ga_error("streamer: start streamer failed (dummy encode)\n");
		return -1;
	}
	//
	if(ctx->spslen == 0 || ctx->ppslen == 0) {
		ga_error("streamer: dummy encode failed to retrieve sps/pps (sps=%d, pps=%d).\n",
			ctx->spslen, ctx->ppslen);
		return -1;
	}
	ga_error("streamer: dummy encode completed [%d output frame(s), sps=%d, pps=%d]\n",
		ctx->frame_out, ctx->spslen, ctx->ppslen);
	//
	return 0;
}

int
omx_streamer_init(omx_streamer_t *ctx, omx_streamer_config_t *config, int width, int height, int fps_n, int fps_d, int bitrate, int gopsize) {
	unsigned char sps[1024], pps[1024];
	int spslen, ppslen;
	//
	if(ctx == NULL)
		return -1;
	//
	if(omx_streamer_init_streamer(ctx, config, width, height, fps_n, fps_d, bitrate, gopsize) < 0) {
		return -1;
	}
	ga_error("streamer: encoder initialized (initial-phase).\n");
	// get SPS & PPS by encoding dummy data
	if(omx_streamer_dummy_encode(ctx) < 0) {
		omx_streamer_deinit_streamer(ctx);
		return -1;
	}
	spslen = ctx->spslen > sizeof(sps) ? sizeof(sps) : ctx->spslen;
	bcopy(ctx->sps, sps, spslen);
	ppslen = ctx->ppslen > sizeof(pps) ? sizeof(pps) : ctx->ppslen;
	bcopy(ctx->pps, pps, ppslen);
	// everything is fine, de-init and re-initialize again!
	ga_error("streamer: resetting streamer ...\n");
	omx_streamer_deinit_streamer(ctx);
	ga_error("streamer: reinitializing ...\n");
	//
	if(omx_streamer_init_streamer(ctx, config, width, height, fps_n, fps_d, bitrate, gopsize) < 0) {
		return -1;
	}
	ctx->spslen = spslen;
	bcopy(sps, ctx->sps, spslen);
	ctx->ppslen = ppslen;
	bcopy(pps, ctx->pps, ppslen);
	ga_error("streamer: codec configuration preserved (sps=%d, pps=%d).\n", ctx->spslen, ctx->ppslen);
	ga_error("streamer: encoder initialized (final-phase).\n");
	ctx->initialized = 1;
	//
	return 0;
}

int
omx_streamer_deinit(omx_streamer_t *ctx) {
	omx_streamer_deinit_streamer(ctx);
	return 0;
}

typedef struct suspend_s {
	omx_streamer_config_t config;
	int		width;
	int		height;
	int		fps_n;
	int		fps_d;
	int		bitrate;
	int		gopsize;
}	suspend_t;

static suspend_t *psuspend = NULL;
static suspend_t suspended;

int
omx_streamer_suspend(omx_streamer_t *ctx) {
	//
	if(psuspend != NULL)
		return 0;
	bzero(&suspended, sizeof(suspended));
	suspended.width = ctx->width;
	suspended.height = ctx->height;
	suspended.fps_n = ctx->fps_n;
	suspended.fps_d = ctx->fps_d;
	suspended.bitrate = ctx->bitrate;
	suspended.gopsize = ctx->gopsize;
	bcopy(&ctx->config, &suspended.config, sizeof(ctx->config));
	//
	pthread_mutex_lock(&reconf_mutex);
	if(omx_streamer_prepare_stop(ctx) < 0) {
		ga_error("streamer[suspend]: prepare stop failed.\n");
		goto failed;
	}
	if(omx_streamer_stop(ctx) < 0) {
		ga_error("streamer[suspend]: stop failed.\n");
		goto failed;
	}
	omx_streamer_deinit_streamer(ctx);
	psuspend = &suspended;
	return 0;
failed:
	pthread_mutex_unlock(&reconf_mutex);
	return -1;
}

int
omx_streamer_resume(omx_streamer_t *ctx) {
	if(psuspend == NULL)
		return 0;
	if(omx_streamer_init_streamer(ctx, &psuspend->config,
			psuspend->width, psuspend->height,
			psuspend->fps_n, psuspend->fps_d,
			psuspend->bitrate, psuspend->gopsize) < 0) {
		ga_error("streamer[resume]: init failed.\n");
		goto failed;
	}
	if(omx_streamer_start(ctx) < 0) {
		ga_error("streamer[resume]: start failed.\n");
		goto failed;
	}
	psuspend = NULL;
	pthread_mutex_unlock(&reconf_mutex);
	return 0;
failed:
	return -1;
}

int
omx_streamer_reconfigure(omx_streamer_t *ctx, int bitrateKbps, unsigned int framerate, unsigned int width, unsigned int height) {
	OMX_ERRORTYPE r;
	int old_width, old_height, old_fps_n, old_fps_d, old_bitrate, old_gopsize;
	int fps_n, fps_d, bitrate;
	omx_streamer_config_t old_config;
	// XXX: looks like it does not allow reconfiguration, so we stop, reset, and restart!
	// preserve old configs
	old_width = width > 0 ? width : ctx->width;
	old_height = height > 0 ? height : ctx->height;
	old_fps_n = ctx->fps_n;
	old_fps_d = ctx->fps_d;
	old_bitrate = ctx->bitrate;
	old_gopsize = ctx->gopsize;
	bcopy(&ctx->config, &old_config, sizeof(old_config));
	// set new parameters
	fps_d = old_fps_d;
	fps_n = old_fps_n;
	if((framerate & 0xffff0000) != 0 && (framerate & 0xffff) != 0) {
		fps_n = (framerate>>16) & 0xffff;
		fps_d = framerate & 0xffff;
	}
	if(bitrateKbps > 0) {
		bitrate = bitrateKbps * 1000;
	} else {
		bitrate = old_bitrate;
	}
	//
	pthread_mutex_lock(&reconf_mutex);
	/////////////////////////////////////////////////////
	if(omx_streamer_prepare_stop(ctx) < 0) {
		ga_error("streamer[reconfigure]: prepare stop failed.\n");
		goto failed;
	}
	if(omx_streamer_stop(ctx) < 0) {
		ga_error("streamer[reconfigure]: stop failed.\n");
		goto failed;
	}
	omx_streamer_deinit_streamer(ctx);
	if(omx_streamer_init_streamer(ctx, &old_config,
			old_width, old_height, fps_n, fps_d, bitrate, old_gopsize) < 0) {
		ga_error("streamer[reconfigure]: init failed.\n");
		goto failed;
	}
	if(omx_streamer_start(ctx) < 0) {
		ga_error("streamer[reconfigure]: start failed.\n");
		goto failed;
	}
	pthread_mutex_unlock(&reconf_mutex);
	return 0;
#if 0
	// get current video source configuration
	OMX_PARAM_PORTDEFINITIONTYPE camera_portdef;
	OMX_INIT_STRUCTURE(camera_portdef);
	camera_portdef.nPortIndex = 70;
	if((r = OMX_GetParameter(ctx->camera, OMX_IndexParamPortDefinition, &camera_portdef)) != OMX_ErrorNone) {
		ga_error("streamer[reconfigure]: failed to get port definition for camera preview output port 70\n");
		goto failed;
	}
	if(framerate != 0) {
		camera_portdef.format.video.xFramerate = framerate;
		if((r = OMX_SetParameter(ctx->camera, OMX_IndexParamPortDefinition, &camera_portdef)) != OMX_ErrorNone) {
			ga_error("streamer[reconfigure]: failed to set port definition for camera preview output port 70\n");
			goto failed;
		}
	}
	// need to reconfigure codec?
	if(framerate == 0 && bitrateKbps == 0)
		return 0;
	// get current encoder configuration
	OMX_PARAM_PORTDEFINITIONTYPE encoder_portdef;
	OMX_INIT_STRUCTURE(encoder_portdef);
	encoder_portdef.nPortIndex = 201;
	if((r = OMX_GetParameter(ctx->encoder, OMX_IndexParamPortDefinition, &encoder_portdef)) != OMX_ErrorNone) {
		ga_error("streamer[reconfigure]: failed to get port definition for encoder output port 201\n");
		goto failed;
	}
	// Copy some of the encoder output port configuration
	// from camera output port
	encoder_portdef.format.video.nFrameWidth  = camera_portdef.format.video.nFrameWidth;
	encoder_portdef.format.video.nFrameHeight = camera_portdef.format.video.nFrameHeight;
	encoder_portdef.format.video.xFramerate   = camera_portdef.format.video.xFramerate;
	encoder_portdef.format.video.nStride      = camera_portdef.format.video.nStride;
	// Which one is effective, this or the configuration just below?
	encoder_portdef.format.video.nBitrate     = bitrateKbps * 1000;
	if((r = OMX_SetParameter(ctx->encoder, OMX_IndexParamPortDefinition, &encoder_portdef)) != OMX_ErrorNone) {
		ga_error("streamer[reconfigure]: failed to set port definition for encoder output port 201\n");
		goto failed;
	}
	// Configure bitrate
	OMX_VIDEO_PARAM_BITRATETYPE bitratetype;
	OMX_INIT_STRUCTURE(bitratetype);
	bitratetype.eControlRate = OMX_Video_ControlRateVariable;
	bitratetype.nTargetBitrate = bitrateKbps * 1000; //encoder_portdef.format.video.nBitrate;
	bitratetype.nPortIndex = 201;
	if((r = OMX_SetParameter(ctx->encoder, OMX_IndexParamVideoBitrate, &bitratetype)) != OMX_ErrorNone) {
		ga_error("streamer: failed to set bitrate for encoder output port 201\n");
		goto failed;
	}
	return 0;
#endif
failed:
	// reconfiguration failed
	pthread_mutex_unlock(&reconf_mutex);
	return -1;
}

static int
omx_streamer_start_stop(omx_streamer_t *ctx, OMX_BOOL start) {
	OMX_ERRORTYPE r;
	OMX_CONFIG_PORTBOOLEANTYPE capture;
	//
	OMX_INIT_STRUCTURE(capture);
	capture.nPortIndex = 71;
	capture.bEnabled = start;
	if((r = OMX_SetParameter(ctx->camera, OMX_IndexConfigPortCapturing, &capture)) != OMX_ErrorNone) {
		ga_error("streamer: failed to switch %s capture on camera video output port 71\n",
			start == OMX_TRUE ? "on" : "off");
		return -1;
	}
	ga_error("streamer: camera %s\n", start == OMX_TRUE ? "started" : "stopped");
	return 0;
}

int
omx_streamer_start(omx_streamer_t *ctx) {
	return omx_streamer_start_stop(ctx, OMX_TRUE);
}

int
omx_streamer_stop(omx_streamer_t *ctx) {
	return omx_streamer_start_stop(ctx, OMX_FALSE);
}

const unsigned char *
omx_streamer_get_h264_sps(omx_streamer_t *ctx, int *size) {
	if(ctx->spslen > 0) {
		*size = ctx->spslen;
		return ctx->sps;
	}
	return NULL;
}

const unsigned char *
omx_streamer_get_h264_pps(omx_streamer_t *ctx, int *size) {
	if(ctx->ppslen > 0) {
		*size = ctx->ppslen;
		return ctx->pps;
	}
	return NULL;
}

unsigned char *
omx_streamer_get(omx_streamer_t *ctx, int *encsize) {
	OMX_ERRORTYPE r;
	int end_of_frame = 0;
	int left;
	unsigned char *dst;
	//
	pthread_mutex_lock(&reconf_mutex);
	//
	left = ctx->bufsize;
	dst = ctx->buffer;
	while(end_of_frame == 0 && left > 0) {
		if(ctx->encoder_output_buffer_available) {
			// end-of-frame
			if(ctx->outbuf->nFlags & OMX_BUFFERFLAG_ENDOFFRAME) {
				end_of_frame = 1;
				ctx->frame_out++;
			}
			if(ctx->outbuf->nFlags & OMX_BUFFERFLAG_CODECCONFIG) {
				if(ctx->outbuf->nFilledLen <= left) {
					bcopy(ctx->outbuf->pBuffer + ctx->outbuf->nOffset, dst, ctx->outbuf->nFilledLen);
					dst += ctx->outbuf->nFilledLen;
					left -= ctx->outbuf->nFilledLen;
				} else {
					ga_error("streamer: codec data too large (%d)\n", ctx->outbuf->nFilledLen);
				}
				end_of_frame = 1;
				goto fill_next;
			}
			// save regular data
			if(ctx->outbuf->nFilledLen > left) {
				ga_error("streamer: insufficient encoding buffer (%d bytes).\n", ctx->bufsize);
				goto fill_next;
			}
			bcopy(ctx->outbuf->pBuffer + ctx->outbuf->nOffset, dst, ctx->outbuf->nFilledLen);
			dst += ctx->outbuf->nFilledLen;
			left -= ctx->outbuf->nFilledLen;
		}
fill_next:
		if(ctx->encoder_output_buffer_available || ctx->buffer_filled==0) {
			ctx->buffer_filled = 1;
			ctx->encoder_output_buffer_available = 0;
			if((r = OMX_FillThisBuffer(ctx->encoder, ctx->outbuf)) != OMX_ErrorNone) {
				pthread_mutex_unlock(&reconf_mutex);
				ga_error("streamer: failed to request filling of the output buffer on encoder output port 201\n");
				return NULL;
			}
		}
		usleep(2000);
	}
	if(encsize == NULL) {
		ga_error("streamer: no encsize given\n");
		pthread_mutex_unlock(&reconf_mutex);
		return NULL;
	}
	*encsize = ctx->bufsize - left;
	pthread_mutex_unlock(&reconf_mutex);
	return ctx->buffer;
}

#ifndef GA_MODULE

static int quit_flag = 0;

void do_quit(int s) {
	quit_flag = 1;
}

int
main(int argc, char *argv[]) {
	FILE *fout = NULL;
	int width, height, fps;
	struct timeval tv1, tv2;
	long long elapsed = 0LL;
	omx_streamer_t omx;
	//
	bcm_host_init();
	if(OMX_Init() != OMX_ErrorNone) {
		ga_error("OMX_Init() failed.\n");
		return -1; 
	}   
	//
	bzero(&omx, sizeof(omx));
	//
	if(argc < 4) {
		ga_error("usage: %s width height fps [output.264]\n", argv[0]);
		return -1;
	}
	//
	width = strtol(argv[1], NULL, 0);
	height = strtol(argv[2], NULL, 0);
	fps = strtol(argv[3], NULL, 0);
	//
	ga_error("capture: %dx%d, %d fps\n", width, height, fps);
	//
	if(omx_streamer_init(&omx, width, height, fps, 1, 3000*1000, fps) < 0) {
		ga_error("encoder-omx: init failed.\n");
		OMX_Deinit();
		return -1;
	}
	if(argc > 4 && (fout = fopen(argv[4], "wb")) == NULL) {
		ga_error("open %s failed.\n", argv[5]);
		goto quit;
	}
#if 0
	// write sps/pps
	if(fout) {
		const unsigned char *raw;
		int rawsize;
		if((raw = omx_streamer_get_h264_sps(&omx, &rawsize)) != NULL)
			fwrite(raw, 1, rawsize, fout);
		if((raw = omx_streamer_get_h264_pps(&omx, &rawsize)) != NULL)
			fwrite(raw, 1, rawsize, fout);
	}
#endif
	//
	signal(SIGTERM, do_quit);
	signal(SIGINT, do_quit);
	signal(SIGQUIT, do_quit);
	//
	if(omx_streamer_start(&omx) < 0) {
		ga_error("streamer: start streamer failed.\n");
		goto quit;
	}
	//
	gettimeofday(&tv1, NULL);
	//
	while(quit_flag == 0) {
		unsigned char *encbuf;
		int encsize;
		if((encbuf = omx_streamer_get(&omx, &encsize)) == NULL) {
			ga_error("streamer: encode failed.\n");
			break;
		}
		if(fout)
			fwrite(encbuf, 1, encsize, fout);
	}
	gettimeofday(&tv2, NULL);
	elapsed = (tv2.tv_sec - tv1.tv_sec)*1000000 + (tv2.tv_usec - tv1.tv_usec);
	ga_error("streaming captured %d frames in %.4f seconds (%.4f fps).\n",
		omx.frame_out,
		0.000001 * elapsed,
		1.0 * omx.frame_out / (0.000001 * elapsed));
	//
	if(omx_streamer_prepare_stop(&omx) < 0) {
		ga_error("streamer: prepare stop failed.\n");
	}
	//
	if(omx_streamer_stop(&omx) < 0) {
		ga_error("streamer: start streamer failed.\n");
	}
quit:
	// finalize
	if(fout)	fclose(fout);
	omx_streamer_deinit(&omx);
	OMX_Deinit();
	//
	return 0;
}
#endif /* ! GA_MODULE */

