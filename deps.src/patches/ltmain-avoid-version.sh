#!/bin/sh

if [ -z "$1" ]; then
	echo usage: $0 path-to-ltmain.sh
	exit 1
fi

if [ ! -f "$1" ]; then
	echo file: $1 does not exist.
	exit 1
fi

TMPNAM=/tmp/ltmain-`date '+%s'`.$PPID

cat "$1" | sed -re 's,^( +avoid_version=).*$,\1yes,g' > "$TMPNAM"
mv "$1" "$1.old"
mv "$TMPNAM" "$1"

