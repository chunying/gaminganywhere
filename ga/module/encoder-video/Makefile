
include ../Makefile.common

ifeq ($(OS), MSYS)
LDFLAGS	+= ../../core/libga.dll $(AVCLD)
endif

OBJS	= encoder-video.o
TARGET	= encoder-video.$(EXT)

include ../Makefile.build

