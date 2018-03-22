/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : vp9Hw.c
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2017/04/13
*   Comment :
*
*
*/

#include "vp9Hw_dec.h"

#include "vdecoder.h"

char* pVp9RegisterBaseAddr = NULL;

static int  Vp9HwDecoderInit(DecoderInterface* pSelf, VConfig* pConfig,
                VideoStreamInfo* pVideoInfo, VideoFbmInfo* pFbmInfo);
void Vp9HwDecoderReset(DecoderInterface* pSelf);
static int  Vp9HwDecoderSetSbm(DecoderInterface* pSelf, SbmInterface* pSbm, int nIndex);
static int  Vp9HwDecoderGetFbmNum(DecoderInterface* pSelf);
static Fbm* Vp9HwDecoderGetFbm(DecoderInterface* pSelf, int nIndex);
static int  Vp9HwDecoderDecode(DecoderInterface* pSelf,
                            int               bEndOfStream,
                            int               bDecodeKeyFrameOnly,
                            int               bSkipBFrameIfDelay,
                            int64_t           nCurrentTimeUs);
static void Destroy(DecoderInterface* pSelf);

extern void Vp9HwInitDecode(Vp9HwDecodeContext* pVp9HwContext, Vp9HwDecode* pVp9HwDec);
extern void Vp9HwFlushPictures(Vp9HwDecode* pVp9HwDec, Fbm* pFbm,
                  Fbm* pFbmScaleDown, u8 bDispFlag);
extern void Vp9HwDecodeSetSbmBuf(u8* pSbmBase, u32 nSbmSize, Vp9HwDecode* pVp9HwDec);
extern void Vp9HwResetDecodeParams(Vp9HwDecode* pVp9HwDec);
extern s32 Vp9RequestBitstreamData(Vp9HwDecodeContext* pVp9HwContext, Vp9HwDecode* pVp9HwDec);
extern s32 Vp9DecodeHeaders(void* decContainer, u8* pDataPtr, u32 nDataLen);
extern s32 Vp9RequestMemory(Vp9HwDecodeContext* pVp9HwContext, Vp9HwDecode* pVp9HwDec);
extern void Vp9AsicProbUpdate(void* dec_cont);
extern void Vp9AsicInitPicture(void* dec_cont);
extern void Vp9AsicStrmPosUpdate(void* dec_cont, u32 strm_bus_address,u32 data_len);
extern void Vp9AsicStrmPosUpdate_sbmback(void* decCont, u32 strm_bus_address, u32 data_len);

extern s32 Vp9MallocDecodeBuffer(Vp9HwDecode* pVp9HwDec);
extern s32 Vp9ReleaseDecodeBuffer(Vp9HwDecode* pVp9HwDec);
extern s32 VP9SyncAndOutput(struct Vp9DecContainer *dec_cont, Fbm* curFbm, VideoPicture* curFrame);
extern s32 Vp9ProcessHeaders(void* decContainer, u8* pDataPtr, u32 nDataLen);
extern void Vp9CreateFbmBuffer(Vp9HwDecodeContext* pVp9HwContext);
extern void VP9CongigureDisplayParameters(Vp9HwDecodeContext* pVp9HwContext);
extern s32 Vp9GetRefFrm(Vp9HwDecodeContext* pVp9HwContext);
extern s32 Vp9AsicAllocateFilterBlockMem(struct Vp9DecContainer *dec_cont);
extern u32 Vp9AsicRun(struct Vp9DecContainer *dec_cont);
extern void Vp9SaveDecodeFrame(void* decContainer);
extern void Vp9HwSaveYuvData(void* decContainer);
extern void Vp9PrintData(void* decContainer);
//extern u32 Vp9ProcessDataLength(VideoStreamDataInfo* curStreamDataInfo);
extern u32 Vp9ProcessDataLength(u8* data,int size, u8* n_frames,u32* realDataLen);
extern int64_t VdecoderGetCurrentTime(void);

//*******************************************************************************//
#include "videoengine.h"
#include "log.h"
//*******************************************************************************//

