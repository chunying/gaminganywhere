
include ../Makefile.common

ifeq ($(OS), Linux)
CFLAGS	+= $(ASNDCF)
LDFLAGS	+= $(ASNDLD)
OBJS	= asource-system.o ga-alsa.o
endif

ifeq ($(OS), MSYS)
LDFLAGS	+= ../../core/libga.dll -lole32 -lksuser
OBJS	= asource-system.o ga-win32-wasapi.o
endif

ifeq ($(OS), Darwin)
# does not support audio now
endif

TARGET	= asource-system.$(EXT)

include ../Makefile.build

