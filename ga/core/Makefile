
include ../Makefile.def

CFLAGS	= -O2 -fPIC -g -Wall -I$(GADEPS)/include $(EXTRACFLAGS) -DPIPELINE_FILTER \
	  $(AVCCF) $(L5CF) $(SDLCF) $(L5CF)
#	-D__STDC_CONSTANT_MACROS
SO_EXT	= .so

ifeq ($(OS), Linux)
CFLAGS	+= $(ASNDCF) $(X11CF)
LDFLAGS	= $(AVCLD) $(L5LD)
endif

ifeq ($(OS), Darwin)
SO_EXT	= .dylib
LDFLAGS	= -install_name @executable_path/libga$(SO_EXT) $(AVCLD) $(L5LD)
endif

TARGET	= libga$(SO_EXT)

all: $(TARGET)

.cpp.o:
	$(CXX) -c -g $(CFLAGS) $<

OBJS =	ga-common.o ga-conf.o ga-confvar.o  ga-module.o ga-avcodec.o \
	ga-videolivesource.o ga-audiolivesource.o ga-mediasubsession.o \
	ga-liveserver.o \
	rtspconf.o pipeline.o vconverter.o \
	vsource.o asource.o encoder-common.o controller.o server.o rtspserver.o

libga.a: $(OBJS)
	ar rc $@ $^

libga$(SO_EXT): $(OBJS)
	$(CXX) -shared -o $@ $^ $(LDFLAGS)

install:
	cp -f libga$(SO_EXT) ../../bin/

clean:
	rm -f $(TARGET) *.o *~

