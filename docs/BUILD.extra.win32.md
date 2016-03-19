# Build Additional Libraries for Windows

This document briefly describes how to build additional
libraries for Windows OS. Currently we only have instructions for
```x264``` library.

> Note : In this document, a prompt of ```$``` means that
the command is launched in ```bash```.
In contrast, a prompt of ```>``` means that the command is
launched in Windows command line
(with Visual Studio configured environment).

## Recommended Approach

1. Install ```Cygwin```, with the following additional packages
   ```
   mingw64-i686-gcc-g++
   mingw64-i686-pthreads
   mingw64-x86_64-gcc-g++
   mingw64-x86_64-pthreads
   ```

2. Build ```yasm```
   ```
   $ ./configure --prefix=/usr/local
   $ make all
   $ make install
   ```
3. Build ```x264```: In ```x264``` source directory
   ```
   $ mkdir ../x264
   ```
4. Build x86 files first
   ```
   $ ./configure --prefix=../x264/x86 \
		--host=i686-w64-mingw32 \
		--cross-prefix=i686-w64-mingw32- \
		--enable-win32thread \
		--enable-shared --extra-ldflags=-Wl,--output-def=libx264.def
   $ mv config.mak config.mak.bak
   $ cat config.mak.bak | sed -e 's/^SONAME=.*/SONAME=libx264.dll/' > config.mak
   $ make all install
   $ cp ./libx264.def ../x264/x86/lib/
   > lib /machine:x86 /def:libx264.def	# generates libx264.lib
   ```

5. Clean up and build again to generate x86_64 files
   ```
   $ ./configure --prefix=../x264/x64 \
		--host=x86_64-w64-mingw32 \
		--cross-prefix=x86_64-w64-mingw32- \
		--enable-win32thread \
		--enable-shared --extra-ldflags=-Wl,--output-def=libx264.def
   $ mv config.mak config.mak.bak
   $ cat config.mak.bak | sed -e 's/^SONAME=.*/SONAME=libx264.dll/' > config.mak
   $ make all install
   $ cp ./libx264.def ../x264/x64/lib/
   > lib /machine:x64 /def:libx264.def	# generates libx264.lib
   ```

6. Package the library (files in the ```../x264``` directory)
   ```
   $ cd ..
   $ zip -ur9 x264-XXXXX.zip x264
   ```

## Old Approach

1. Install MinGW
  * Package selection:
    ```mingw-developer-toolkit```, ````mingw32-base```,
    ```mingw32-gcc-g++```, ```msys-base```
  * Additional packages: ```msys-zip```

2. Build ```yasm```: In ```yasm``` source code directory:
   ```
   $ ./configure
   $ make
   $ copy *.exe /path/to/MinGW/bin
   ```

3. Build x264: In ```x264``` source code directory:
   ```
   $ ./configure --prefix=../x264bin --enable-shared \
		--extra-ldflags=-Wl,--output-def=libx264.def
   $ make all install
   # You will have libx264.dll and libx264.def
   > lib /def:libx264.def
   #  You will then have libx264.lib
   > copy libx264.lib ..\x264bin\lib
   ```

4. Package the library (in the ```..``` directory)
   ```
   $ zip -ur9 x264-XXXXX.zip x264bin
   ```
