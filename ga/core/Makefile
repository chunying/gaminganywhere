
include ../Makefile.def

CFLAGS	+= $(AVCCF) $(SDLCF)
SO_EXT	= .so

ifeq ($(HAVE_X11), 1)
CFLAGS	+= -DHAVE_X11
endif

ifeq ($(OS), Linux)
CFLAGS	+= $(ASNDCF) $(X11CF)
LDFLAGS	+= $(EXTRALDFLAGS) $(AVCLD) $(X11LD)
endif

ifeq ($(OS), MSYS)
SO_EXT	= .dll
CFLAGS  += -DGA_LIB
LDFLAGS	+= $(EXTRALDFLAGS) $(AVCLD) $(L5LD) -lws2_32
endif

ifeq ($(OS), Darwin)
SO_EXT	= .dylib
LDFLAGS	+= $(EXTRALDFLAGS) -install_name @executable_path/libga$(SO_EXT) $(AVCLD) $(L5LD)
endif

TARGET	= libga$(SO_EXT)

all: $(TARGET)

.cpp.o:
	$(CXX) -c -g $(CFLAGS) $<

OBJS =	ga-common.o ga-conf.o ga-confvar.o ga-module.o ga-avcodec.o \
	ga-crc.o \
	rtspconf.o dpipe.o vconverter.o \
	vsource.o asource.o encoder-common.o \
	controller.o ctrl-msg.o

ifeq ($(OS), MSYS)
OBJS +=	ga-win32.o
endif

libga.a: $(OBJS)
	$(AR) rc $@ $^

libga$(SO_EXT): $(OBJS)
	$(CXX) -shared -o $@ $^ $(LDFLAGS)

install:
	cp -f libga$(SO_EXT) ../../bin/

clean:
	rm -f $(TARGET) *.o *~

