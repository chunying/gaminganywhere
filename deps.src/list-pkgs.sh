#!/bin/sh

cat Makefile.packages | awk -F= '{print $2}' | colrm 1 1 | grep '\.tar\.'

