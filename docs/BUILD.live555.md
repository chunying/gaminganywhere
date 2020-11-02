# Build ```live555``` Library for Windows

This document briefly describes how to build live555 libraries,
including both release-mode and debug-mode libraries,
for Windows OS.

## On Linux or Mac OS X
Simply build live555 from deps.src.
In ```gaminganywhere/deps.src``` directory,
run the following commands:
```
$ . ../env-setup
$ make
```
You may need to modify ```../env-setup``` script to meet your development
environment.

## On Windows (Visual C++ 2010)

1. Extract live555 source code.
   Also apply patch files ```patches/live555-*```.

2. Modify live/win32config, set ```TOOLS32=/path/to/VC```,
   e.g. ```TOOLS32 = C:\Program Files\Microsoft Visual Studio 10.0\VC```

3. Modify ```live/win32config```,
   ensure ```NODEBUG=1``` is not commented out.

4. Run ```genWindowsMakefiles``` in the ```live``` directory.

5. In ```live``` directory, switch to the corresponding directories
   (```UsageEnvironment```, ```groupsock```, ```liveMedia```, and
   ```BasicUsageEnvironment```) and execute:
   ```
   $ nmake -f UsageEnvironment.mak
   $ nmake -f groupsock.mak
   $ nmake -f liveMedia.mak
   $ nmake -f BasicUsageEnvironment.mak
   ```
   This will generate release-mode libraries.

6. Copy generated libraries to a proper location.

7. Modify ```live/win32config```, comment out ```NODEBUG=1```

8. Repeat steps 4-5 to generate debug-mode libraries.

9. Copy header files from ```live``` to a proper location.

> PS1: To build 64-bit library, set ```C_COMPILER = cl``` and ```RC32 = rc```
(do not hardcode ```PATH```, the VS script will do that for you),
and then compile in x86_64 visual studio command line prompt.

> PS2: Commands for Cygwin or Unix
```
# suppose 'live' dir has source codes,
# and we plan to put required files in 'live555' directory
mkdir -p live555/include
mkdir -p live555/lib
# build release-mode files: use the aforementioned steps
# copy release-mode -library
find live -name '*.lib' -exec cp {} live555/lib/ \;
# clean up
find live -name '*.obj' -exec rm -f {} \;
find live -name '*.lib' -exec rm -f {} \;
# build debug-mode files: use the aforementioned steps
# copy debug-mode library
export LIVELIB='UsageEnvironment groupsock liveMedia BasicUsageEnvironment'
for l in $LIVELIB; do cp live/$l/lib$l.lib live555/lib/lib$l.d.lib; done
unset LIVELIB
# copy headers
find live -name '*.hh' -exec cp -f {} live555/include/ \;
cp live/groupsock/include/NetCommon.h live555/include/
# compress
zip -ur9 live.2014.03.25-bin.zip live555
```
