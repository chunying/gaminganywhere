
# you may need libva-dev i965-va-driver on intel platform

include Makefile.common

TARGET	= asource-system vsource-desktop filter-rgb2yuv \
	  encoder-video encoder-x264 encoder-audio ctrl-sdl \
	  server-ffmpeg server-live555

ifeq ($(shell uname -s),Linux)
TARGET	+= asource-alsa asource-pulseaudio
endif

all:
	for t in $(TARGET); do make -C $$t || exit 1; done

install:
	-mkdir -p ../../bin/mod
	find . -name '*.$(EXT)' -exec cp -f {} ../../bin/mod \;

clean:
	for t in $(TARGET); do make -C $$t clean; done

