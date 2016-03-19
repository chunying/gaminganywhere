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

#include "NvEncoderLowLatency.h"
#include "dynlink_builtin_types.h"


NVENCSTATUS CNvEncoderLowLatency::PreProcessInput(EncodeBuffer *pEncodeBuffer, unsigned char *yuv[3],
                                                    uint32_t srcWidth, uint32_t srcHeight,
                                                    uint32_t dstWidth, uint32_t dstHeight,
                                                    uint32_t maxWidth, uint32_t maxHeight)
{

    bool bScaling = srcWidth != dstWidth || srcHeight != dstHeight;

    if (bScaling)
    {
        ConvertYUVToNV12(pEncodeBuffer->stInputBfr.pNV12TempdevPtr, pEncodeBuffer->stInputBfr.uNV12TempStride, yuv,
                            srcWidth, srcHeight, maxWidth, maxHeight);
        ScaleNV12Image(pEncodeBuffer->stInputBfr.pNV12TempdevPtr, pEncodeBuffer->stInputBfr.pNV12devPtr,
            srcWidth, pEncodeBuffer->stInputBfr.uNV12TempStride, srcHeight,
            dstWidth, pEncodeBuffer->stInputBfr.uNV12Stride, dstHeight,
            maxWidth, maxHeight);
    }
    else
    {
        ConvertYUVToNV12(pEncodeBuffer->stInputBfr.pNV12devPtr, pEncodeBuffer->stInputBfr.uNV12Stride, yuv,
                           dstWidth, dstHeight, maxWidth, maxHeight);
    }
    return NV_ENC_SUCCESS;
}


NVENCSTATUS CNvEncoderLowLatency::ConvertYUVToNV12(CUdeviceptr dNV12devPtr, int dstPitch, unsigned char *yuv[3],
                                                    int width, int height, int maxWidth, int maxHeight)
{
    CCudaAutoLock cuLock(m_cuContext);
    // copy luma
    CUDA_MEMCPY2D copyParam;
    memset(&copyParam, 0, sizeof(copyParam));
    copyParam.dstMemoryType = CU_MEMORYTYPE_DEVICE;
    copyParam.dstDevice = dNV12devPtr;
    copyParam.dstPitch = dstPitch;
    copyParam.srcMemoryType = CU_MEMORYTYPE_HOST;
    copyParam.srcHost = yuv[0];
    copyParam.srcPitch = width;
    copyParam.WidthInBytes = width;
    copyParam.Height = height;
    __cu(cuMemcpy2D(&copyParam));

    // copy chroma

    __cu(cuMemcpyHtoD(m_ChromaDevPtr[0], yuv[1], width*height / 4));
    __cu(cuMemcpyHtoD(m_ChromaDevPtr[1], yuv[2], width*height / 4));

#define BLOCK_X 32
#define BLOCK_Y 16
    int chromaHeight = height / 2;
    int chromaWidth = width / 2;
    dim3 block(BLOCK_X, BLOCK_Y, 1);
    dim3 grid((chromaWidth + BLOCK_X - 1) / BLOCK_X, (chromaHeight + BLOCK_Y - 1) / BLOCK_Y, 1);
#undef BLOCK_Y
#undef BLOCK_X

    CUdeviceptr dNV12Chroma = (CUdeviceptr)((unsigned char*)dNV12devPtr + dstPitch*maxHeight);
    void *args[8] = { &m_ChromaDevPtr[0], &m_ChromaDevPtr[1], &dNV12Chroma, &chromaWidth, &chromaHeight, &chromaWidth, &chromaWidth, &dstPitch };

    __cu(cuLaunchKernel(m_cuInterleaveUVFunction, grid.x, grid.y, grid.z,
        block.x, block.y, block.z,
        0,
        NULL, args, NULL));
    CUresult cuResult = cuStreamQuery(NULL);
    if (!((cuResult == CUDA_SUCCESS) || (cuResult == CUDA_ERROR_NOT_READY)))
    {
        return NV_ENC_ERR_GENERIC;
    }
    return NV_ENC_SUCCESS;
}

CUresult CNvEncoderLowLatency::ScaleNV12Image(CUdeviceptr dInput, CUdeviceptr dOutput,
                                                int srcWidth, int srcPitch, int srcHeight,
                                                int dstWidth, int dstPitch, int dstHeight,
                                                int maxWidth, int maxHeight)

