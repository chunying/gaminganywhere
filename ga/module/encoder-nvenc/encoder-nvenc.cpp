/*
 *	Programmer: Jiaming Zhang   @Sun Yat-Sen University, Guangzhou
 *	
 *	Nvidia Video Encoding SDK, integrate into GamingAnywhere-0.8.0
 */

#include <stdio.h>

#include "vsource.h"
#include "rtspconf.h"
#include "encoder-common.h"

#include "ga-common.h"
#include "ga-avcodec.h"
#include "ga-conf.h"
#include "ga-module.h"

#include "dpipe.h"

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

#define SAVEENC

static char							*ga_root;					// GA_ROOT PATH

static struct RTSPConf				*rtspconf = NULL;

static int							vencoder_initialized = 0;
static int							vencoder_started = 0;
static pthread_t					nvencthread;				// thread
static ga_ioctl_reconfigure_t		nvenc_reconf;

// specific data for h.264
static char							*_sps;
static int							_spslen;
static char							*_pps;
static int							_ppslen;

//------------------------------------NVENC COMMON-------------------------------------------

static EncodeBuffer					*pEncodeBuffer;				//encode buffer ptr
static EncodeConfig					encodeConfig;				//encode config struct

static CNvEncoderLowLatency			nvEncoder;					// NVENC main Class instance

/*
 *	NVENC Encoder Initialize method
 */