DecoderInterface* CreateVp9HwDecoder(VideoEngine* p)
{
    Vp9HwDecodeContext* pVp9HwContext;

    pVp9HwContext = (Vp9HwDecodeContext*) malloc(sizeof(Vp9HwDecodeContext));
    if(pVp9HwContext == NULL)
        return NULL;

    memset(pVp9HwContext, 0, sizeof(Vp9HwDecodeContext));

    pVp9HwContext->pVideoEngine        = p;
    pVp9HwContext->interface.Init      = Vp9HwDecoderInit;
    pVp9HwContext->interface.Reset     = Vp9HwDecoderReset;
    pVp9HwContext->interface.SetSbm    = Vp9HwDecoderSetSbm;
    pVp9HwContext->interface.GetFbmNum = Vp9HwDecoderGetFbmNum;
    pVp9HwContext->interface.GetFbm    = Vp9HwDecoderGetFbm;
    pVp9HwContext->interface.Decode    = Vp9HwDecoderDecode;
    pVp9HwContext->interface.Destroy   = Destroy;

    return &pVp9HwContext->interface;
}

void CedarPluginVDInit(void)
{
    int ret;
    ret = VDecoderRegister(VIDEO_CODEC_FORMAT_VP9, "Vp9Hw", CreateVp9HwDecoder, 0);
    if (0 == ret)
    {
        logi("register Vp9Hw decoder success!");
    }
    else
    {
        loge("register Vp9Hw soft decoder failure!!!");
    }
    return ;
}

