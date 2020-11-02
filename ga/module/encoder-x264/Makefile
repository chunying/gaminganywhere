
include ../Makefile.common

ifeq ($(OS), MSYS)
LDFLAGS	+= ../../core/libga.dll $(AVCLD)
endif

CFLAGS	+= $(shell pkg-config --cflags x264)
LDFLAGS	+= $(shell pkg-config --libs x264)

OBJS	= encoder-x264.o
TARGET	= encoder-x264.$(EXT)

include ../Makefile.build

