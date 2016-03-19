
include ../Makefile.def

CFLAGS	+= -I../../core $(AVCCF) $(L5CF) $(SDLCF)
#LDFLAGS	= -rdynamic -L../../core -Wl,--whole-archive -lga -Wl,--no-whole-archive $(AVCLD) $(SDLLD)
LDFLAGS	+= -rdynamic -L../../core -lga $(AVCLD) $(SDLLD) #-Wl,-rpath,\$$ORIGIN

ifeq ($(OS), Linux)
CFLAGS	+= $(ASNDCF) $(X11CF)
LDFLAGS	+= $(ASNDLD) $(X11LD)
endif

ifeq ($(OS), Darwin)
#LDFLAGS	= -rdynamic -force_load ../../core/libga.a $(AVCLD) $(SDLLD)
LDFLAGS	+= -framework Cocoa
endif

TARGET	= ga-server-periodic

all: $(TARGET)

.cpp.o:
	$(CXX) -c -g $(CFLAGS) $<

ga-server-periodic: ga-server-periodic.o 
	$(CXX) -o $@ $^ $(LDFLAGS)

install: $(TARGET)
	mkdir -p ../../../bin
	cp -f $(TARGET) ../../../bin/

clean:
	rm -f $(TARGET) *.o *~

