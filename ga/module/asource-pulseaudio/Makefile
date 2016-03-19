
include ../Makefile.common

ifeq ($(OS), Linux)
CFLAGS	+= $(shell pkg-config --cflags libpulse-simple)
LDFLAGS	+= $(shell pkg-config --libs libpulse-simple)
OBJS	= asource-pulseaudio.o
endif

ifeq ($(OS), Darwin)
# does not support audio now
endif

TARGET	= asource-pulseaudio.$(EXT)

include ../Makefile.build

