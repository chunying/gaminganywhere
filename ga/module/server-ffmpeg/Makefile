
include ../Makefile.common

CFLAGS	+= $(shell pkg-config --cflags libswscale libswresample libpostproc libavdevice libavfilter libavcodec libavformat)
LDFLAGS	+= $(shell pkg-config --libs libswscale libswresample libpostproc libavdevice libavfilter libavcodec libavformat)

OBJS	= server-ffmpeg.o rtspserver.o
TARGET	= server-ffmpeg.$(EXT)

include ../Makefile.build

