
include ../Makefile.common

ifeq ($(OS), MSYS)
LDFLAGS	+= ../../core/libga.dll $(AVCLD)
endif

OBJS	= filter-rgb2yuv.o
TARGET	= filter-rgb2yuv.$(EXT)

include ../Makefile.build