int CNvEncoderLowLatency::NvencInit() {
	// Get GA_ROOT environment path
	char *confpath;
	if ((confpath = getenv("GA_ROOT")) == NULL) {
		ga_error("GA_ROOT not set.\n");
		return 1;
	}
	ga_root = strdup(confpath);
	ga_error("+++ ENCODER-NVECN  GA_ROOT: %s +++\n", ga_root);

    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

    memset(&encodeConfig, 0, sizeof(EncodeConfig));

	// Encode Config Setting
    encodeConfig.endFrameIdx = INT_MAX;
    encodeConfig.bitrate = ga_conf_mapreadint("video-specific", "b");// 3000000;
    encodeConfig.rcMode = NV_ENC_PARAMS_RC_VBR;
    encodeConfig.gopLength = NVENC_INFINITE_GOPLENGTH;
    encodeConfig.deviceType = 0;	//DX9 DeviceType
    encodeConfig.codec = NV_ENC_H264;
    encodeConfig.fps = 30;
    encodeConfig.qp = 28;
    encodeConfig.i_quant_factor = DEFAULT_I_QFACTOR;
    encodeConfig.b_quant_factor = DEFAULT_B_QFACTOR;  
    encodeConfig.i_quant_offset = DEFAULT_I_QOFFSET;
    encodeConfig.b_quant_offset = DEFAULT_B_QOFFSET; 
    encodeConfig.presetGUID = NV_ENC_PRESET_LOW_LATENCY_HQ_GUID;	// Low Latency HQ
    encodeConfig.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
    encodeConfig.numB = 0;

	// Set encoding resolution
	encodeConfig.width  = video_source_out_width(0);
	encodeConfig.height = video_source_out_height(0);

#ifdef SAVEENC
	// Set output file:
	char outputFileName[128];
	sprintf(outputFileName, "%slog/nvenc_output.h264", ga_root);	// Output file path
	encodeConfig.outputFileName = outputFileName;

	encodeConfig.fOutput = fopen(encodeConfig.outputFileName, "wb");

    if (encodeConfig.fOutput == NULL)
    {
        ga_error("Failed to create \"%s\"\n", encodeConfig.outputFileName);
        return 1;
    }
#endif

	ga_error("+++ Make point 0 +++\n");

    // initialize D3D
    //nvStatus = InitCuda(encodeConfig.deviceID, argv[0]);
	encodeConfig.deviceID = 0;
	nvStatus = InitCuda(encodeConfig.deviceID, ga_root);    //exec path: current
    if (nvStatus != NV_ENC_SUCCESS)
        return nvStatus;
	ga_error("+++ Make point 1 +++\n");

    nvStatus = m_pNvHWEncoder->Initialize((void*)m_cuContext, NV_ENC_DEVICE_TYPE_CUDA);
    if (nvStatus != NV_ENC_SUCCESS)
        return 1;

	ga_error("+++ Make point 2 +++\n");

    encodeConfig.presetGUID = m_pNvHWEncoder->GetPresetGUID(encodeConfig.encoderPreset, encodeConfig.codec);
    
	ga_error("------------------------------NVENC Encode Config-------------------------------\n");
#ifdef SAVEENC
    ga_error("         output          : \"%s\"\n", encodeConfig.outputFileName);
#endif
    if (encodeConfig.encCmdFileName)
    {
        ga_error("Command File             : %s\n", encodeConfig.encCmdFileName);
    }
    ga_error("         codec           : \"%s\"\n", encodeConfig.codec == NV_ENC_HEVC ? "HEVC" : "H264");
    ga_error("         size            : %dx%d\n", encodeConfig.width, encodeConfig.height);
    ga_error("         bitrate         : %d bits/sec\n", encodeConfig.bitrate);
    ga_error("         vbvSize         : %d bits\n", encodeConfig.vbvSize);
    ga_error("         fps             : %d frames/sec\n", encodeConfig.fps);
    if (encodeConfig.intraRefreshEnableFlag)
    {
        ga_error("IntraRefreshPeriod       : %d\n", encodeConfig.intraRefreshPeriod);
        ga_error("IntraRefreshDuration     : %d\n", encodeConfig.intraRefreshDuration);
    }
    ga_error("         rcMode          : %s\n", encodeConfig.rcMode == NV_ENC_PARAMS_RC_CONSTQP ? "CONSTQP" :
                                              encodeConfig.rcMode == NV_ENC_PARAMS_RC_VBR ? "VBR" :
                                              encodeConfig.rcMode == NV_ENC_PARAMS_RC_CBR ? "CBR" :
                                              encodeConfig.rcMode == NV_ENC_PARAMS_RC_VBR_MINQP ? "VBR MINQP" :
                                              encodeConfig.rcMode == NV_ENC_PARAMS_RC_2_PASS_QUALITY ? "TWO_PASS_QUALITY" :
                                              encodeConfig.rcMode == NV_ENC_PARAMS_RC_2_PASS_FRAMESIZE_CAP ? "TWO_PASS_FRAMESIZE_CAP" :
                                              encodeConfig.rcMode == NV_ENC_PARAMS_RC_2_PASS_VBR ? "TWO_PASS_VBR" : "UNKNOWN");
    ga_error("         preset          : %s\n", (encodeConfig.presetGUID == NV_ENC_PRESET_LOW_LATENCY_HQ_GUID) ? "LOW_LATENCY_HQ" :
                                              (encodeConfig.presetGUID == NV_ENC_PRESET_LOW_LATENCY_HP_GUID) ? "LOW_LATENCY_HP" :
                                              (encodeConfig.presetGUID == NV_ENC_PRESET_HQ_GUID) ? "HQ_PRESET" :
                                              (encodeConfig.presetGUID == NV_ENC_PRESET_HP_GUID) ? "HP_PRESET" :
                                              (encodeConfig.presetGUID == NV_ENC_PRESET_LOSSLESS_HP_GUID) ? "LOSSLESS_HP" :
                                              (encodeConfig.presetGUID == NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID) ? "LOW_LATENCY_DEFAULT" : "DEFAULT");
    ga_error("--------------------------------------------------------------------------------------\n\n");




	nvStatus = m_pNvHWEncoder->CreateEncoder(&encodeConfig);
    if (nvStatus != NV_ENC_SUCCESS)
    {
		ga_error("+++ Create NVENC Encoder fail! +++\n");
        if (encodeConfig.fOutput)
			fclose(encodeConfig.fOutput);

		Deinitialize();
    }

	return 0;
}

/*
 *	NVENC Encoder Encoding and Streaming method
 */
