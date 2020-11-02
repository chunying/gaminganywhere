
include ../Makefile.common

ifeq ($(OS), Linux)
CFLAGS	+= -I.. $(X11CF)
LDFLAGS	+= $(X11LD)
OBJS	= vsource-desktop.o ga-xwin.o
endif

ifeq ($(OS), MSYS)
LDFLAGS	+= ../../core/libga.dll -lole32 -lgdi32
OBJS	= vsource-desktop.o ga-win32-common.o ga-win32-gdi.o
endif

ifeq ($(OS), Darwin)
LDFLAGS	+= -framework Cocoa
OBJS	= vsource-desktop.o ga-osx.o
endif

TARGET	= vsource-desktop.$(EXT)

include ../Makefile.build

