/*
* Copyright (c) 2008-2020 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : veAdapter.c
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2016/04/13
*   Comment :
*
*
*/

#include "veInterface.h"
#include "veAdapter.h"
#include "veAw/veAwEntry.h"
#include "veVp9/veVp9Entry.h"
#include "log.h"

VeOpsS* GetVeOpsS(int type)
{

    if(type == VE_OPS_TYPE_NORMAL)
    {
        return getVeAwOpsS();
    }
    else if(type == VE_OPS_TYPE_VP9)
    {
        return getVeVp9OpsS();
    }
    else
    {
        loge("getVeOpsS: not suppurt type = %d",type);
        return NULL;
    }
}

