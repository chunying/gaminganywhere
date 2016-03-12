////////////////////////////////////////////////////////////////////////////
//
// Copyright 1993-2014 NVIDIA Corporation.  All rights reserved.
//
// Please refer to the NVIDIA end user license agreement (EULA) associated
// with this source code for terms and conditions that govern your use of
// this software. Any use, reproduction, disclosure, or distribution of
// this software and related documentation outside the terms of the EULA
// is strictly prohibited.
//
////////////////////////////////////////////////////////////////////////////

#include<string>
#include "NvEncoderLowLatency.h"
#include "nvUtils.h"
#include "nvFileIO.h"
#include "helper_string.h"

#if defined(__x86_64) || defined(AMD64) || defined(_M_AMD64) || defined(__aarch64__)
#define PTX_FILE "preproc64_lowlat.ptx"
#else
#define PTX_FILE "preproc32_lowlat.ptx"
#endif
using namespace std;

#if defined(NV_WINDOWS)
#pragma warning(disable : 4996)
#endif

#define BITSTREAM_BUFFER_SIZE 2 * 1024 * 1024

CNvEncoderLowLatency::CNvEncoderLowLatency()
{
    m_pNvHWEncoder = new CNvHWEncoder;

    m_cuContext = NULL;
    m_cuModule = NULL;
    m_cuInterleaveUVFunction = NULL;

    m_NumNvEncCommands = 0;
    m_CurEncCommandIdx = 0;

    m_uEncodeBufferCount = 0;
    memset(&m_stEncoderInput, 0, sizeof(m_stEncoderInput));
    memset(&m_stEOSOutputBfr, 0, sizeof(m_stEOSOutputBfr));

    memset(&m_stEncodeBuffer, 0, sizeof(m_stEncodeBuffer));

    memset(m_ChromaDevPtr, 0, sizeof(m_ChromaDevPtr));
    memset(m_nvEncCommands, 0, sizeof(m_nvEncCommands));

    m_qpHandle = NULL;
    m_qpDeltaMapArray = NULL;
    m_qpDeltaMapArraySize = 0;
}

CNvEncoderLowLatency::~CNvEncoderLowLatency()
{
    if (m_pNvHWEncoder)
    {
        delete m_pNvHWEncoder;
        m_pNvHWEncoder = NULL;
    }

    if (m_qpHandle)
    {
        fclose(m_qpHandle);
        m_qpHandle = NULL;
    }

    if (m_qpDeltaMapArray)
    {
        free(m_qpDeltaMapArray);
        m_qpDeltaMapArray = NULL;
    }
}

CUtexref CNvEncoderLowLatency::InitTexture(CUmodule hModule, const char *name, CUarray_format fmt, int NumPackedComponents, unsigned int Flags)
{
    CUresult result;
    CUtexref texref = NULL;

    result = cuModuleGetTexRef(&texref, hModule, name);
    if (result != CUDA_SUCCESS)
    {
        PRINTERR("cuModuleGetTexRef: %d\n", result);
        return NULL;
    }
    result = cuTexRefSetFormat(texref, fmt, NumPackedComponents);
    if (result != CUDA_SUCCESS)
    {
        PRINTERR("cuTexRefSetFormat: %d\n", result);
        return NULL;
    }
    result = cuTexRefSetFlags(texref, Flags);
    if (result != CUDA_SUCCESS)
    {
        PRINTERR("cuTexRefSetFlags: %d\n", result);
        return NULL;
    }
    return texref;
}

