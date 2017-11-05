
OS	= $(shell uname -s)
MACHINE	= $(shell uname -m)
OS_M	= $(OS).$(MACHINE)

ifeq ($(findstring MSYS,$(OS)),MSYS)
OS	= MSYS
endif

ifeq ($(OS), MSYS)
TARGET	= periodic 
else
TARGET	= periodic event-posix
endif

all:
	for t in $(TARGET); do make -C $$t || exit 1; done

install:
	for t in $(TARGET); do make -C $$t install || exit 1; done

clean:
	for t in $(TARGET); do make -C $$t clean || exit 1; done

