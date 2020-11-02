
OS	= $(shell uname -s)
SYS	= $(shell uname -o | tr a-z A-Z)
MACHINE	= $(shell uname -m)
OS_M	= $(OS).$(MACHINE)

ifeq ($(SYS), MSYS)
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