NVENCSTATUS CNvEncoderLowLatency::InitCuda(uint32_t deviceID, const char *exec_path)
{
    CUresult        cuResult = CUDA_SUCCESS;
    CUdevice        cuDevice = 0;
    CUcontext       cuContextCurr;
    int  deviceCount = 0;
    int  SMminor = 0, SMmajor = 0;

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
    typedef HMODULE CUDADRIVER;
#else
    typedef void *CUDADRIVER;
#endif
    CUDADRIVER hHandleDriver = 0;

    // CUDA interfaces
    __cu(cuInit(0, __CUDA_API_VERSION, hHandleDriver));

    __cu(cuDeviceGetCount(&deviceCount));
    if (deviceCount == 0)
    {
        return NV_ENC_ERR_NO_ENCODE_DEVICE;
    }

    if (deviceID > (unsigned int)deviceCount - 1)
    {
        PRINTERR("Invalid Device Id = %d\n", deviceID);
        return NV_ENC_ERR_INVALID_ENCODERDEVICE;
    }

    // Now we get the actual device
    __cu(cuDeviceGet(&cuDevice, deviceID));

    __cu(cuDeviceComputeCapability(&SMmajor, &SMminor, deviceID));
    if (((SMmajor << 4) + SMminor) < 0x30)
    {
        PRINTERR("GPU %d does not have NVENC capabilities exiting\n", deviceID);
        return NV_ENC_ERR_NO_ENCODE_DEVICE;
    }

    // Create the CUDA Context and Pop the current one
    __cu(cuCtxCreate(&m_cuContext, 0, cuDevice));

    // in this branch we use compilation with parameters
    const unsigned int jitNumOptions = 3;
    CUjit_option *jitOptions = new CUjit_option[jitNumOptions];
    void **jitOptVals = new void *[jitNumOptions];

    // set up size of compilation log buffer
    jitOptions[0] = CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES;
    int jitLogBufferSize = 1024;
    jitOptVals[0] = (void *)(size_t)jitLogBufferSize;

    // set up pointer to the compilation log buffer
    jitOptions[1] = CU_JIT_INFO_LOG_BUFFER;
    char *jitLogBuffer = new char[jitLogBufferSize];
    jitOptVals[1] = jitLogBuffer;

    // set up pointer to set the Maximum # of registers for a particular kernel
    jitOptions[2] = CU_JIT_MAX_REGISTERS;
    int jitRegCount = 32;
    jitOptVals[2] = (void *)(size_t)jitRegCount;

	string ptx_source;
	//char *ptx_path = sdkFindFilePath(PTX_FILE, exec_path);
	char *ptx_path;		// PATH:  <ga_root>/mod/data/preproc32_lowlat.ptx
	sprintf(ptx_path, "%sdata/preproc32_lowlat.ptx", exec_path); //Jiaming Zhang
	if (ptx_path == NULL) {
		PRINTERR("Unable to find ptx file path %s\n", PTX_FILE);
		return NV_ENC_ERR_INVALID_PARAM;
	}
	FILE *fp = fopen(ptx_path, "rb");
    if (!fp)
    {
        PRINTERR("Unable to read ptx file %s\n", PTX_FILE);
        return NV_ENC_ERR_INVALID_PARAM;
    }
    fseek(fp, 0, SEEK_END);
    int file_size = ftell(fp);
    char *buf = new char[file_size + 1];
    fseek(fp, 0, SEEK_SET);
    fread(buf, sizeof(char), file_size, fp);
    fclose(fp);
    buf[file_size] = '\0';
    ptx_source = buf;
    delete[] buf;

    cuResult = cuModuleLoadDataEx(&m_cuModule, ptx_source.c_str(), jitNumOptions, jitOptions, (void **)jitOptVals);
    if (cuResult != CUDA_SUCCESS)
    {
        return NV_ENC_ERR_OUT_OF_MEMORY;
    }

    delete[] jitOptions;
    delete[] jitOptVals;
    delete[] jitLogBuffer;

    __cu(cuModuleGetFunction(&m_cuInterleaveUVFunction, m_cuModule, "InterleaveUV"));
    __cu(cuModuleGetFunction(&m_cuScaleNV12Function, m_cuModule, "Scale_Bilinear_NV12"));

    m_texLuma2D = InitTexture(m_cuModule, "luma_tex", CU_AD_FORMAT_UNSIGNED_INT8, 1);
    m_texChroma2D = InitTexture(m_cuModule, "chroma_tex", CU_AD_FORMAT_UNSIGNED_INT8, 2);

    __cu(cuCtxPopCurrent(&cuContextCurr));
    return NV_ENC_SUCCESS;
}

