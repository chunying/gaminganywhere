
include ../Makefile.common

ifeq ($(OS), Linux)
CFLAGS	+= $(X11CF)
LDFLAGS	+= $(X11LD)
endif

ifeq ($(OS), MSYS)
LDFLAGS	+= ../../core/libga.dll -lws2_32 
endif

OBJS	= ctrl-sdl.o
TARGET	= ctrl-sdl.$(EXT)

include ../Makefile.build