{
    CCudaAutoLock cuLock(m_cuContext);
    CUDA_ARRAY_DESCRIPTOR desc;
    CUresult result;
    float left, right;
    float xOffset, yOffset, xScale, yScale;
    int srcLeft, srcTop, srcRight, srcBottom;
    int dstLeft, dstTop, dstRight, dstBottom;

    srcLeft = 0;
    srcTop = 0;
    srcRight = srcWidth;
    srcBottom = srcHeight;

    dstLeft = 0;
    dstTop = 0;
    dstRight = dstWidth;
    dstBottom = dstHeight;


    if ((!dInput) || (!dOutput))
    {
        PRINTERR("NULL surface pointer!\n");
        return CUDA_ERROR_INVALID_VALUE;
    }
    xScale = (float)(srcRight - srcLeft) / (float)(dstRight - dstLeft);
    xOffset = 0.5f*xScale - 0.5f;
    if (xOffset > 0.5f)
        xOffset = 0.5f;
    yScale = (float)(srcBottom - srcTop) / (float)(dstBottom - dstTop);
    yOffset = 0.5f*yScale - 0.5f;
    if (yOffset > 0.5f)
        yOffset = 0.5f;
    left = (float)srcLeft;
    right = (float)(srcRight - 1);
    xOffset += left;
    desc.NumChannels = 1;
    desc.Width = srcPitch / desc.NumChannels;
    desc.Height = srcBottom - srcTop;
    desc.Format = CU_AD_FORMAT_UNSIGNED_INT8;
    result = cuTexRefSetFilterMode(m_texLuma2D, CU_TR_FILTER_MODE_LINEAR);
    if (result != CUDA_SUCCESS)
    {
        PRINTERR("cuTexRefSetFilterMode: %d\n", result);
        return result;
    }
    result = cuTexRefSetAddress2D(m_texLuma2D, &desc, dInput + srcTop*srcPitch, srcPitch);
    if (result != CUDA_SUCCESS)
    {
        PRINTERR("BindTexture2D(luma): %d\n", result);
        return result;
    }
    desc.NumChannels = 2;
    desc.Width = srcPitch / desc.NumChannels;
    desc.Height = (srcBottom - srcTop) >> 1;
    desc.Format = CU_AD_FORMAT_UNSIGNED_INT8;
    result = cuTexRefSetFilterMode(m_texChroma2D, CU_TR_FILTER_MODE_LINEAR);
    if (result != CUDA_SUCCESS)
    {
        PRINTERR("cuTexRefSetFilterMode: %d\n", result);
        return result;
    }

    result = cuTexRefSetAddress2D(m_texChroma2D, &desc, dInput + (maxHeight + srcTop/2)*srcPitch, srcPitch);
    if (result != CUDA_SUCCESS)
    {
        PRINTERR("BindTexture2D(chroma): %d\n", result);
        return result;
    }

    int dstUVOffset = maxHeight * srcPitch;
    float x_Offset = xOffset - dstLeft*xScale;
    float y_Offset = yOffset + 0.5f - dstTop*yScale;
    float xc_offset = xOffset - dstLeft*xScale*0.5f;
    float yc_offset = yOffset + 0.5f - dstTop*yScale*0.5f;

    void *args[13] = { &dOutput, &dstUVOffset, &dstWidth, &dstHeight, &dstPitch,
        &left, &right, &x_Offset, &y_Offset,
        &xc_offset, &yc_offset, &xScale, &yScale };
    dim3 block(256, 1, 1);
    dim3 grid((dstRight + 255) >> 8, (dstBottom + 1) >> 1, 1);

    result = cuLaunchKernel(m_cuScaleNV12Function, grid.x, grid.y, grid.z,
        block.x, block.y, block.z,
        0,
        NULL, args, NULL);
    if (result != CUDA_SUCCESS)
    {
        PRINTERR("cuLaunchKernel: %d\n", result);
        return result;
    }

    result = cuStreamQuery(NULL);
    if (!((result == CUDA_SUCCESS) || (result == CUDA_ERROR_NOT_READY)))
    {
        return CUDA_SUCCESS;
    }
    return result;
}
