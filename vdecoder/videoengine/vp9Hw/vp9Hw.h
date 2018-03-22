/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : vp9Hw.h
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2016/04/13
*   Comment :
*
*
*/

#ifndef VP9HW_H
#define VP9HW_H


#ifdef __cplusplus
extern "C" {
#endif
#include "vp9Hw_config.h"


    typedef struct VP9HW_DECODER_CONTEXT
    {
        DecoderInterface    interface;
        VideoEngine*        pVideoEngine;
        VConfig             vconfig;           //* decode library configuration;
        VideoStreamInfo     videoStreamInfo;   //* video stream information;
        void*               pVp9HwDec;         //* vp9 Hw decoder handle

        //* memory for hardware using.
        u8*                 pStreamBufferBase;
        u32                 nStreamBufferSize;
        SbmInterface*       pSbm;
        Fbm*                pFbm;
        Fbm*                pFbm_scaledown;
        //VideoPicture*       pCurFrm;
        VideoFbmInfo*       pFbmInfo;
    }Vp9HwDecodeContext;

#ifdef __cplusplus
}
#endif

#endif

