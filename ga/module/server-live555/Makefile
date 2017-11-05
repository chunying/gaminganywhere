
include ../Makefile.common

CFLAGS	+= -I$(GADEPS)/include/live555
LDFLAGS	+= -L$(GADEPS)/lib -lliveMedia -lBasicUsageEnvironment -lUsageEnvironment -lgroupsock

ifeq ($(OS), MSYS)
LDFLAGS	+= ../../core/libga.dll -lws2_32
endif

OBJS	= server-live555.o ga-liveserver.o ga-mediasubsession.o ga-qossink.o \
	  ga-audiolivesource.o ga-videolivesource.o
TARGET	= server-live555.$(EXT)

include ../Makefile.build

