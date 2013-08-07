

EXT	= so

ifeq ($(OS), Darwin)
EXT	= dylib
endif

TARGET	= ga-server-event-driven ga-hook-sdl.$(EXT)

all: $(TARGET)

ga-server-event-driven:

ga-hook-sdl.$(EXT):

install: $(TARGET)
	mkdir -p ../../../bin
	cp -f $(TARGET) ../../../bin/

clean:

