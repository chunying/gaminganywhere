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

#include <Cocoa/Cocoa.h>
#include <ApplicationServices/ApplicationServices.h>
#include <Foundation/Foundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/Graphics/IOGraphicsLib.h>

#include "ga-osx.h"

static CGDirectDisplayID displayID;
static CGContextRef cgctx;
static CGRect rect;
static int imagesize = 0;
static void *imgData = NULL;
static struct gaImage gaimage;

static CGContextRef
CreateARGBBitmapContext(CGImageRef inImage) {
	CGContextRef    context = NULL;
	CGColorSpaceRef colorSpace;
	void *          bitmapData;
	int             bitmapByteCount;
	int             bitmapBytesPerRow;

	size_t pixelsWide = CGImageGetWidth(inImage);
	size_t pixelsHigh = CGImageGetHeight(inImage);

	bitmapBytesPerRow   = (pixelsWide * 4);
	bitmapByteCount     = (bitmapBytesPerRow * pixelsHigh);

	colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
	if (colorSpace == NULL) {
		fprintf(stderr, "Error allocating color space\n");
		return NULL;
	}

	bitmapData = malloc( bitmapByteCount );
	if (bitmapData == NULL) {
		fprintf (stderr, "Memory not allocated!");
		CGColorSpaceRelease( colorSpace );
		return NULL;
	}

	context = CGBitmapContextCreate (bitmapData,
			pixelsWide,
			pixelsHigh,
			8,      // bits per component
			bitmapBytesPerRow,
			colorSpace,
			kCGImageAlphaPremultipliedLast);
	if (context == NULL) {
		free (bitmapData);
		fprintf (stderr, "Context not created!");
	}

	CGColorSpaceRelease( colorSpace );

	return context;
}

int
ga_osx_init(struct gaImage *image) {
	CGImageRef screen;
	//
	displayID = CGMainDisplayID();
	screen = CGDisplayCreateImage(displayID);
	if((cgctx = CreateARGBBitmapContext(screen)) == NULL) {
		CGImageRelease(screen);
		return -1;
	}
	image->width = CGImageGetWidth(screen);
	image->height = CGImageGetHeight(screen);
	image->bytes_per_line =
		(CGImageGetBitsPerPixel(screen)>>3) * image->width;
	bcopy(image, &gaimage, sizeof(gaimage));
	//
	CGRect myrect = { {0, 0}, {image->width, image->height} };
	rect = myrect;
	imagesize = image->height * image->bytes_per_line;
	//
	CGImageRelease(screen);
	//
	return 0;
}

void
ga_osx_deinit() {
	CGContextRelease(cgctx);
	if(imgData) {
		free(imgData);
	}
	return;
}

int
ga_osx_capture(char *buf, int buflen, struct gaRect *grect) {
	CGImageRef screen;
	void *data;
	//
	if(grect == NULL && buflen < imagesize)
		return -1;
	if(grect != NULL && buflen < grect->size)
		return -1;
	//
	screen = CGDisplayCreateImage(displayID);
	CGContextDrawImage(cgctx, rect, screen);
	data = CGBitmapContextGetData(cgctx);
	if(data != NULL) {
		if(grect == NULL) {
			bcopy(data, buf, imagesize);
		} else {
			int i;
			char *src, *dst;
			src = (char *) data;
			src += gaimage.bytes_per_line * grect->top;
			src += RGBA_SIZE * grect->left;
			dst = (char*) buf;
			//
			for(i = 0; i < grect->height; i++) {
				bcopy(src, dst, grect->linesize);
				src += gaimage.bytes_per_line;
				dst += grect->linesize;
			}
		}
	}
	CGImageRelease(screen);
	//
	return grect == NULL ? imagesize : grect->size;
}