//************************************************************************************/
/*
      Name:        Vp9HwDecoderInit
      Prototype:    Handle ve_open (VConfig* config, VideoStreamInfo* stream_info);
      Function:    Start up the VE CSP.
      Return:    A handle of the VE device.
      Input:    VConfig* config, the configuration for the VE CSP.
*/
//************************************************************************************/
static int  Vp9HwDecoderInit(DecoderInterface* pSelf, VConfig* pConfig,
                                  VideoStreamInfo * pVideoInfo, VideoFbmInfo* pFbmInfo)
{
    Vp9HwDecodeContext* pVp9HwContext=NULL;
    Vp9HwDecode* pVp9HwDecHandle=NULL;
    //struct Vp9DecContainer* dec_cont=NULL;
    //s32 nSeqInfLen = 0;

    pVp9HwContext = (Vp9HwDecodeContext*)pSelf;
    pVp9HwContext->pFbmInfo = pFbmInfo;

    memcpy(&pVp9HwContext->vconfig, pConfig, sizeof(VConfig));

    if(pVideoInfo->nWidth > 4096)
    {
        loge("the width is %d,height is %d\n", pVideoInfo->nWidth, pVideoInfo->nHeight);
        return VDECODE_RESULT_UNSUPPORTED;
    }

    pVp9HwContext->videoStreamInfo.eCodecFormat            = pVideoInfo->eCodecFormat;
    pVp9HwContext->videoStreamInfo.nWidth                  = pVideoInfo->nWidth;
    pVp9HwContext->videoStreamInfo.nHeight                 = pVideoInfo->nHeight;
    pVp9HwContext->videoStreamInfo.nFrameRate              = pVideoInfo->nFrameRate;
    pVp9HwContext->videoStreamInfo.nFrameDuration          = pVideoInfo->nFrameDuration;
    pVp9HwContext->videoStreamInfo.nAspectRatio            = pVideoInfo->nAspectRatio;
    pVp9HwContext->videoStreamInfo.nCodecSpecificDataLen   = pVideoInfo->nCodecSpecificDataLen;
    pVp9HwContext->videoStreamInfo.pCodecSpecificData      = pVideoInfo->pCodecSpecificData;
    pVp9HwContext->videoStreamInfo.bIs3DStream             = pVideoInfo->bIs3DStream;

    pVp9HwDecHandle = (Vp9HwDecode*)malloc(sizeof(Vp9HwDecode));
    if(pVp9HwDecHandle == NULL)
    {
        free(pVp9HwContext);
        pVp9HwContext = NULL;
        loge("Vp9Hw_open, malloc memory fail.");
        return VDECODE_RESULT_UNSUPPORTED;
    }
    memset(pVp9HwDecHandle, 0, sizeof(Vp9HwDecode));
    pVp9HwDecHandle->memops = pConfig->memops;
    pVp9HwDecHandle->veOpsS = pConfig->veOpsS;
    pVp9HwDecHandle->pVeOpsSelf = pConfig->pVeOpsSelf;
    pVp9HwDecHandle->bThumbnailMode = pConfig->bThumbnailMode;

    pVp9RegisterBaseAddr = (char*)CdcVeGetGroupRegAddr(pVp9HwContext->vconfig.veOpsS,
                                            pVp9HwContext->vconfig.pVeOpsSelf,
                                            REG_GROUP_VETOP);

    pVp9HwContext->pVp9HwDec = (void*)pVp9HwDecHandle;
    if(Vp9MallocDecodeBuffer(pVp9HwDecHandle) < 0)
    {
        free(pVp9HwDecHandle);
        pVp9HwDecHandle = NULL;
        pVp9HwContext->pVp9HwDec  = NULL;
        free(pVp9HwContext);
        pVp9HwContext = NULL;
        loge("Vp9Hw_open, malloc memory fail.");
        return VDECODE_RESULT_UNSUPPORTED;
    }
    //*******add for recycle sbm*************
    pVp9HwDecHandle->tempsbminfo.nSbmBufSize = 1024*1024*3/2;
    pVp9HwDecHandle->tempsbminfo.pSbmBuf = (u8*)CdcMemPalloc(pVp9HwDecHandle->memops,
                                                pVp9HwDecHandle->tempsbminfo.nSbmBufSize,
                                                (void *)pVp9HwContext->vconfig.veOpsS,
                                                pVp9HwContext->vconfig.pVeOpsSelf);
    pVp9HwDecHandle->tempsbminfo.nVbvBufPhyAddr =
        (uintptr_t)CdcMemGetPhysicAddress(pVp9HwDecHandle->memops,
                                          pVp9HwDecHandle->tempsbminfo.pSbmBuf);;

    pVp9HwDecHandle->tempsbminfo.nSbmDataSize = 0;
    //************************************

    pVp9HwContext->pFbm = NULL;
    pVp9HwContext->pFbm_scaledown = NULL;
    Vp9HwInitDecode(pVp9HwContext, pVp9HwContext->pVp9HwDec);
    return VDECODE_RESULT_OK;
}

/********************************************************************/
/*
          Name:    ve_reset
          Prototype: vresult_e ve_reset (u8 flush_pictures)
          Function:  Reset the VE CSP.
          Return:    VDECODE_RESULT_OK: success.
                       VDECODE_RESULT_UNSUPPORTED: the CSP is not opened yet.
*/
/********************************************************************/

void Vp9HwDecoderReset(DecoderInterface* pSelf)
{
    Vp9HwDecodeContext*pVp9HwContext=NULL;
    pVp9HwContext = (Vp9HwDecodeContext*)pSelf;

    if(pVp9HwContext == NULL)
    {
        return;
    }
    else
    {
        Vp9HwDecode*pVp9HwDec=NULL;
        pVp9HwDec = (Vp9HwDecode*)pVp9HwContext->pVp9HwDec;
        if(pVp9HwDec->sbmInfo.curStreamData != NULL)
        {
            SbmFlushStream(pVp9HwDec->sbmInfo.pSbm,
                pVp9HwDec->sbmInfo.curStreamData);
            pVp9HwDec->sbmInfo.curStreamData = NULL;
            pVp9HwDec->sbmInfo.pReadPtr = NULL;
            pVp9HwDec->sbmInfo.nSbmDataSize = 0;
        }
        Vp9HwFlushPictures(pVp9HwDec, pVp9HwContext->pFbm, pVp9HwContext->pFbm_scaledown, 0);
        Vp9HwDecodeSetSbmBuf(pVp9HwContext->pStreamBufferBase,
            pVp9HwContext->nStreamBufferSize, pVp9HwContext->pVp9HwDec);
        //ResetVeInternal(pVp9HwContext->pVideoEngine);
        Vp9HwResetDecodeParams(pVp9HwDec);
        return;
    }
}


