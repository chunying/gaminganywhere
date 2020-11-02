
include ../Makefile.common

ifeq ($(OS), MSYS)
LDFLAGS	+= ../../core/libga.dll $(AVCLD)
endif

OBJS	= encoder-audio.o
TARGET	= encoder-audio.$(EXT)

include ../Makefile.build

