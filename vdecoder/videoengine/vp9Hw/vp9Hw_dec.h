/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : vp9Hw_dec.h
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2017/04/13
*   Comment :
*
*
*/

#ifndef VP9HW_DEC_H
#define VP9HW_DEC_H

#include "vp9Hw.h"
#include "vp9hwd_decapi.h"

#ifdef __cplusplus
extern "C" {
#endif

enum VP9_DEC_STEP
{
    VP9_DEC_PREPARE_DATA = 0,
    VP9_DEC_PREPARE_FRAME_BUFFER = 1,
    VP9_DEC_SYNC_OUTPUT  = 2,
    VP9_DEC_PICTURE_HERDER = 3,
    VP9_DEC_GET_FBM_BUFFER = 4,
    VP9_DEC_PREPARE_ASIC = 5,
    VP9_DEC_PROCESS_DEC_RESULT = 6,
};

typedef struct VP9_VBV
{
    SbmInterface*  pSbm;
    VideoStreamDataInfo* curStreamData;
    u8* pSbmBuf;           // the start address of the vbv buffer
    u8* pReadPtr;          // the current read pos32er of the vbv buffer
    u8* pSbmBufEnd;        // the end address of the vbv buffer
    u8* pDataPtr;
    u32 nSbmBufSize;       // the total size of the vbv buffer
    u32 nSbmDataSize;      // the valid data size of the vbv buffer
    //u32 real_data_len;
    s32 nDataSize;
    u64 nSbmDataPts;
    u64 validDataPts;
    u32 nVbvBufEndPhyAddr;
    u32 nVbvBufPhyAddr;
    u8  nFrameNums;
    u32 real_data_len[4];
 }Vp9Sbm;


typedef struct VP9_TEMP_VBV
{
    u8* pSbmBuf;           // the start address of the vbv buffer
    u32 nSbmBufSize;       // the total size of the vbv buffer
    u32 nSbmDataSize;      // the valid data size of the vbv buffer
    uintptr_t nVbvBufPhyAddr;
}Vp9empSbm;

typedef struct Vp9Hw_DECODER
{
    struct ScMemOpsS* memops;
    Vp9Sbm   sbmInfo;
    Vp9empSbm tempsbminfo;
    u8       bMallocFrmBufFlag;
    u8       nDecStep;
    u8       bEndOfStream;
    u8       bNeedKeyFrame;

    s32      nDispCBufferSize;
    s32      nRefCBufferSize;
    s32      nRefYLineStride;
    s32      nRefCLineStride;
    s32      nDispYLineStride;
    s32      nDispCLineStride;

    u64 CurFramePts;
    u64 PreFramePts;

    #if 0
    void*    pVp9Decoder;
    void*    pAnciBuffer;
    void*    pBc;
    #endif

    u32      nDecFrmNum;
    void*    dec_cont;
    VideoPicture*     pCurFrm;
    VeOpsS*           veOpsS;
    void*             pVeOpsSelf;
     s32   bThumbnailMode;
}Vp9HwDecode;


#ifdef __cplusplus
}
#endif

#endif

