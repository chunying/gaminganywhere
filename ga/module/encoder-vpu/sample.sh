#!/bin/sh
g++ -o encoder -DNO_LIBGA -Wall -I/usr/include/imx-mm/vpu vpu-common.cpp -lfslvpuwrap -lvpu
