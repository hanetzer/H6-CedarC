
/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : h264_config.h
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2016/04/13
*   Comment :
*
*
*/

#ifndef H264_V2_CONFIG_H
#define H264_V2_CONFIG_H

#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include "typedef.h"
#include "videoengine.h"
#include "veAwTopRegDef.h"

#ifdef __cplusplus
extern "C" {
#endif

#define H264_DEBUG_SAVE_BIT_STREAM 0
#define H264_DEBUG_SAVE_BIT_STREAM_ES 0

#define H264_DEBUG_SAVE_PIC 0
#define H264_DEBUG_SHOW_FPS 0
#define H264_DEBUG_SHOW_BIT_RATE 0
#define H264_DEBUG_FRAME_CYCLE 32

#define H264_DEBUG_PRINT_REF_LIST 0
#define H264_DEBUG_PRINTF_REGISTER 0
#define H264_DEBUG_COMPUTE_PIC_MD5 0

#define AVC_SAVE_STREAM_PATH "/data/camera/avcstream.dat"

#ifdef __cplusplus
}
#endif

#endif

