# NVENC Module
This encoder module use [NVIDIA VIDEO CODEC SDK][1].  

## How to use ?
* Add libs `deps.win32\lib\NVENC\*.libs` and include files `deps.win32\include\NVENC\*.h` in your project.  

* Modify `ga-hook-common.cpp`. Then, build and install.
```cpp
	snprintf(module_path, sizeof(module_path),
		//BACKSLASHDIR("%s/mod/encoder-video", "%smod\\encoder-video"),
		BACKSLASHDIR("%s/mod/encoder-nvenc", "%smod\\encoder-nvenc"),
		ga_root);
```

* This module is not in default build, because it is hardware based. So, you should build this module and install seperately.
```
cd ga\module\encoder-nvenc
nmake /f NMakefile all
nmake /f NMakefile install
```


[Jiaming Zhang][2]  
*Sun Yat-Sen University, Guangzhou*


  [1]: https://developer.nvidia.com/nvidia-video-codec-sdk
  [2]: https://github.com/yunyu-Mr