NVENCSTATUS CNvEncoderLowLatency::AllocateIOBuffers(uint32_t uInputWidth, uint32_t uInputHeight)
{
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

    m_EncodeBufferQueue.Initialize(m_stEncodeBuffer, m_uEncodeBufferCount);

    CCudaAutoLock cuLock(m_cuContext);

    __cu(cuMemAlloc(&m_ChromaDevPtr[0], uInputWidth*uInputHeight / 4));
    __cu(cuMemAlloc(&m_ChromaDevPtr[1], uInputWidth*uInputHeight / 4));

    __cu(cuMemAllocHost((void **)&m_yuv[0], uInputWidth*uInputHeight));
    __cu(cuMemAllocHost((void **)&m_yuv[1], uInputWidth*uInputHeight / 4));
    __cu(cuMemAllocHost((void **)&m_yuv[2], uInputWidth*uInputHeight / 4));

    for (uint32_t i = 0; i < m_uEncodeBufferCount; i++)
    {
        __cu(cuMemAllocPitch(&m_stEncodeBuffer[i].stInputBfr.pNV12devPtr, (size_t *)&m_stEncodeBuffer[i].stInputBfr.uNV12Stride, uInputWidth, uInputHeight * 3 / 2, 16));
        __cu(cuMemAllocPitch(&m_stEncodeBuffer[i].stInputBfr.pNV12TempdevPtr, (size_t *)&m_stEncodeBuffer[i].stInputBfr.uNV12TempStride, uInputWidth, uInputHeight * 3 / 2, 16));

        nvStatus = m_pNvHWEncoder->NvEncRegisterResource(NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR, (void*)m_stEncodeBuffer[i].stInputBfr.pNV12devPtr,
            uInputWidth, uInputHeight, m_stEncodeBuffer[i].stInputBfr.uNV12Stride, &m_stEncodeBuffer[i].stInputBfr.nvRegisteredResource);
        if (nvStatus != NV_ENC_SUCCESS)
            return nvStatus;

        m_stEncodeBuffer[i].stInputBfr.bufferFmt = NV_ENC_BUFFER_FORMAT_NV12_PL;
        m_stEncodeBuffer[i].stInputBfr.dwWidth = uInputWidth;
        m_stEncodeBuffer[i].stInputBfr.dwHeight = uInputHeight;

        nvStatus = m_pNvHWEncoder->NvEncCreateBitstreamBuffer(BITSTREAM_BUFFER_SIZE, &m_stEncodeBuffer[i].stOutputBfr.hBitstreamBuffer);
        if (nvStatus != NV_ENC_SUCCESS)
            return nvStatus;
        m_stEncodeBuffer[i].stOutputBfr.dwBitstreamBufferSize = 0xc000;

#if defined(NV_WINDOWS)
        m_pNvHWEncoder->NvEncRegisterAsyncEvent(&m_stEncodeBuffer[i].stOutputBfr.hOutputEvent);
        if (nvStatus != NV_ENC_SUCCESS)
            return nvStatus;
        m_stEncodeBuffer[i].stOutputBfr.bWaitOnEvent = true;
#else
        m_stEncodeBuffer[i].stOutputBfr.hOutputEvent = NULL;
#endif
    }

    m_stEOSOutputBfr.bEOSFlag = TRUE;
#if defined(NV_WINDOWS)
    nvStatus = m_pNvHWEncoder->NvEncRegisterAsyncEvent(&m_stEOSOutputBfr.hOutputEvent);
    if (nvStatus != NV_ENC_SUCCESS)
        return nvStatus;
#else
    m_stEOSOutputBfr.hOutputEvent = NULL;
#endif

    return nvStatus;
}

