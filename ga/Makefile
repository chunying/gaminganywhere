
# Makefile for POSIX-based OSes

TARGET	= core module server client

.PHONY: $(TARGET)

all: $(TARGET)

core:
	make -C core

module: core
	make -C module

server: core
	make -C server

client: core
	make -C client

install:
	mkdir -p ../bin
	make -C core install
	make -C module install
	make -C server install
	make -C client install
	cp -fr config ../bin/

clean:
	make -C core clean
	make -C module clean
	make -C server clean
	make -C client clean

