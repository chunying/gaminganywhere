
include ../Makefile.common

CFLAGS	+= $(shell pkg-config --cflags libswscale libswresample libpostproc libavdevice libavfilter libavcodec libavformat libavutil)
LDFLAGS	+= $(shell pkg-config --libs libswscale libswresample libpostproc libavdevice libavfilter libavcodec libavformat libavutil)

ifeq ($(OS), MSYS)
LDFLAGS	+= ../../core/libga.dll -lws2_32
endif

OBJS	= server-ffmpeg.o rtspserver.o
TARGET	= server-ffmpeg.$(EXT)

include ../Makefile.build