int CNvEncoderLowLatency::ThreadProc() 
{
    unsigned long long lStart, lEnd, lFreq;
    int numFramesEncoded = 0;
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    bool bError = false;

    m_uEncodeBufferCount = 3;
    nvStatus = AllocateIOBuffers(m_pNvHWEncoder->m_uMaxWidth, m_pNvHWEncoder->m_uMaxHeight);
    if (nvStatus != NV_ENC_SUCCESS)
        return nvStatus;

	ga_error("+++ Make point 4 +++\n");

    NvQueryPerformanceCounter(&lStart);

	/* ga */
	vsource_frame_t *frame = NULL;
	char *pipename = "filter-0"; //(char*) arg;
	dpipe_t *pipe = dpipe_lookup(pipename);
	dpipe_buffer_t *data = NULL;
	//
	unsigned char *pktbuf = NULL;
	int pktbufsize = 0, pktbufmax = 0;
	int video_written = 0;
	int64_t x264_pts = 0;
	//
	rtspconf = rtspconf_global();
	//
	pktbufmax = encodeConfig.width * encodeConfig.height * 2;
	if((pktbuf = (unsigned char*) malloc(pktbufmax)) == NULL) {
		ga_error("video encoder: allocate memory failed.\n");
		goto exit;
	}

	/* Start encoding */
	for (int frm = encodeConfig.startFrameIdx; frm <= encodeConfig.endFrameIdx && vencoder_started != 0; frm++)   // need to be fixed!
    {
        //numBytesRead = 0;
        //loadframe(m_yuv, hInput, frm, encodeConfig.width, encodeConfig.height, numBytesRead);

		//Try to load data to m_yuv
		struct timeval tv;
		struct timespec to;
		gettimeofday(&tv, NULL);
		// wait for notification
		to.tv_sec = tv.tv_sec+1;
		to.tv_nsec = tv.tv_usec * 1000;

		data = dpipe_load(pipe, &to);
		if(data == NULL) {
			ga_error("viedo encoder: image source timed out.\n");
			continue;
		}
		frame = (vsource_frame_t*) data->pointer;

		x264_pts++;		// presentation time stamp

		int stride = encodeConfig.width * encodeConfig.height;
		CopyMemory(m_yuv[0], frame->imgbuf,  stride);
		CopyMemory(m_yuv[1], frame->imgbuf + stride,  stride / 4);
		CopyMemory(m_yuv[2], frame->imgbuf + stride + stride / 4, stride / 4);

		dpipe_put(pipe, data);

        //if (numBytesRead == 0)
        //    break;

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
			ga_error("+++ Make point 5 +++\n");
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
			ga_error("+++ Make point 6 +++\n");
            assert(0);
            return nvStatus;
        }

        nvStatus = m_pNvHWEncoder->NvEncMapInputResource(pEncodeBuffer->stInputBfr.nvRegisteredResource, &pEncodeBuffer->stInputBfr.hInputSurface);
        if (nvStatus != NV_ENC_SUCCESS)
        {
            ga_error("Failed to Map input buffer %p\n", pEncodeBuffer->stInputBfr.hInputSurface);
            bError = true;
            goto exit;
        }

        if (m_qpDeltaMapArray)
        {
            memset(m_qpDeltaMapArray, 0, m_qpDeltaMapArraySize);
            size_t numElemsRead = fread(m_qpDeltaMapArray, m_qpDeltaMapArraySize, 1, m_qpHandle);
            if (numElemsRead != 1)
            {
                ga_error("Warning: Amount of data read from m_qpHandle is less than requested.\n");
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

		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.pts = x264_pts;
		pkt.stream_index = 0;
		// concatenate nals
		pktbufsize = 0;

		// handle NVENC pEncodeBuffer
		pEncodeBuffer = m_EncodeBufferQueue.GetPending();

		m_pNvHWEncoder->ProcessEncodeBuffer(pEncodeBuffer, pktbuf, pktbufsize);

		// UnMap the input buffer after frame is done
        if (pEncodeBuffer && pEncodeBuffer->stInputBfr.hInputSurface)
        {
            nvStatus = m_pNvHWEncoder->NvEncUnmapInputResource(pEncodeBuffer->stInputBfr.hInputSurface);
            pEncodeBuffer->stInputBfr.hInputSurface = NULL;
        }

		pkt.size = pktbufsize;
		pkt.data = pktbuf;
		
		// send the packet
		if(encoder_send_packet("video-encoder",
				0/*iid*/, &pkt,
				pkt.pts, NULL) < 0) {
			goto exit;
		}
    }

    //FlushEncoder();

exit:
#ifdef SAVEENC
    if (encodeConfig.fOutput)
    {
        fclose(encodeConfig.fOutput);
    }
#endif

    Deinitialize();

	//
	if(pipe) {
		pipe = NULL;
	}
	if(pktbuf != NULL) {
		free(pktbuf);
	}
	pktbuf = NULL;
	//
	ga_error("video encoder: thread terminated (tid=%ld).\n", ga_gettid());

	return bError ? 1 : 0;
}

/*
 *	Use for dump SPS/PPS
 */
static void
nvenc_dump_buffer(const char *prefix, unsigned char *buf, int buflen) {
	char output[2048];
	int size = 0, wlen;
	// head
	wlen = snprintf(output+size, sizeof(output)-size, "%s [", prefix);
	size += wlen;
	// numbers
	while(buflen > 0) {
		wlen = snprintf(output+size, sizeof(output)-size, " %02x", *buf++);
		size += wlen;
		buflen--;
	}
	// tail
	wlen = snprintf(output+size, sizeof(output)-size, " ]\n");
	ga_error(output);
	return;
}

/*
 *	NVENC Encoder Get SPS/PPS
 */
int CNvEncoderLowLatency::GetSPSPPS() {
	char tmpHeader[1024];
	NV_ENC_SEQUENCE_PARAM_PAYLOAD spspps;
	//
	if (vencoder_initialized == 0) {
		ga_error("video encoder: get sps/pps failed - not initialized?\n");
		return GA_IOCTL_ERR_NOTINITIALIZED;
	}
	//
	NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

	uint32_t outSize = 0;

	// Memset zero
	bzero(&spspps, sizeof(spspps));
	bzero(tmpHeader, sizeof(tmpHeader));

	spspps.spsppsBuffer = tmpHeader;
	spspps.inBufferSize = sizeof(tmpHeader);
	spspps.outSPSPPSPayloadSize = &outSize;
	SET_VER(spspps, NV_ENC_SEQUENCE_PARAM_PAYLOAD);

	// Get SPS PPS
	nvStatus = m_pNvHWEncoder->NvEncGetSequenceParams(&spspps);
	if (nvStatus != NV_ENC_SUCCESS) {
		ga_error("+++ NVENC: Get sps/pps failed. nvStatus: %d +++\n", nvStatus);
		return GA_IOCTL_ERR_NOTFOUND;
	}

	// Get SPS
    char *sps = tmpHeader;
    unsigned int i_sps = 4;
    
    while (tmpHeader[i_sps    ] != 0x00
        || tmpHeader[i_sps + 1] != 0x00
        || tmpHeader[i_sps + 2] != 0x00
        || tmpHeader[i_sps + 3] != 0x01)
    {
        i_sps += 1;
        if (i_sps >= outSize)
        {
            ga_error("+++ Invalid SPS/PPS +++\n");
            return NV_ENC_ERR_GENERIC;
        }
    }

	// Get PPS
    char *pps = tmpHeader + i_sps;
    unsigned int i_pps = outSize - i_sps;

	// Allocate memory for SPS
	if((_sps = (char*) malloc(i_sps)) == NULL) {
		ga_error("video encoder: get sps/pps failed - alloc sps failed.\n");
		return GA_IOCTL_ERR_NOMEM;
	}
	// Allocate memory for PPS
	if((_pps = (char*) malloc(i_pps)) == NULL) {
		free(_sps);
		_sps = NULL;
		ga_error("video encoder: get sps/pps failed - alloc pps failed.\n");
		return GA_IOCTL_ERR_NOMEM;
	}

	bcopy(sps, _sps, sizeof(i_sps));
	bcopy(pps, _pps, sizeof(i_pps));
	_spslen = i_sps;
	_ppslen = i_pps;

	ga_error("+++ SPSPPSPayloadSize: %d +++\n", outSize);

	nvenc_dump_buffer("+++ video encoder: sps = +++", (unsigned char*) _sps, _spslen);
	nvenc_dump_buffer("+++ video encoder: pps = +++", (unsigned char*) _pps, _ppslen);

	return nvStatus;
}

/*
 *	NVENC Encoder Process Encoder Buffer
 *
 *	Lock the encode buffer and copy it to packet buffer for packet sending, then unlock encode buffer.
 */
NVENCSTATUS CNvHWEncoder::ProcessEncodeBuffer(const EncodeBuffer *pEncodeBuffer, unsigned char * pktbuf, int &pktbufsize) {
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

    if (pEncodeBuffer->stOutputBfr.hBitstreamBuffer == NULL && pEncodeBuffer->stOutputBfr.bEOSFlag == FALSE)
    {
        return NV_ENC_ERR_INVALID_PARAM;
    }

    if (pEncodeBuffer->stOutputBfr.bWaitOnEvent == TRUE)
    {
        if (!pEncodeBuffer->stOutputBfr.hOutputEvent)
        {
            return NV_ENC_ERR_INVALID_PARAM;
        }
#if defined(NV_WINDOWS)
        WaitForSingleObject(pEncodeBuffer->stOutputBfr.hOutputEvent, INFINITE);
#endif
    }

    if (pEncodeBuffer->stOutputBfr.bEOSFlag)
        return NV_ENC_SUCCESS;

    nvStatus = NV_ENC_SUCCESS;
    NV_ENC_LOCK_BITSTREAM lockBitstreamData;
    memset(&lockBitstreamData, 0, sizeof(lockBitstreamData));
    SET_VER(lockBitstreamData, NV_ENC_LOCK_BITSTREAM);
    lockBitstreamData.outputBitstream = pEncodeBuffer->stOutputBfr.hBitstreamBuffer;
    lockBitstreamData.doNotWait = false;

    nvStatus = m_pEncodeAPI->nvEncLockBitstream(m_hEncoder, &lockBitstreamData);
    if (nvStatus == NV_ENC_SUCCESS)
    {
        //fwrite(lockBitstreamData.bitstreamBufferPtr, 1, lockBitstreamData.bitstreamSizeInBytes, m_fOutput);
		CopyMemory(pktbuf, lockBitstreamData.bitstreamBufferPtr, lockBitstreamData.bitstreamSizeInBytes);
		pktbufsize = lockBitstreamData.bitstreamSizeInBytes;

        nvStatus = m_pEncodeAPI->nvEncUnlockBitstream(m_hEncoder, pEncodeBuffer->stOutputBfr.hBitstreamBuffer);
    }
    else
    {
        ga_error("lock bitstream function failed \n");
    }
	
    return nvStatus;
}

/*
 *	NVENC Encoder Reset bitrate
 */
int CNvEncoderLowLatency::SetBitRate(int bitrate, int buffsize) {
	NvEncPictureCommand encPicCommand;
    	memset(&encPicCommand, 0, sizeof(encPicCommand));
	encPicCommand.bBitrateChangePending = true;
	encPicCommand.newBitrate = bitrate;
	encPicCommand.newVBVSize = buffsize;

    	NVENCSTATUS nvStatus = m_pNvHWEncoder->NvEncReconfigureEncoder(&encPicCommand);
	if (nvStatus != NV_ENC_SUCCESS)
		return -1;

	return 0;
}



//----------------------------------- encoder nvenc -----------------------------------------------

/*
 *	Encoder deinitialize
 */
static int
nvenc_deinit(void *arg) {
#ifdef SAVEENC
	if (encodeConfig.fOutput){
        fclose(encodeConfig.fOutput);
    }
#endif
	free(_sps);
	free(_pps);

	_sps = NULL;
	_pps = NULL;
	_spslen = 0;
	_ppslen = 0;

	vencoder_initialized = 0;
	ga_error("video encoder: deinitialized.\n");
	return 0;
}

/*
 *	Encoder initialize
 */
static int
nvenc_init(void *arg) {
	if(vencoder_initialized != 0)
		return 0;

	if (nvEncoder.NvencInit() != 0)
		return 0;
	
	ga_error("+++ NVENC initialized! +++\n");
	vencoder_initialized = 1;

	return 0;
}


/*
 *	Encoder Reconfiguration
 */
static int
nvenc_reconfigure(int kbitrate, int buffsize) {
	int sts;
	if ((sts = nvEncoder.SetBitRate(kbitrate * 1000, buffsize * 1000)) < 0) {
		ga_error("+++ NVENC Reconf failed! nvStatus: %d +++\n", sts);
		return -1;
	}
	return 0;
}

/*
 *	Encoder thread proc
 */
static void*
nvenc_threadproc(void *arg) {
	ga_error("+++ NVENC thread process +++\n");
	
	int ret = nvEncoder.ThreadProc();
	ga_error("+++++++++++++  nvStatus: %d ++++++++++++++\n\n\n", ret);

	return NULL;
}

/*
 *	Encoder start
 */
static int
nvenc_start(void *arg) {
	if(vencoder_started != 0)
		return 0;
	vencoder_started = 1;

	if(pthread_create(&nvencthread, NULL, nvenc_threadproc, NULL) != 0) {
		ga_error("video encoder: create thread failed.\n");
		return -1;
	}

	return 0;
}

/*
 *	Encoder stop
 */
static int
nvenc_stop(void *arg) {
	void *ignore;
	if (vencoder_started == 0)
		return 0;
	vencoder_started = 0;
	pthread_join(nvencthread, &ignore);

	ga_error("video encoder: all stopped\n");
	return 0;
}

static int nvenc_get_sps_pps(int iid) {
	if (nvEncoder.GetSPSPPS() != NV_ENC_SUCCESS)
		return GA_IOCTL_ERR_NOTFOUND;
	return 0;
}

/*
 *	Encoder IO Contrl
 */
static int 
nvenc_ioctl(int command, int argsize, void *arg) {
	int						ret		= 0;
	ga_ioctl_buffer_t		*buf	= (ga_ioctl_buffer_t*) arg;
	ga_ioctl_reconfigure_t	*reconf = (ga_ioctl_reconfigure_t*) arg;
	//
	if(vencoder_initialized == 0)
		return GA_IOCTL_ERR_NOTINITIALIZED;
	//
	switch(command) {
	case GA_IOCTL_RECONFIGURE:
		if(argsize != sizeof(ga_ioctl_reconfigure_t))
			return GA_IOCTL_ERR_INVALID_ARGUMENT;
		nvenc_reconfigure(reconf->bitrateKbps, reconf->bufsize);
		break;
	case GA_IOCTL_GETSPS:
		if(argsize != sizeof(ga_ioctl_buffer_t))
			return GA_IOCTL_ERR_INVALID_ARGUMENT;
		if(nvenc_get_sps_pps(0) < 0)
			return GA_IOCTL_ERR_NOTFOUND;
		if(buf->size < _spslen)
			return GA_IOCTL_ERR_BUFFERSIZE;
		buf->size = _spslen;
		bcopy(_sps, buf->ptr, buf->size);
		break;
	case GA_IOCTL_GETPPS:
		if(argsize != sizeof(ga_ioctl_buffer_t))
			return GA_IOCTL_ERR_INVALID_ARGUMENT;
		if(nvenc_get_sps_pps(0) < 0)
			return GA_IOCTL_ERR_NOTFOUND;
		if(buf->size < _ppslen)
			return GA_IOCTL_ERR_BUFFERSIZE;
		buf->size = _ppslen;
		bcopy(_pps, buf->ptr, buf->size);
		break;
	default:
		ret = GA_IOCTL_ERR_NOTSUPPORTED;
		break;
	}
	return ret;
}

//---------------------------------------------------------

/*
 *	ga_module interface for encoder-nvenc
 */
ga_module_t *
module_load() {
	static ga_module_t m;
	struct RTSPConf *rtspconf = rtspconf_global();
	//
	bzero(&m, sizeof(m));
	m.type = GA_MODULE_TYPE_VENCODER;
	m.name = strdup("nvenc-video-encoder");
	m.mimetype = strdup("video/H264");
	m.init  = nvenc_init;
	m.deinit= nvenc_deinit;
	m.start = nvenc_start;
	m.ioctl = nvenc_ioctl;
	m.stop  = nvenc_stop;

	return &m;
}