NVENCSTATUS CNvEncoderLowLatency::ReleaseIOBuffers()
{
    NVENCSTATUS nvStatus;

    CCudaAutoLock cuLock(m_cuContext);

    for (int i = 0; i < 3; i++)
    {
        if (m_yuv[i])
        {
            cuMemFreeHost(m_yuv[i]);
        }
    }

    for (uint32_t i = 0; i < m_uEncodeBufferCount; i++)
    {
        cuMemFree(m_stEncodeBuffer[i].stInputBfr.pNV12devPtr);
        cuMemFree(m_stEncodeBuffer[i].stInputBfr.pNV12TempdevPtr);
        
        nvStatus = m_pNvHWEncoder->NvEncDestroyBitstreamBuffer(m_stEncodeBuffer[i].stOutputBfr.hBitstreamBuffer);
        if (nvStatus != NV_ENC_SUCCESS)
            return nvStatus;
        m_stEncodeBuffer[i].stOutputBfr.hBitstreamBuffer = NULL;

#if defined(NV_WINDOWS)
        nvStatus = m_pNvHWEncoder->NvEncUnregisterAsyncEvent(m_stEncodeBuffer[i].stOutputBfr.hOutputEvent);
        if (nvStatus != NV_ENC_SUCCESS)
            return nvStatus;
        nvCloseFile(m_stEncodeBuffer[i].stOutputBfr.hOutputEvent);
        m_stEncodeBuffer[i].stOutputBfr.hOutputEvent = NULL;
#endif
    }

    if (m_stEOSOutputBfr.hOutputEvent)
    {
#if defined(NV_WINDOWS)
        nvStatus = m_pNvHWEncoder->NvEncUnregisterAsyncEvent(m_stEOSOutputBfr.hOutputEvent);
        if (nvStatus != NV_ENC_SUCCESS)
            return nvStatus;
        nvCloseFile(m_stEOSOutputBfr.hOutputEvent);
        m_stEOSOutputBfr.hOutputEvent = NULL;
#endif
    }

    return NV_ENC_SUCCESS;
}

NVENCSTATUS CNvEncoderLowLatency::FlushEncoder()
{
    NVENCSTATUS nvStatus = m_pNvHWEncoder->NvEncFlushEncoderQueue(m_stEOSOutputBfr.hOutputEvent);
    if (nvStatus != NV_ENC_SUCCESS)
    {
        assert(0);
        return nvStatus;
    }

    EncodeBuffer *pEncodeBuffer = m_EncodeBufferQueue.GetPending();
    while (pEncodeBuffer)
    {
        m_pNvHWEncoder->ProcessOutput(pEncodeBuffer);
        pEncodeBuffer = m_EncodeBufferQueue.GetPending();
        // UnMap the input buffer after frame is done
        if (pEncodeBuffer && pEncodeBuffer->stInputBfr.hInputSurface)
        {
            nvStatus = m_pNvHWEncoder->NvEncUnmapInputResource(pEncodeBuffer->stInputBfr.hInputSurface);
            pEncodeBuffer->stInputBfr.hInputSurface = NULL;
        }
    }
#if defined(NV_WINDOWS)
    if (WaitForSingleObject(m_stEOSOutputBfr.hOutputEvent, 500) != WAIT_OBJECT_0)
    {
        assert(0);
        nvStatus = NV_ENC_ERR_GENERIC;
    }
#endif
    return nvStatus;
}

NVENCSTATUS CNvEncoderLowLatency::Deinitialize()
{
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

    ReleaseIOBuffers();

    m_pNvHWEncoder->NvEncDestroyEncoder();

    __cu(cuCtxDestroy(m_cuContext));

    return nvStatus;
}

NVENCSTATUS loadframe(uint8_t *yuvInput[3], HANDLE hInputYUVFile, uint32_t frmIdx, uint32_t width, uint32_t height, uint32_t &numBytesRead)
{
    uint64_t fileOffset;
    uint32_t result;
    uint32_t dwInFrameSize = width*height + (width*height) / 2;
    fileOffset = (uint64_t)dwInFrameSize * frmIdx;
    result = nvSetFilePointer64(hInputYUVFile, fileOffset, NULL, FILE_BEGIN);
    if (result == INVALID_SET_FILE_POINTER)
    {
        return NV_ENC_ERR_INVALID_PARAM;
    }
    nvReadFile(hInputYUVFile, yuvInput[0], width * height, &numBytesRead, NULL);
    nvReadFile(hInputYUVFile, yuvInput[1], width * height / 4, &numBytesRead, NULL);
    nvReadFile(hInputYUVFile, yuvInput[2], width * height / 4, &numBytesRead, NULL);

    return NV_ENC_SUCCESS;
}

