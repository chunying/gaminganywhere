
cl sample.cpp allocator.cpp mfx-common.cpp /MD -GS -W3 -DNO_LIBGA /EHsc "/IC:\Program Files\Intel\Media SDK 2014 R2 for Clients\include" /link /NODEFAULTLIB:LIBCMT "/LIBPATH:C:\Program Files\Intel\Media SDK 2014 R2 for Clients\lib\win32" libmfx.lib advapi32.lib

