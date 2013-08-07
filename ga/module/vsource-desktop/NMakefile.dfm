
!include <..\NMakefile.common>

CXX_FLAGS = $(CXX_FLAGS) -DDFM_CAPTURE
LIBS	= $(LIBS) advapi32.lib
OBJS	= vsource-desktop-dfm.obj ga-win32-common.obj ga-win32-dfm.obj
TARGET	= vsource-desktop-dfm.$(EXT)

!include <..\NMakefile.build>

