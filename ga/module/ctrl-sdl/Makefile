
include ../Makefile.common

ifeq ($(OS), Linux)
CFLAGS	+= $(X11CF)
LDFLAGS	+= $(X11LD)
endif

OBJS	= ctrl-sdl.o
TARGET	= ctrl-sdl.$(EXT)

include ../Makefile.build

