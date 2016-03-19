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

#include <stdlib.h>
#include <string>
#include <iostream>
#include <fstream>
#include "NvEncoderLowLatency.h"


void CNvEncoderLowLatency::ParseEncodeCommandFile(char *fileName)
{
    std::fstream input(fileName, std::ios::in);
    std::string line;
    int lineNumber = 0;
    long value = 0;
    const std::string delims(" \t\n");

    if (strlen(fileName) == 0)
    {
        PRINTERR("no encode command file available\n");
    }
    else if (!input.is_open())
    {
        PRINTERR("Can't open %s\n", fileName);
        exit(1);
    }
    else
    {
        int cmdIdx = 0;
        while (std::getline(input, line))
        {

            lineNumber++;

            // check if a line is empty
            size_t beg_idx = line.find_first_not_of(delims, 0);
            // check if a line is a comment or empty
            if (beg_idx == std::string::npos || line[beg_idx] == '#')
            {
                continue;
            }
            char *begin, *end;
            begin = end = NULL;

            value = std::strtol(line.c_str(), &begin, 10);
            if (line.c_str() != begin) {
                m_nvEncCommands[cmdIdx].nvEncCommand = (NvEncLowLatencyCmd) value;
            }
            else {
                PRINTERR("Invalid command\n");
                continue;
            }

            value = std::strtol(begin, &end, 10);
            if (begin != end) {
                m_nvEncCommands[cmdIdx].frameNumber = value;
            }
            else {
                PRINTERR("Invalid frame number\n");
                continue;
            }
            begin = end;

            int numParams = 0;
            while (*begin && numParams < MAX_ENC_COMMAND_PARAMS)
            {
                value = strtol(begin, &end, 10);
                if (begin != end) {
                    m_nvEncCommands[cmdIdx].params[numParams] = value;
                    numParams++;
                }
                else {
                    PRINTERR("Invalid parameter(s)\n");
                    break;
                }
                begin = end;
            }
            m_nvEncCommands[cmdIdx].numParams = numParams;
            cmdIdx++;
        }
        m_NumNvEncCommands = cmdIdx;
    }
}

void CNvEncoderLowLatency::CheckAndInitNvEncCommand(uint32_t curFrameIdx, NvEncPictureCommand *pEncPicCommand)
{
    if (!pEncPicCommand)
        return;

    memset(pEncPicCommand, 0, sizeof(NvEncPictureCommand));
    for (uint32_t cmdIdx = m_CurEncCommandIdx; cmdIdx < m_NumNvEncCommands; cmdIdx++)
    {
        if (m_nvEncCommands[cmdIdx].frameNumber == curFrameIdx)
        {
            if (m_nvEncCommands[cmdIdx].nvEncCommand == NV_ENC_DYNAMIC_RESOLUTION_CHANGE)
            {
                pEncPicCommand->bResolutionChangePending = true;
                pEncPicCommand->newWidth = m_nvEncCommands[cmdIdx].params[0];
                pEncPicCommand->newHeight = m_nvEncCommands[cmdIdx].params[1];
            }
            else if (m_nvEncCommands[cmdIdx].nvEncCommand == NV_ENC_DYNAMIC_BITRATE_CHANGE)
            {
                pEncPicCommand->bBitrateChangePending = true;
                pEncPicCommand->newBitrate = m_nvEncCommands[cmdIdx].params[0];
                pEncPicCommand->newVBVSize = m_nvEncCommands[cmdIdx].params[1];
            }
            else if (m_nvEncCommands[cmdIdx].nvEncCommand == NV_ENC_FORCE_IDR)
            {
                pEncPicCommand->bForceIDR = true;
            }
            else if (m_nvEncCommands[cmdIdx].nvEncCommand == NV_ENC_FORCE_INTRA_REFRESH)
            {
                pEncPicCommand->bForceIntraRefresh = true;
                pEncPicCommand->intraRefreshDuration = m_nvEncCommands[cmdIdx].params[0];
            }
            else if (m_nvEncCommands[cmdIdx].nvEncCommand == NV_ENC_INVALIDATE_REFRENCE_FRAME)
            {
                pEncPicCommand->bInvalidateRefFrames = true;
                pEncPicCommand->numRefFramesToInvalidate = m_nvEncCommands[cmdIdx].numParams;
                for (uint32_t j = 0; j < pEncPicCommand->numRefFramesToInvalidate; j++)
                {
                    pEncPicCommand->refFrameNumbers[j] = m_nvEncCommands[cmdIdx].params[j];
                }
            }
            else
            {
                PRINTERR("Invalid Encode command = %d\n", m_nvEncCommands[cmdIdx].nvEncCommand);
            }
        }
    }
}

NVENCSTATUS CNvEncoderLowLatency::ProcessRefFrameInvalidateCommands(const NvEncPictureCommand *pEncPicCommand)
{
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

    nvStatus = m_pNvHWEncoder->NvEncInvalidateRefFrames(pEncPicCommand);

    return nvStatus;
}
