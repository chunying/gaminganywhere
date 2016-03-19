# NVENC Module
This encoder module use [NVIDIA VIDEO CODEC SDK][1].  

## How to use ?
Add libs `deps.win32\lib\NVENC\*.libs` and include files `deps.win32\include\NVENC\*.h` in your project.  

Modify `ga-hook-common.cpp`:
```cpp
	snprintf(module_path, sizeof(module_path),
		//BACKSLASHDIR("%s/mod/encoder-video", "%smod\\encoder-video"),
		BACKSLASHDIR("%s/mod/encoder-nvenc", "%smod\\encoder-nvenc"),
		ga_root);
```

*Jiaming Zhang*  
*Sun Yat-Sen University, Guangzhou*


  [1]: https://developer.nvidia.com/nvidia-video-codec-sdk