/**********************************************************************/
/*
             Name:    ve_set_vbv
             Prototype: vresult_e ve_set_vbv (u8* vbv_buf, u32 nStreamBufferSize, Handle ve);
             Function: Set pSbm's bitstream buffer base address and buffer size to the CSP..
             Return:   VDECODE_RESULT_OK: success.
                         VDECODE_RESULT_UNSUPPORTED: the CSP is not opened yet.
*/
/***********************************************************************/

static int  Vp9HwDecoderSetSbm(DecoderInterface* pSelf, SbmInterface* pSbm, int nIndex)
{
    Vp9HwDecodeContext* pVp9HwContext=NULL;
    pVp9HwContext = (Vp9HwDecodeContext*)pSelf;

    if(nIndex != 0)
    {
        loge("multiple video stream unsupported.");
        return VDECODE_RESULT_UNSUPPORTED;
    }

    if(pVp9HwContext == NULL)
    {
        return VDECODE_RESULT_UNSUPPORTED;
    }
    else
    {
        Vp9HwDecode* pVp9HwDecHandle=NULL;
        pVp9HwDecHandle = (Vp9HwDecode*)pVp9HwContext->pVp9HwDec;
        pVp9HwContext->pSbm    = pSbm;
        pVp9HwContext->pStreamBufferBase = (u8*)SbmBufferAddress(pSbm);
        pVp9HwContext->nStreamBufferSize =      SbmBufferSize(pSbm);

        pVp9HwDecHandle->sbmInfo.pSbm         = pSbm;
        pVp9HwDecHandle->sbmInfo.pReadPtr     = pVp9HwContext->pStreamBufferBase;
        pVp9HwDecHandle->sbmInfo.pSbmBuf      = pVp9HwContext->pStreamBufferBase;
        pVp9HwDecHandle->sbmInfo.nSbmBufSize  = pVp9HwContext->nStreamBufferSize;
        pVp9HwDecHandle->sbmInfo.nSbmDataSize = 0;
        pVp9HwDecHandle->sbmInfo.pSbmBufEnd
            = pVp9HwContext->pStreamBufferBase+pVp9HwContext->nStreamBufferSize-1;
        return VDECODE_RESULT_OK;
    }
}

static int  Vp9HwDecoderGetFbmNum(DecoderInterface* pSelf)
{
    Vp9HwDecodeContext* pVp9HwContext;

    pVp9HwContext = (Vp9HwDecodeContext*)pSelf;

    if(pVp9HwContext->pFbm != NULL || pVp9HwContext->pFbm_scaledown != NULL)
        return 1;
    else
        return 0;
}

/*************************************************************************/
/*
             Name:    Vp9HwDecoderGetFbm
             Prototype: Handle get_fbm (void);
             Function: Get a handle of the pFbm instance, in which pictures for display are stored.
             Return:   Not NULL Handle: handle of the pFbm instance.
                       NULL Handle: pFbm module is not initialized yet.
*/
/************************************************************************/

static Fbm* Vp9HwDecoderGetFbm(DecoderInterface* pSelf, int nIndex)
{
    Vp9HwDecodeContext* pVp9HwContext = NULL;
    pVp9HwContext = (Vp9HwDecodeContext*)pSelf;

    if(nIndex != 0)
        return NULL;

    if(pVp9HwContext == NULL)
    {
        return NULL;
    }
    else
    {
        Vp9HwDecode* pVp9HwDec = NULL;
        pVp9HwDec = (Vp9HwDecode*)pVp9HwContext->pVp9HwDec;
        if(pVp9HwDec == NULL)
        {
            return NULL;
        }
        #if 0
        else if(pVp9HwDec->bMallocFrmBufFlag == 0)
        {
            return NULL;
        }
        #endif
        else
        {
            if(pVp9HwContext->pFbm_scaledown != NULL)
            {

                return pVp9HwContext->pFbm_scaledown;
            }
            else
            {
                return pVp9HwContext->pFbm;
            }
        }
    }
}


