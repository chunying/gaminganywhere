
include ../Makefile.common

CFLAGS	+= -DHAVE_LIBOPENMAX=2 -DOMX -DOMX_SKIP64BIT -DUSE_EXTERNAL_OMX -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST \
	   -DUSE_VCHIQ_ARM \
	   -I../ctrl-sdl \
	   -I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux
LDFLAGS	+= -L/opt/vc/lib -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -lpthread -lrt

OBJS	= streamer-omx.o omx-streamer.o
TARGET	= streamer-omx.$(EXT)

include ../Makefile.build

