/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : h265_drv.h
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2016/04/13
*   Comment :
*
*
*/

#ifndef H265_DRV_H
#define H265_DRV_H

#include "h265_config.h"
#include <semaphore.h>
#include "h265_var.h"

#ifdef __cplusplus
extern "C" {
#endif
// interface to decode_interface


typedef struct H265_DEC_CTX
{
    DecoderInterface    interface;
    VideoEngine*        pVideoEngine;
    VConfig             vconfig;   //* decode library configuration
    VideoStreamInfo     videoStreamInfo;   //* video stream information;

    HevcContex            *pHevcDec;
    VideoFbmInfo*       pFbmInfo;
}H265DecCtx;

#ifdef __cplusplus
}
#endif

#endif