/******************************************************************************************/
//       Name:    ve_close                                                                  //
//       Prototype: vresult_e ve_close (u8 flush_pictures);                               //
//       Function:    Close the VE CSP.                                                     //
//       Return:    VDECODE_RESULT_OK: success.                                                  //
//         VDECODE_RESULT_UNSUPPORTED: the CSP is not opened yet.                         //
/******************************************************************************************/

static void Destroy(DecoderInterface* pSelf)
{
    //int i =0;
    Vp9HwDecodeContext* pVp9HwContext=NULL;
    pVp9HwContext = (Vp9HwDecodeContext*)pSelf;

    if(pVp9HwContext == NULL)
    {
        return;
    }
    else
    {
        Vp9HwDecode* pVp9HwDec = NULL;
        pVp9HwDec = (Vp9HwDecode*)pVp9HwContext->pVp9HwDec;

        if(pVp9HwDec == NULL)
        {
            return;
        }

        if(pVp9HwDec->tempsbminfo.pSbmBuf)
        {
            CdcMemPfree(pVp9HwDec->memops,pVp9HwDec->tempsbminfo.pSbmBuf,
                                                                pVp9HwDec->veOpsS,
                                                                pVp9HwDec->pVeOpsSelf);
            pVp9HwDec->tempsbminfo.pSbmBuf = NULL;
        }

        if(pVp9HwContext->pFbm)
        {
            FbmDestroy(pVp9HwContext->pFbm);
            pVp9HwContext->pFbm = NULL;
        }

        if(pVp9HwContext->pFbm_scaledown)
        {
            FbmDestroy(pVp9HwContext->pFbm_scaledown);
            pVp9HwContext->pFbm_scaledown = NULL;
        }

        Vp9ReleaseDecodeBuffer((Vp9HwDecode*)pVp9HwContext->pVp9HwDec);

        free(pVp9HwContext->pVp9HwDec);
        pVp9HwContext->pVp9HwDec = NULL;

        free(pVp9HwContext);
        pVp9HwContext = NULL;
        return;
    }
}

/***********************************************************************************/
/*
Name:    ve_decode
Prototype: vresult_e decode (vstream_data_t* stream, u8 keyframe_only,
               u8 bSkipBFrameIfDelay, u32 nCurrentTimeUs);
Function: Decode one bitstream frame.
INput:    vstream_data_t* stream: start address of the stream frame.
            u8 keyframe_only:   tell the CSP to decode key frame only;
            u8 bSkipBFrameIfDelay:  tell the CSP to skip B frame if it is overtime;
            u32 nCurrentTimeUs:current time, used to compare with PTS when decoding B frame;
Return:  VDECODE_RESULT_OK:             decode stream success but no frame decoded;
            VDECODE_RESULT_FRAME_DECODED:  one common frame decoded;
            VRESULT_KEYFRAME_DECODED:    one key frame decoded;
            VDECODE_RESULT_UNSUPPORTED:           decode stream fail;
            VRESULT_ERR_INVALID_PARAM:  either stream or ve is NULL;
            VRESULT_ERR_INVALID_STREAM: some error data in the stream, decode fail;
            VRESULT_ERR_NO_MEMORY:      allocate memory fail in this method;
            VRESULT_ERR_NO_FRAMEBUFFER: request empty frame buffer fail in this method;
            VRESULT_ERR_UNSUPPORTED:    stream format is unsupported by this version of VE CSP;
            VDECODE_RESULT_UNSUPPORTED:  'open' has not been successfully called yet.
*/
/***********************************************************************************/

