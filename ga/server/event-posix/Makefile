
# note: requires libgl1-mesa-dev, libglu1-mesa-dev, libpulse-dev

include ../Makefile.def

CFLAGS	+= -I../../core $(AVCCF) $(L5CF) $(SDLCF)
#LDFLAGS	= -rdynamic -L../../core -Wl,--whole-archive -lga -Wl,--no-whole-archive $(AVCLD) $(SDLLD)
LDFLAGS	= $(EXTRALDFLAGS) -rdynamic -L../../core -lga $(AVCLD) $(SDLLD) -Wl,-rpath,\$$ORIGIN

EXT	= so

MAKEMODULE	= $(CXX) -o $@ -shared $^ $(LDFLAGS) -ldl

ADDOBJ	= 

TARGET	= ga-server-event-driven ga-hook-sdl.$(EXT) ga-hook-sdlaudio.$(EXT) ga-hook-gl.$(EXT) \
	  ga-hook-sdl2.$(EXT) ga-hook-sdl2audio.$(EXT)

ifeq ($(OS), Linux)
CFLAGS	+= $(ASNDCF) $(X11CF)
LDFLAGS	+= $(ASNDLD) $(X11LD) -lGL
ADDOBJ	+= elf_hook.o ga-hook-lib.o
TARGET	+= ga-hook-pulse.$(EXT)
endif

ifeq ($(OS), Darwin)
EXT	= dylib
CFLAGS  += -I/usr/X11/include
#LDFLAGS	= -rdynamic -force_load ../../core/libga.a $(AVCLD) $(SDLLD)
LDFLAGS	+= -framework Cocoa
MAKEMODULE = $(CXX) -o $@ -dynamiclib -Wl,-undefined,dynamic_lookup $^ $(LDFLAGS) -ldl
ADDOBJ	+= mach_hook.o
endif

all: $(TARGET)

.cpp.o:
	$(CXX) -c -g $(CFLAGS) $<

ga-server-event-driven: ga-server-event-driven.o 
	$(CXX) -o $@ $^ $(LDFLAGS)

ga-hook-sdl.$(EXT): $(ADDOBJ) ga-hook-common.o ga-hook-sdl.o ctrl-sdl.o
	$(MAKEMODULE)

ga-hook-sdlaudio.$(EXT): $(ADDOBJ) ga-hook-common.o ga-hook-sdlaudio.o
	$(MAKEMODULE)

ga-hook-sdl2.$(EXT): $(ADDOBJ) ga-hook-common.o ga-hook-sdl2.o ctrl-sdl.o
	$(MAKEMODULE)

ga-hook-sdl2audio.$(EXT): $(ADDOBJ) ga-hook-common.o ga-hook-sdl2audio.o
	$(MAKEMODULE)

ga-hook-gl.$(EXT): $(ADDOBJ) ga-hook-common.o ga-hook-gl.o
	$(MAKEMODULE)

ga-hook-pulse.$(EXT): $(ADDOBJ) ga-hook-pulse.o
	$(MAKEMODULE)

install: $(TARGET)
	mkdir -p ../../../bin
	cp -f $(TARGET) ../../../bin/

clean:
	rm -f $(TARGET) *.o *.so *~