void PrintHelp()
{
    printf("Usage : NvEncoderLowlatency \n"
        "-i <string>                  Specify input yuv 420 file\n"
        "-o <string>                  Specify output bitstream file\n"
        "-size <int int>              Specify input resolution <width height>\n"
        "-maxSize <int int>           Specify maximum resolution <maxWidth maxHeight>\n"
        "\n### Optional parameters ###\n"
        "-startf <integer>            Specify start index for encoding. Default is 0\n"
        "-endf <integer>              Specify end index for encoding. Default is end of file\n"
        "-codec <integer>             Specify the codec \n"
        "                                 0: H264\n"
        "                                 1: HEVC\n"
        "-preset <string>             Specify the preset for encoder settings\n"
        "                                 hq : nvenc HQ \n"
        "                                 hp : nvenc HP \n"
        "                                 lowLatencyHP : nvenc low latency HP \n"
        "                                 lowLatencyHQ : nvenc low latency HQ \n"
        "                                 lossless : nvenc Lossless HP \n"
        "-fps <integer>               Specify encoding frame rate\n"
        "-bitrate <integer>           Specify the encoding average bitrate\n"
        "-vbvSize <integer>           Specify the encoding vbv/hrd buffer size\n"
        "-rcmode <integer>            Specify the rate control mode\n"
        "                                 0:  Constant QP\n"
        "                                 1:  Single pass VBR\n"
        "                                 2:  Single pass CBR\n"
        "                                 4:  Single pass VBR minQP\n"
        "                                 8:  Two pass frame quality\n"
        "                                 16: Two pass frame size cap\n"
        "                                 32: Two pass VBR\n"
        "-encCmdFile <string>         Specify the encode commands to do be\n"
        "                             applied during the encoding session.\n"
        "                             Encode command can be passed as the \n"
        "                             below format in the encode command file.\n"
        "                             <encode command> <frame number> <param0> <param1>....<param15>\n"
        "                             0: Dynamic resolution change <param0 = new width> <param1 = new height>\n"
        "                             1: Dynamic bitrate change <param0 = new bitrate> <param1 = new vbv size>\n"
        "                             2: Force idr\n"
        "                             3: Force intra refresh <param0 = intra refresh duration> \n"
        "                             4: Invalidate Refrence frame <param0 = ref frame 0> <param0 = ref frame 1> ..<param15 = ref frame 15>\n"
        "-qpDeltaMapFile <string>     Specify the file containing the external QP delta map\n"
        "-intraRefresh <boolean>      Specify if intra refresh is used during encoding.\n"
        "-intraRefreshPeriod <integer>       Specify period for cyclic intra refresh\n"
        "-intraRefreshDuration <integer>     Specify the intra refresh duration\n"
        "-deviceID <integer>            Specify the GPU device on which encoding will take place\n"
        "-help                        Prints Help Information\n\n"
        );
}