static int  Vp9HwDecoderDecode(DecoderInterface* pSelf,
                            int               bEndOfStream,
                            int               bDecodeKeyFrameOnly,
                            int               bSkipBFrameIfDelay,
                            int64_t           nCurrentTimeUs)
{
    Vp9HwDecodeContext* pVp9HwContext = NULL;
    Vp9HwDecode*     pVp9HwDec = NULL;
    struct ScMemOpsS *_memops = NULL;
    VideoPicture* pVidPicture = NULL;
    struct Vp9DecContainer* dec_cont = NULL;
    s8  ret = 0;
    u32 data_len = 0;
    u32 first_part_len = 0;
    u32 second_part_len = 0;
    bDecodeKeyFrameOnly;
    bSkipBFrameIfDelay;
    nCurrentTimeUs;


    pVp9HwContext = (Vp9HwDecodeContext*)pSelf;
    pVp9HwDec = (Vp9HwDecode*)pVp9HwContext->pVp9HwDec;
    _memops = pVp9HwContext->vconfig.memops;
    pVp9HwDec->bEndOfStream = bEndOfStream;
    dec_cont = (struct Vp9DecContainer*)pVp9HwDec->dec_cont;

    //***************************************************//
    //**step1: prepare data
    //***************************************************//
    if(pVp9HwDec->nDecStep == VP9_DEC_PREPARE_DATA)
    {
        ret = Vp9RequestBitstreamData(pVp9HwContext, pVp9HwDec);
        //****************************************
        if(ret == VDECODE_RESULT_NO_BITSTREAM)
        {
            return ret;
        }

        //************add for recycle buffer***********

        dec_cont->sbm_back = 0;
        if(pVp9HwDec->sbmInfo.pReadPtr +  pVp9HwDec->sbmInfo.nSbmDataSize >
              pVp9HwDec->sbmInfo.pSbmBufEnd)
           {
              if( pVp9HwDec->tempsbminfo.nSbmBufSize < pVp9HwDec->sbmInfo.nSbmDataSize)
                  {
                      logd("here1: pVp9HwDec->tempsbminfo.nSbmBufSize=%d, nSbmDataSize=%d\n",
                    pVp9HwDec->tempsbminfo.nSbmBufSize, pVp9HwDec->sbmInfo.nSbmDataSize);
                CdcMemPfree(pVp9HwDec->memops,pVp9HwDec->tempsbminfo.pSbmBuf,
                                              pVp9HwContext->vconfig.veOpsS,
                                              pVp9HwContext->vconfig.pVeOpsSelf);
                pVp9HwDec->tempsbminfo.nSbmBufSize = pVp9HwDec->sbmInfo.nSbmDataSize+1024;
                pVp9HwDec->tempsbminfo.pSbmBuf = (u8*)CdcMemPalloc(pVp9HwDec->memops,
                                                            pVp9HwDec->tempsbminfo.nSbmBufSize,
                                                            (void *)pVp9HwContext->vconfig.veOpsS,
                                                            pVp9HwContext->vconfig.pVeOpsSelf);
                if(pVp9HwDec->tempsbminfo.pSbmBuf == NULL)
                {
                    return VDECODE_RESULT_UNSUPPORTED;
                }
                pVp9HwDec->tempsbminfo.nVbvBufPhyAddr =
                (uintptr_t)CdcMemGetPhysicAddress(_memops,pVp9HwDec->tempsbminfo.pSbmBuf);;
                  }
              dec_cont->sbm_back = 1;
           first_part_len = pVp9HwDec->sbmInfo.pSbmBufEnd - pVp9HwDec->sbmInfo.pReadPtr +1;
           second_part_len = pVp9HwDec->sbmInfo.nSbmDataSize - first_part_len;

           CdcMemCopy(pVp9HwDec->memops, pVp9HwDec->tempsbminfo.pSbmBuf,
                      pVp9HwDec->sbmInfo.pReadPtr,first_part_len);
           CdcMemCopy(pVp9HwDec->memops, pVp9HwDec->tempsbminfo.pSbmBuf + first_part_len,
                      pVp9HwDec->sbmInfo.pSbmBuf,second_part_len);
           CdcMemFlushCache(pVp9HwDec->memops, pVp9HwDec->tempsbminfo.pSbmBuf,
                               pVp9HwDec->sbmInfo.nSbmDataSize);
           }

        pVp9HwDec->sbmInfo.nDataSize = pVp9HwDec->sbmInfo.curStreamData->nLength;
        pVp9HwDec->sbmInfo.pDataPtr = (u8*)pVp9HwDec->sbmInfo.curStreamData->pData;
        if(dec_cont->sbm_back == 1)
        {
            pVp9HwDec->sbmInfo.pDataPtr = pVp9HwDec->tempsbminfo.pSbmBuf;
        }

        Vp9ProcessDataLength(pVp9HwDec->sbmInfo.pDataPtr,pVp9HwDec->sbmInfo.nDataSize,
            &pVp9HwDec->sbmInfo.nFrameNums,pVp9HwDec->sbmInfo.real_data_len);

        pVp9HwDec->nDecStep = VP9_DEC_PICTURE_HERDER;

    }


    //***************************************************//
    //**step2: decode frame header
    //***************************************************//
    if(pVp9HwDec->nDecStep == VP9_DEC_PICTURE_HERDER)
    {
        ret = Vp9ProcessHeaders(pVp9HwContext, pVp9HwDec->sbmInfo.pDataPtr,
                pVp9HwDec->sbmInfo.real_data_len[pVp9HwDec->sbmInfo.nFrameNums-1]);
        if(ret < 0)
        {
            loge("Vp9ProcessHeaders have some trouble! ret=%d\n", ret);
        }
        if(pVp9HwContext->pFbm == NULL)
        {
            Vp9CreateFbmBuffer(pVp9HwContext);
        }
        pVp9HwDec->nDecStep = VP9_DEC_PREPARE_FRAME_BUFFER;
    }

    //***************************************************//
    //**step3: prepare frame buffer
    //***************************************************//

    if(pVp9HwDec->nDecStep == VP9_DEC_PREPARE_FRAME_BUFFER)
    {
        if(pVp9HwContext->pFbm != NULL && pVp9HwDec->pCurFrm == NULL)
        {
            pVidPicture = FbmRequestBuffer(pVp9HwContext->pFbm);
            if(pVidPicture == NULL)
            {
               return VDECODE_RESULT_NO_FRAME_BUFFER;
            }

            #if 0
            if(dec_cont->pic_number < 0)
                {
                    char name[128];
                    FILE* fp = NULL;
                    sprintf(name, "/data/camera/vp9_%d.dat",dec_cont->pic_number);
                    fp = fopen(name, "wb");
                    if(fp != NULL)
                    {
                       fwrite(pVidPicture->pData0, 1,
                              pVidPicture->nWidth*pVidPicture->nHeight*3, fp);
                       fclose(fp);
                    }
                }
            #endif
            pVp9HwDec->pCurFrm = pVidPicture;
        }
        pVp9HwDec->nDecStep = VP9_DEC_GET_FBM_BUFFER;
    }

    //***************************************************//
    //**step4: get fbm empty buffer
    //***************************************************//
    if(pVp9HwDec->nDecStep == VP9_DEC_GET_FBM_BUFFER)
    {
        ret = Vp9GetRefFrm(pVp9HwContext);
        if(ret)
        {
            logd("***********Vp9GetRefFrm error!\n");
            return ret;
        }
        ret = Vp9AsicAllocateFilterBlockMem(pVp9HwDec->dec_cont);
        if(ret)
        {
            logd("***********Vp9AsicAllocateFilterBlockMem error!\n");
            return ret;
        }
        pVp9HwDec->nDecStep = VP9_DEC_PREPARE_ASIC;
    }

    //***************************************************//
    //**step5: prepare asic
    //***************************************************//
    if(pVp9HwDec->nDecStep == VP9_DEC_PREPARE_ASIC)
    {
        uintptr_t stream_bus_address = 0;
        _memops = pVp9HwContext->vconfig.memops;
        pVp9HwDec->sbmInfo.nFrameNums -= 1;
        data_len = pVp9HwDec->sbmInfo.real_data_len[pVp9HwDec->sbmInfo.nFrameNums];

        if(dec_cont->sbm_back == 0)
        {
            stream_bus_address
                = (uintptr_t)CdcMemGetPhysicAddress(_memops,pVp9HwDec->sbmInfo.pSbmBuf);
            stream_bus_address += pVp9HwDec->sbmInfo.pDataPtr-pVp9HwDec->sbmInfo.pSbmBuf;
        }
        else
        {
            stream_bus_address = pVp9HwDec->tempsbminfo.nVbvBufPhyAddr;
            stream_bus_address += pVp9HwDec->sbmInfo.pDataPtr-pVp9HwDec->tempsbminfo.pSbmBuf;
        }
        Vp9AsicProbUpdate(pVp9HwDec->dec_cont);

        Vp9AsicInitPicture(pVp9HwDec->dec_cont); //**set ref frame**

        Vp9AsicStrmPosUpdate(pVp9HwDec->dec_cont,stream_bus_address, data_len);

        Vp9AsicRun(pVp9HwDec->dec_cont);

        //* wait ve interrupt here
        ret = CdcVeWaitInterrupt(pVp9HwContext->vconfig.veOpsS, pVp9HwContext->vconfig.pVeOpsSelf);
        if(ret == -1)
        {
            logd("AdapterVeWaitInterrupt,ret == -1!!!\n");
        }

      #if 0
            logd("**************print the VP9 register after AdapterVeWaitInterrupt!!!\n");
            unsigned int * nRegisterBaseAddr = NULL;
            int i = 0;
            nRegisterBaseAddr = (unsigned int*)ve_get_reglist(REG_GROUP_VETOP);
            for(i=0; i<184; i++)
            {
               logd("read reg, i = %d, value = 0x%08x",i,*(nRegisterBaseAddr+i));
            }
      #endif
       VP9CongigureDisplayParameters(pVp9HwContext);

      #if 0
       Vp9SaveDecodeFrame(pVp9HwDec->dec_cont);
      #endif

       pVp9HwDec->sbmInfo.pDataPtr
            += pVp9HwDec->sbmInfo.real_data_len[pVp9HwDec->sbmInfo.nFrameNums];
       pVp9HwDec->sbmInfo.nDataSize
            -= (s32)pVp9HwDec->sbmInfo.real_data_len[pVp9HwDec->sbmInfo.nFrameNums];
       pVp9HwDec->nDecStep = VP9_DEC_PROCESS_DEC_RESULT;
    }
    //***************************************************//
    //**step6: process decode result
    //***************************************************//
    if(pVp9HwDec->nDecStep == VP9_DEC_PROCESS_DEC_RESULT)
    {
        ret = VP9SyncAndOutput(pVp9HwDec->dec_cont,pVp9HwContext->pFbm, pVp9HwDec->pCurFrm);
        pVp9HwDec->pCurFrm = NULL;
        if(ret)
        {
            logd("*****faile: VP9SyncAndOutput error\n");
            return ret;
        }
        //logd("********************dec_cont->pic_number =%d\n", dec_cont->pic_number);
        dec_cont->pic_number += 1;

        if(pVp9HwDec->sbmInfo.nFrameNums != 0)
        {
            pVp9HwDec->nDecStep = VP9_DEC_PICTURE_HERDER;
        }
        else
        {
            SbmFlushStream(pVp9HwDec->sbmInfo.pSbm,
            pVp9HwDec->sbmInfo.curStreamData);
            pVp9HwDec->sbmInfo.curStreamData = NULL;
            pVp9HwDec->sbmInfo.pReadPtr = NULL;
            pVp9HwDec->sbmInfo.nSbmDataSize = 0;
            pVp9HwDec->nDecStep = VP9_DEC_PREPARE_DATA;
        }
    }
    if(dec_cont->decoder.key_frame)
    {
        return VDECODE_RESULT_KEYFRAME_DECODED;
    }
    else
    {
        return VDECODE_RESULT_FRAME_DECODED;
    }
}

