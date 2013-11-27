@echo off
@REM
@set GADEPS=..\deps.win32
echo Create directories in %GADEPS%
@REM mkdir %GADEPS%
mkdir %GADEPS%\bin
mkdir %GADEPS%\lib
mkdir %GADEPS%\include
mkdir %GADEPS%\include\SDL2
mkdir %GADEPS%\include\live555
@REM
echo Installing stdint headers ...
bin\7za x -y msinttypes-r26.zip *.h
move /y *.h %GADEPS%\include
@REM
echo Installing ffmpeg ...
set FFMPEG=2.1.1
bin\7za x -y ffmpeg-%FFMPEG%-win32-shared.7z
move /y ffmpeg-%FFMPEG%-win32-shared\bin\* %GADEPS%\bin\
rmdir /s /q ffmpeg-%FFMPEG%-win32-shared
@REM
bin\7za x -y ffmpeg-%FFMPEG%-win32-dev.7z
move /y ffmpeg-%FFMPEG%-win32-dev\lib\*.lib %GADEPS%\lib\
xcopy /e /q /h /r /y ffmpeg-%FFMPEG%-win32-dev\include\* %GADEPS%\include\
rmdir /s /q ffmpeg-%FFMPEG%-win32-dev
@REM
@REM echo Installing SDL ...
@REM @REM set SDL=20130130
@REM set SDL=20130219
@REM bin\7za x -y SDL-devel-%SDL%-VC.zip
@REM move /y SDL-%SDL%\lib\x86\*.dll %GADEPS%\bin\
@REM move /y SDL-%SDL%\include\*.h %GADEPS%\include\SDL2\
@REM move /y SDL-%SDL%\lib\x86\*.lib %GADEPS%\lib\
@REM rmdir /s /q SDL-%SDL%
@REM
@REM
echo Installing SDL2 ...
set SDL2=2.0.1
bin\7za x -y SDL2-devel-%SDL2%-VC.zip
move /y SDL2-%SDL2%\lib\x86\*.dll %GADEPS%\bin\
move /y SDL2-%SDL2%\include\*.h %GADEPS%\include\SDL2\
move /y SDL2-%SDL2%\lib\x86\*.lib %GADEPS%\lib\
rmdir /s /q SDL2-%SDL2%
@REM
echo Installing SDL2_ttf ...
set SDL2TTF=2.0.12
bin\7za x -y SDL2_ttf-devel-%SDL2TTF%-VC.zip
move /y SDL2_ttf-%SDL2TTF%\lib\x86\*.dll %GADEPS%\bin\
move /y SDL2_ttf-%SDL2TTF%\include\*.h %GADEPS%\include\SDL2\
move /y SDL2_ttf-%SDL2TTF%\lib\x86\*.lib %GADEPS%\lib\
rmdir /s /q SDL2_ttf-%SDL2TTF%
@REM
echo Installing pthreads ...
bin\7za x pthreads-w32-2-9-1-release.zip Pre-built.2
move /y Pre-built.2\dll\x86\pthread*.dll %GADEPS%\bin\
move /y Pre-built.2\include\*.h %GADEPS%\include\
move /y Pre-built.2\lib\x86\*.lib %GADEPS%\lib\
rmdir /s /q Pre-built.2
@REM
echo Installing live555 ...
@REM bin\7za x live.2012.05.17-bin.zip
bin\7za x live.2013.04.30-bin.zip
move /y live555\include\*.* %GADEPS%\include\live555\
move /y live555\lib\*.lib %GADEPS%\lib\
rmdir /s /q live555
@REM
echo Installing detour library ...
bin\7za x detour.7z
move /y detour\*.h %GADEPS%\include\
move /y detour\*.lib %GADEPS%\lib\
move /y detour\*.dll %GADEPS%\bin\
rmdir /s /q detour
@REM
echo Installation finished
pause
