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

#ifndef __NVENCODERLOWLATENCY_H__
#define __NVENCODERLOWLATENCY_H__

//#include "../common/inc/NvHWEncoder.h"
#include "NvHWEncoder.h"

#define MAX_ENCODE_QUEUE 32
#define MAX_ENC_COMMAND_PARAMS 16
#define MAX_ENC_COMMAND 256

#define SET_VER(configStruct, type) {configStruct.version = type##_VER;}

#define __cu(a) do { CUresult  ret; if ((ret = (a)) != CUDA_SUCCESS) { fprintf(stderr, "%s has returned CUDA error %d\n", #a, ret); return NV_ENC_ERR_GENERIC;}} while(0)

template<class T>
class CNvQueue {
    T** m_pBuffer;
    unsigned int m_uSize;
    unsigned int m_uPendingCount;
    unsigned int m_uAvailableIdx;
    unsigned int m_uPendingndex;
public:
    CNvQueue():  m_pBuffer(NULL), m_uSize(0), m_uPendingCount(0), m_uAvailableIdx(0),
                m_uPendingndex(0)
    {
    }

    ~CNvQueue()
    {
        delete[] m_pBuffer;
    }

    bool Initialize(T *pItems, unsigned int uSize)
    {
        m_uSize = uSize;
        m_uPendingCount = 0;
        m_uAvailableIdx = 0;
        m_uPendingndex = 0;
        m_pBuffer = new T *[m_uSize];
        for (unsigned int i = 0; i < m_uSize; i++)
        {
            m_pBuffer[i] = &pItems[i];
        }
        return true;
    }

    T * GetAvailable()
    {
        T *pItem = NULL;
        if (m_uPendingCount == m_uSize)
        {
            return NULL;
        }
        pItem = m_pBuffer[m_uAvailableIdx];
        m_uAvailableIdx = (m_uAvailableIdx+1)%m_uSize;
        m_uPendingCount += 1;
        return pItem;
    }

    T* GetPending()
    {
        if (m_uPendingCount == 0) 
        {
            return NULL;
        }

        T *pItem = m_pBuffer[m_uPendingndex];
        m_uPendingndex = (m_uPendingndex+1)%m_uSize;
        m_uPendingCount -= 1;
        return pItem;
    }
};

class CCudaAutoLock
{
private:
    CUcontext m_pCtx;
public:
    CCudaAutoLock(CUcontext pCtx) :m_pCtx(pCtx) { cuCtxPushCurrent(m_pCtx); };
    ~CCudaAutoLock()  { CUcontext cuLast = NULL; cuCtxPopCurrent(&cuLast); };
};

typedef enum
{
    NV_ENC_DYNAMIC_RESOLUTION_CHANGE = 0,
    NV_ENC_DYNAMIC_BITRATE_CHANGE = 1,
    NV_ENC_FORCE_IDR = 2,
    NV_ENC_FORCE_INTRA_REFRESH = 3,
    NV_ENC_INVALIDATE_REFRENCE_FRAME = 4
}NvEncLowLatencyCmd;

typedef struct _NvEncLowLatencyComamnds
{
    NvEncLowLatencyCmd  nvEncCommand;
    uint32_t            frameNumber;
    uint32_t            numParams;
    uint32_t            params[MAX_ENC_COMMAND_PARAMS];
}NvEncLowLatencyComamnds;

typedef struct _EncodeFrameConfig
{
    uint8_t  *yuv[3];
    uint32_t stride[3];
    uint32_t width;
    uint32_t height;
    NvEncPictureCommand nvEncPicCommand;
}EncodeFrameConfig;

class CNvEncoderLowLatency
{
public:
    CNvEncoderLowLatency();
    virtual ~CNvEncoderLowLatency();
    int                                                  EncodeMain(int argc, char *argv[]);

	int		NvencInit(); // smtech
	int		ThreadProc();  // smtech
	int		GetSPSPPS(); // smtech

	int		SetBitRate(int bitrate, int buffsize);



protected:

    CNvHWEncoder                                        *m_pNvHWEncoder;
    CUcontext                                            m_cuContext;
    CUmodule                                             m_cuModule;
    CUfunction                                           m_cuInterleaveUVFunction;
    CUfunction                                           m_cuScaleNV12Function;
    CUdeviceptr                                          m_ChromaDevPtr[2];
    CUtexref                                             m_texLuma2D;   // YYYY 2D PL texture (uchar1)
    CUtexref                                             m_texChroma2D; // UVUV 2D PL texture (uchar2)
    EncodeConfig                                         m_stEncoderInput;
    EncodeBuffer                                         m_stEncodeBuffer[MAX_ENCODE_QUEUE];
    CNvQueue<EncodeBuffer>                               m_EncodeBufferQueue;
    uint32_t                                             m_uEncodeBufferCount;
    EncodeOutputBuffer                                   m_stEOSOutputBfr; 
    NvEncLowLatencyComamnds                              m_nvEncCommands[MAX_ENC_COMMAND];
    uint32_t                                             m_NumNvEncCommands;
    uint32_t                                             m_CurEncCommandIdx;
    uint8_t                                             *m_yuv[3];
    FILE                                                *m_qpHandle;
    int8_t                                              *m_qpDeltaMapArray;
    uint32_t                                             m_qpDeltaMapArraySize;

protected:
    NVENCSTATUS                                          Deinitialize();
	NVENCSTATUS                                          InitCuda(uint32_t deviceID, const char *exec_path);
    CUtexref                                             InitTexture(CUmodule hModule, const char *name, CUarray_format fmt, int NumPackedComponents = 1, unsigned int Flags = CU_TRSF_READ_AS_INTEGER);
    NVENCSTATUS                                          AllocateIOBuffers(uint32_t uInputWidth, uint32_t uInputHeight);
    NVENCSTATUS                                          ReleaseIOBuffers();
    NVENCSTATUS                                          FlushEncoder();
    NVENCSTATUS                                          PreProcessInput(EncodeBuffer *pEncodeBuffer, unsigned char *yuv[3],
                                                         uint32_t srcWidth, uint32_t srcHeight, uint32_t dstWidth, uint32_t dstHeight,
                                                         uint32_t maxWidth, uint32_t maxHeight);
    NVENCSTATUS                                          ConvertYUVToNV12(CUdeviceptr dNV12devPtr, int dstPitch, unsigned char *yuv[3],
                                                         int width, int height, int maxWidth, int maxHeight);
    void                                                 ParseEncodeCommandFile(char *FileName);
    void                                                 CheckAndInitNvEncCommand(uint32_t curFrameIdx, NvEncPictureCommand *pEncPicCommand);
    CUresult                                             ScaleNV12Image(CUdeviceptr dInput, CUdeviceptr dOutput,
                                                         int srcWidth, int srcPitch, int srcHeight,
                                                         int dstWidth, int dstPitch, int dstHeight,
                                                         int maxWidth, int maxHeight);
    NVENCSTATUS                                          ProcessRefFrameInvalidateCommands(const NvEncPictureCommand *pEncPicCommand);
};

// NVEncodeAPI entry point
typedef NVENCSTATUS (NVENCAPI *MYPROC)(NV_ENCODE_API_FUNCTION_LIST*); 


#endif