int CNvEncoderLowLatency::EncodeMain(int argc, char *argv[])
{
    HANDLE hInput;
    uint32_t numBytesRead = 0;
    unsigned long long lStart, lEnd, lFreq;
    int numFramesEncoded = 0;
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    bool bError = false;
    EncodeBuffer *pEncodeBuffer;
    EncodeConfig encodeConfig;

    memset(&encodeConfig, 0, sizeof(EncodeConfig));

    encodeConfig.endFrameIdx = INT_MAX;
    encodeConfig.bitrate = 5000000;
    encodeConfig.rcMode = NV_ENC_PARAMS_RC_2_PASS_QUALITY;
    encodeConfig.gopLength = NVENC_INFINITE_GOPLENGTH;
    encodeConfig.deviceType = 0;
    encodeConfig.codec = NV_ENC_H264;
    encodeConfig.fps = 30;
    encodeConfig.qp = 28;
    encodeConfig.i_quant_factor = DEFAULT_I_QFACTOR;
    encodeConfig.b_quant_factor = DEFAULT_B_QFACTOR;  
    encodeConfig.i_quant_offset = DEFAULT_I_QOFFSET;
    encodeConfig.b_quant_offset = DEFAULT_B_QOFFSET; 
    encodeConfig.presetGUID = NV_ENC_PRESET_LOW_LATENCY_HQ_GUID;
    encodeConfig.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
    encodeConfig.numB = 0;

    nvStatus = m_pNvHWEncoder->ParseArguments(&encodeConfig, argc, argv);
    if (nvStatus != NV_ENC_SUCCESS)
    {
        PrintHelp();
        return 1;
    }

    if (!encodeConfig.inputFileName || !encodeConfig.outputFileName || encodeConfig.width == 0 || encodeConfig.height == 0)
    {
        PrintHelp();
        return 1;
    }

    if (encodeConfig.numB > 0)
    {
        PRINTERR("B-frames are not supported\n");
        PrintHelp();
        return 1;
    }

    encodeConfig.fOutput = fopen(encodeConfig.outputFileName, "wb");
    if (encodeConfig.fOutput == NULL)
    {
        PRINTERR("Failed to create \"%s\"\n", encodeConfig.outputFileName);
        return 1;
    }

    hInput = nvOpenFile(encodeConfig.inputFileName);
    if (hInput == INVALID_HANDLE_VALUE)
    {
        PRINTERR("Failed to open \"%s\"\n", encodeConfig.inputFileName);
        return 1;
    }

    // initialize D3D
    nvStatus = InitCuda(encodeConfig.deviceID, argv[0]);
    if (nvStatus != NV_ENC_SUCCESS)
        return nvStatus;

    nvStatus = m_pNvHWEncoder->Initialize((void*)m_cuContext, NV_ENC_DEVICE_TYPE_CUDA);
    if (nvStatus != NV_ENC_SUCCESS)
        return 1;

    encodeConfig.presetGUID = m_pNvHWEncoder->GetPresetGUID(encodeConfig.encoderPreset, encodeConfig.codec);
    
    printf("Encoding input           : \"%s\"\n", encodeConfig.inputFileName);
    printf("         output          : \"%s\"\n", encodeConfig.outputFileName);
    if (encodeConfig.encCmdFileName)
    {
        printf("Command File             : %s\n", encodeConfig.encCmdFileName);
    }
    printf("         codec           : \"%s\"\n", encodeConfig.codec == NV_ENC_HEVC ? "HEVC" : "H264");
    printf("         size            : %dx%d\n", encodeConfig.width, encodeConfig.height);
    printf("         bitrate         : %d bits/sec\n", encodeConfig.bitrate);
    printf("         vbvSize         : %d bits\n", encodeConfig.vbvSize);
    printf("         fps             : %d frames/sec\n", encodeConfig.fps);
    if (encodeConfig.intraRefreshEnableFlag)
    {
        printf("IntraRefreshPeriod       : %d\n", encodeConfig.intraRefreshPeriod);
        printf("IntraRefreshDuration     : %d\n", encodeConfig.intraRefreshDuration);
    }
    printf("         rcMode          : %s\n", encodeConfig.rcMode == NV_ENC_PARAMS_RC_CONSTQP ? "CONSTQP" :
                                              encodeConfig.rcMode == NV_ENC_PARAMS_RC_VBR ? "VBR" :
                                              encodeConfig.rcMode == NV_ENC_PARAMS_RC_CBR ? "CBR" :
                                              encodeConfig.rcMode == NV_ENC_PARAMS_RC_VBR_MINQP ? "VBR MINQP" :
                                              encodeConfig.rcMode == NV_ENC_PARAMS_RC_2_PASS_QUALITY ? "TWO_PASS_QUALITY" :
                                              encodeConfig.rcMode == NV_ENC_PARAMS_RC_2_PASS_FRAMESIZE_CAP ? "TWO_PASS_FRAMESIZE_CAP" :
                                              encodeConfig.rcMode == NV_ENC_PARAMS_RC_2_PASS_VBR ? "TWO_PASS_VBR" : "UNKNOWN");
    printf("         preset          : %s\n", (encodeConfig.presetGUID == NV_ENC_PRESET_LOW_LATENCY_HQ_GUID) ? "LOW_LATENCY_HQ" :
                                              (encodeConfig.presetGUID == NV_ENC_PRESET_LOW_LATENCY_HP_GUID) ? "LOW_LATENCY_HP" :
                                              (encodeConfig.presetGUID == NV_ENC_PRESET_HQ_GUID) ? "HQ_PRESET" :
                                              (encodeConfig.presetGUID == NV_ENC_PRESET_HP_GUID) ? "HP_PRESET" :
                                              (encodeConfig.presetGUID == NV_ENC_PRESET_LOSSLESS_HP_GUID) ? "LOSSLESS_HP" :
                                              (encodeConfig.presetGUID == NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID) ? "LOW_LATENCY_DEFAULT" : "DEFAULT");
    printf("\n");

    if (encodeConfig.encCmdFileName)
    {
        ParseEncodeCommandFile(encodeConfig.encCmdFileName);
        for (uint32_t i = 0; i < m_NumNvEncCommands; i++)
        {
            if (m_nvEncCommands[i].nvEncCommand == NV_ENC_FORCE_INTRA_REFRESH)
                encodeConfig.intraRefreshEnableFlag = 1;
            if (m_nvEncCommands[i].nvEncCommand == NV_ENC_INVALIDATE_REFRENCE_FRAME)
                encodeConfig.invalidateRefFramesEnableFlag = 1;
        }
    }

    if (encodeConfig.intraRefreshEnableFlag && encodeConfig.invalidateRefFramesEnableFlag)
    {
        PRINTERR("Intra Refresh and Invalidate Refrence Frames cant be enabled simultaneously \n");
        encodeConfig.invalidateRefFramesEnableFlag = 0;
    }

    if (encodeConfig.qpDeltaMapFile)
    {
        m_qpHandle = fopen(encodeConfig.qpDeltaMapFile, "rb");
        if (m_qpHandle == NULL)
        {
            PRINTERR("Invalid external QP Delta Map file: %s\n", encodeConfig.qpDeltaMapFile);
            return NV_ENC_ERR_INVALID_PARAM;
        }

        uint32_t widthInMBs = ((encodeConfig.width + 15) & ~15) >> 4;
        uint32_t heightInMBs = ((encodeConfig.height + 15) & ~15) >> 4;
        m_qpDeltaMapArraySize = widthInMBs * heightInMBs;

        m_qpDeltaMapArray = (int8_t*)malloc(m_qpDeltaMapArraySize * sizeof(int8_t));
        if (m_qpDeltaMapArray == NULL)
        {
            return NV_ENC_ERR_OUT_OF_MEMORY;
        }
        memset(m_qpDeltaMapArray, 0, m_qpDeltaMapArraySize);
    }
    nvStatus = m_pNvHWEncoder->CreateEncoder(&encodeConfig);
    if (nvStatus != NV_ENC_SUCCESS)
    {
        bError = true;
        goto exit;
    }
    m_uEncodeBufferCount = 3;
    nvStatus = AllocateIOBuffers(m_pNvHWEncoder->m_uMaxWidth, m_pNvHWEncoder->m_uMaxHeight);
    if (nvStatus != NV_ENC_SUCCESS)
        return nvStatus;

    NvQueryPerformanceCounter(&lStart);

    for (int frm = encodeConfig.startFrameIdx; frm <= encodeConfig.endFrameIdx; frm++)
    {
        numBytesRead = 0;
        loadframe(m_yuv, hInput, frm, encodeConfig.width, encodeConfig.height, numBytesRead);
        if (numBytesRead == 0)
            break;

        pEncodeBuffer = m_EncodeBufferQueue.GetAvailable();
        if (!pEncodeBuffer)
        {
            pEncodeBuffer = m_EncodeBufferQueue.GetPending();
            m_pNvHWEncoder->ProcessOutput(pEncodeBuffer);
            // UnMap the input buffer after frame done
            if (pEncodeBuffer->stInputBfr.hInputSurface)
            {
                nvStatus = m_pNvHWEncoder->NvEncUnmapInputResource(pEncodeBuffer->stInputBfr.hInputSurface);
                pEncodeBuffer->stInputBfr.hInputSurface = NULL;
            }
            pEncodeBuffer = m_EncodeBufferQueue.GetAvailable();
        }

        NvEncPictureCommand encPicCommand;
        CheckAndInitNvEncCommand(m_pNvHWEncoder->m_EncodeIdx, &encPicCommand);
        nvStatus = m_pNvHWEncoder->NvEncReconfigureEncoder(&encPicCommand);
        if (nvStatus != NV_ENC_SUCCESS)
        {
            assert(0);
            return nvStatus;
        }

        if (encPicCommand.bInvalidateRefFrames)
        {
            nvStatus = ProcessRefFrameInvalidateCommands(&encPicCommand);
        }

        EncodeFrameConfig stEncodeFrame;
        memset(&stEncodeFrame, 0, sizeof(stEncodeFrame));
        stEncodeFrame.yuv[0] = m_yuv[0];
        stEncodeFrame.yuv[1] = m_yuv[1];
        stEncodeFrame.yuv[2] = m_yuv[2];

        stEncodeFrame.stride[0] = encodeConfig.width;
        stEncodeFrame.stride[1] = encodeConfig.width / 2;
        stEncodeFrame.stride[2] = encodeConfig.width / 2;
        stEncodeFrame.width = encodeConfig.width;
        stEncodeFrame.height = encodeConfig.height;

        nvStatus = PreProcessInput(pEncodeBuffer, stEncodeFrame.yuv, stEncodeFrame.width, stEncodeFrame.height,
                                   m_pNvHWEncoder->m_uCurWidth, m_pNvHWEncoder->m_uCurHeight, 
                                   m_pNvHWEncoder->m_uMaxWidth, m_pNvHWEncoder->m_uMaxHeight);
        if (nvStatus != NV_ENC_SUCCESS)
        {
            assert(0);
            return nvStatus;
        }

        nvStatus = m_pNvHWEncoder->NvEncMapInputResource(pEncodeBuffer->stInputBfr.nvRegisteredResource, &pEncodeBuffer->stInputBfr.hInputSurface);
        if (nvStatus != NV_ENC_SUCCESS)
        {
            PRINTERR("Failed to Map input buffer %p\n", pEncodeBuffer->stInputBfr.hInputSurface);
            bError = true;
            goto exit;
        }

        if (m_qpDeltaMapArray)
        {
            memset(m_qpDeltaMapArray, 0, m_qpDeltaMapArraySize);
            size_t numElemsRead = fread(m_qpDeltaMapArray, m_qpDeltaMapArraySize, 1, m_qpHandle);
            if (numElemsRead != 1)
            {
                PRINTERR("Warning: Amount of data read from m_qpHandle is less than requested.\n");
            }
        }

        nvStatus = m_pNvHWEncoder->NvEncEncodeFrame(pEncodeBuffer, &encPicCommand, encodeConfig.width, encodeConfig.height,
                                                    NV_ENC_PIC_STRUCT_FRAME, m_qpDeltaMapArray, m_qpDeltaMapArraySize);
        if (nvStatus != NV_ENC_SUCCESS)
        {
            bError = true;
            goto exit;
        }
        numFramesEncoded++;
    }

    FlushEncoder();

    if (numFramesEncoded > 0)
    {
        NvQueryPerformanceCounter(&lEnd);
        NvQueryPerformanceFrequency(&lFreq);
        double elapsedTime = (double)(lEnd - lStart);
        printf("Encoded %d frames in %6.2fms\n", numFramesEncoded, (elapsedTime*1000.0) / lFreq);
        printf("Avergage Encode Time : %6.2fms\n", ((elapsedTime*1000.0) / numFramesEncoded) / lFreq);
    }

exit:
    if (encodeConfig.fOutput)
    {
        fclose(encodeConfig.fOutput);
    }

    if (hInput)
    {
        nvCloseFile(hInput);
    }

    Deinitialize();

    return bError ? 1 : 0;
}


//int main(int argc, char **argv)
//{
//    CNvEncoderLowLatency nvEncoder;
//    return nvEncoder.EncodeMain(argc, argv);
//}
