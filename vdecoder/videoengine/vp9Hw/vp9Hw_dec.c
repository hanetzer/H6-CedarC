/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : vp9Hw_dec.c
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2017/04/13
*   Comment :
*
*
*/

#include "vp9Hw_dec.h"
#include "vp9hwd_stream.h"
#include "vp9hwd_decoder.h"
#include "vp9hwd_bool.h"
#include "vp9hwd_container.h"
#include "vp9hwd_asic.h"
#include "vp9Sha.h"
#include <sys/time.h>

extern u32 CheckSyncCode(struct StrmData *rb);
extern void ExistingRefRegister(struct Vp9Decoder *dec, u32 map);
//extern void SetupFrameSize(struct StrmData *rb, struct Vp9Decoder *dec);
extern void SetupFrameSize(struct StrmData *rb, Vp9HwDecodeContext* pVp9HwContext);
//extern void Vp9CreateFbmBuffer(Vp9HwDecodeContext* pVp9HwContext, int width, int height);
extern void Vp9GetProbs(struct Vp9Decoder *dec);

//extern int SetupFrameSizeWithRefs(struct StrmData *rb,
   // struct Vp9DecContainer *dec_cont);
 extern int SetupFrameSizeWithRefs(struct StrmData *rb,
    Vp9HwDecodeContext* pVp9HwContext );
extern void DecodeLfParams(struct StrmData *rb, struct Vp9Decoder *dec);
extern s32 DecodeQuantizerDelta(struct StrmData *rb);
extern void DecodeSegmentationData(struct StrmData *rb, struct Vp9Decoder *dec);

extern void GetTileNBits(struct Vp9Decoder *dec, u32 *min_log2_ntiles_ptr,
                  u32 *delta_log2_ntiles);
extern u32 Vp9DecodeFrameHeader(const u8 *stream, u32 strm_len, struct VpBoolCoder *bc,
                         struct Vp9Decoder *dec);
extern u32 Vp9SetPartitionOffsets(const u8 *stream, u32 len, struct Vp9Decoder *dec);
extern void Vp9PicToOutput(struct Vp9DecContainer *dec_cont);
void Vp9DecRelease(Vp9DecInst dec_inst);

//**************************************************************************//
//**************************************************************************//
#define NEXT_MULTIPLE(value, n) (((value) + (n) - 1) & ~((n) - 1))

void Vp9HwInitDecode(Vp9HwDecodeContext* pVp9HwContext, Vp9HwDecode* pVp9HwDec)
{
    enum DecPictureFormat format;
    enum DecErrorConcealment concealmentMode;

    switch(pVp9HwContext->vconfig.eOutputPixelFormat)
    {
        case PIXEL_FORMAT_YV12:
        {
            format = DEC_OUT_FRM_PLANAR_420;
            break;
        }
        case PIXEL_FORMAT_NV12:
        {
            format = DEC_OUT_FRM_RASTER_SCAN;
            break;
        }
        case PIXEL_FORMAT_NV21:
        {
            format = DEC_OUT_FRM_RASTER_SCAN;
            break;
        }
        default:
        {
            pVp9HwContext->vconfig.eOutputPixelFormat = PIXEL_FORMAT_YV12;
            format = DEC_OUT_FRM_PLANAR_420;
            break;
        }
    }
    concealmentMode = DEC_PICTURE_FREEZE;
    pVp9HwDec->PreFramePts = 0;

}

void Vp9HwFlushPictures(Vp9HwDecode* pVp9HwDec, Fbm* pFbm,
                  Fbm* pFbmScaleDown, u8 bDispFlag)
{
      pFbmScaleDown;
      if(pVp9HwDec->pCurFrm != NULL)
      {
          VideoPicture* pVPicture;
          pVPicture = pVp9HwDec->pCurFrm;
          FbmReturnBuffer(pFbm, pVPicture, bDispFlag);
          pVp9HwDec->pCurFrm = NULL;
      }
}

void Vp9HwDecodeSetSbmBuf(u8* pSbmBase, u32 nSbmSize, Vp9HwDecode* pVp9HwDec)
{
    //s16 i = 0;
    pVp9HwDec->sbmInfo.pSbmBuf      = pSbmBase;
    pVp9HwDec->sbmInfo.nSbmBufSize  = nSbmSize;
    pVp9HwDec->sbmInfo.pSbmBufEnd   = pVp9HwDec->sbmInfo.pSbmBuf + nSbmSize -1;
    pVp9HwDec->sbmInfo.nSbmDataSize = 0;
    pVp9HwDec->sbmInfo.pReadPtr     = pSbmBase;
    pVp9HwDec->sbmInfo.curStreamData = NULL;
}

void Vp9HwResetDecodeParams(Vp9HwDecode* pVp9HwDec)
{
    pVp9HwDec->nDecStep = VP9_DEC_PREPARE_DATA;
    pVp9HwDec->PreFramePts = 0;
}

s32 Vp9RequestBitstreamData(Vp9HwDecodeContext* pVp9HwContext, Vp9HwDecode* pVp9HwDec)
{
    VideoStreamDataInfo* newStreamData = NULL;
    //u8 *pVbvDataEndPtr = NULL;

    while(1)
    {
        newStreamData = SbmRequestStream(pVp9HwDec->sbmInfo.pSbm);
        if(newStreamData == NULL)
        {
            if(pVp9HwDec->bEndOfStream == 1)
            {
                Vp9HwFlushPictures(pVp9HwDec,pVp9HwContext->pFbm,
                    pVp9HwContext->pFbm_scaledown, 1);
            }
            return VDECODE_RESULT_NO_BITSTREAM;
        }

        if(newStreamData->nLength == 0)
        {
            logw("newStreamData->nLength == 0!!!\n");
            SbmFlushStream(pVp9HwDec->sbmInfo.pSbm, newStreamData);
            newStreamData = NULL;
            continue;
        }
        break;
    }

    pVp9HwDec->sbmInfo.curStreamData = newStreamData;
    pVp9HwDec->sbmInfo.nSbmDataSize = newStreamData->nLength;
    pVp9HwDec->sbmInfo.pReadPtr = (u8*)newStreamData->pData;
    pVp9HwDec->sbmInfo.validDataPts = newStreamData->nPts;
    pVp9HwDec->CurFramePts = pVp9HwDec->sbmInfo.validDataPts;
    return VDECODE_RESULT_OK;
}

u32 Vp9DecodeFrameTag(Vp9HwDecodeContext* pVp9HwContext, u8 *strm, u32 dataLen)
{
    /* Use common bit parse but don't remove emulation prevention bytes. */
    u32 key_frame = 0;
    u32 err, tmp, i, delta_log2_tiles;
    struct DecAsicBuffers* pAnciBuffer = NULL;
    Vp9HwDecode*     pVp9HwDec = NULL;
    struct Vp9DecContainer* dec_cont = NULL;
    struct Vp9Decoder* dec = NULL;

    pVp9HwDec = (Vp9HwDecode*)pVp9HwContext->pVp9HwDec;
    //dec_cont = (struct Vp9DecContainer*)decContainer;
    dec_cont = (struct Vp9DecContainer*)pVp9HwDec->dec_cont;

    struct StrmData rb = {strm, strm, 0, dataLen, 0, 1, 0};

    dec = (struct Vp9Decoder*)&dec_cont->decoder;
    pAnciBuffer = ( struct DecAsicBuffers*)dec_cont->asic_buff;

    tmp = SwGetBits(&rb, 2);

    if(tmp == END_OF_STREAM)
    {
        loge("****here1:Vp9DecodeFrameTag failed\n");
        return HANTRO_NOK;
    }

    dec->vp_version = SwGetBits(&rb, 1);
    dec->vp_profile = (SwGetBits(&rb, 1)<<1) + dec->vp_version;
    if(dec->vp_profile>2)
    {
        dec->vp_profile += SwGetBits(&rb, 1);
    }

    dec->reset_frame_flags = 0;
    dec->show_existing_frame = SwGetBits(&rb, 1);
    if(dec->show_existing_frame)
    {
        s32 idx_to_show = SwGetBits(&rb, 3);
        dec->show_existing_frame_index = dec->ref_frame_map[idx_to_show];
        dec->refresh_frame_flags = 0;
        dec->loop_filter_level = 0;
        return HANTRO_OK;
    }

    key_frame = SwGetBits(&rb, 1);
    dec->key_frame = !key_frame;
    dec->show_frame = SwGetBits(&rb, 1);
    dec->error_resilient = SwGetBits(&rb, 1);
    if(dec->error_resilient == END_OF_STREAM)
    {
        loge("****here1:Vp9DecodeFrameTag failed\n");
        return HANTRO_NOK;
    }

    if(dec->key_frame)
    {
        err = CheckSyncCode(&rb);
        if(err == HANTRO_NOK)
        {
            loge("****here2:Vp9DecodeFrameTag failed\n");
            return err;
        }
        dec->bit_depth = dec->vp_profile>=2? SwGetBits(&rb, 1)? 12 : 10 : 8;

        dec->color_space = SwGetBits(&rb, 3);

        if(dec->color_space != VP9_RGB) /* != s_rgb */
        {
            tmp = SwGetBits(&rb, 1);
            if(dec->vp_version == 1)
            {
                dec->subsampling_x = SwGetBits(&rb, 1);
                dec->subsampling_y = SwGetBits(&rb, 1);
                tmp = SwGetBits(&rb, 1);
            }
            else
            {
                dec->subsampling_x = dec->subsampling_y = 1;
            }
        }
        else
        {
            if(dec->vp_version == 1)
            {
                dec->subsampling_x = dec->subsampling_y = 0;
                tmp = SwGetBits(&rb, 1);
                logd("Alpha plane=%d\n", tmp);
            }
            else
            {
                logd("RGB not supported in profile\n");
                loge("****here3:Vp9DecodeFrameTag failed\n");
                return HANTRO_NOK;
            }
        }

        dec->reset_frame_flags = 1;
        dec->refresh_frame_flags = (1 << NUM_REF_FRAMES) - 1;

        for(i = 0; i < ALLOWED_REFS_PER_FRAME; i++)
        {
            dec->active_ref_idx[i] = 0; /* TODO next free frame buffer */
            ExistingRefRegister(dec, 1<<i);
        }
        //SetupFrameSize(&rb, dec);
        SetupFrameSize(&rb, pVp9HwContext);
        if(dec_cont->initial_bitdepth == 0 /* Not set */)
        {
            dec_cont->initial_bitdepth = dec->bit_depth;
        }
    }
    else
    {
        if(!dec->show_frame)
        {
            dec->intra_only = SwGetBits(&rb, 1);
        }
        else
        {
            dec->intra_only = 0;
        }
        if(dec_cont->dec_stat == VP9DEC_INITIALIZED && dec->intra_only == 0)
        {
            /* we need an intra frame before decoding anything else */
            loge("****here4:Vp9DecodeFrameTag failed\n");
            return HANTRO_NOK;
        }

        if(!dec->error_resilient)
        {
            dec->reset_frame_context = SwGetBits(&rb, 2);
        }
        else
        {
            dec->reset_frame_context = 0;
        }

        if(dec->intra_only)
        {
            err = CheckSyncCode(&rb);
            if(err == HANTRO_NOK)
            {
                loge("****here5:Vp9DecodeFrameTag failed\n");
                return err;
            }
            dec->bit_depth = dec->vp_profile>=2? SwGetBits(&rb, 1)? 12 : 10 : 8;
            if(dec->vp_profile > 0)
            {
                dec->color_space = SwGetBits(&rb, 3);
                STREAM_TRACE("Color space", dec->color_space);
                if(dec->color_space != VP9_RGB) /* != s_rgb */
                {
                    tmp = SwGetBits(&rb, 1);
                    STREAM_TRACE("YUV range", tmp);
                    if(dec->vp_version == 1)
                    {
                        dec->subsampling_x = SwGetBits(&rb, 1);
                        dec->subsampling_y = SwGetBits(&rb, 1);
                        tmp = SwGetBits(&rb, 1);
                        STREAM_TRACE("Subsampling X", dec->subsampling_x);
                        STREAM_TRACE("Subsampling Y", dec->subsampling_y);
                        STREAM_TRACE("Alpha plane", tmp);
                    }
                    else
                    {
                        dec->subsampling_x = dec->subsampling_y = 1;
                    }
                }
                else
                {
                    if(dec->vp_version == 1)
                    {
                        dec->subsampling_x = dec->subsampling_y = 0;
                        tmp = SwGetBits(&rb, 1);
                        STREAM_TRACE("Alpha plane", tmp);
                    }
                    else
                    {
                        STREAM_TRACE("RGB not supported in profile", 0);
                        loge("****here6:Vp9DecodeFrameTag failed\n");
                        return HANTRO_NOK;
                    }
                }
            }
            /* Refresh reference frame flags */
            dec->refresh_frame_flags = SwGetBits(&rb, NUM_REF_FRAMES);
            //SetupFrameSize(&rb, dec);
            SetupFrameSize(&rb, pVp9HwContext);
            if(dec_cont->initial_bitdepth == 0 /* Not set */)
            {
                dec_cont->initial_bitdepth = dec->bit_depth;
            }
        }
        else
        {
            /* Refresh reference frame flags */
            dec->refresh_frame_flags = SwGetBits(&rb, NUM_REF_FRAMES);
            for(i = 0; i < ALLOWED_REFS_PER_FRAME; i++)
            {
                s32 ref_frame_num = SwGetBits(&rb, NUM_REF_FRAMES_LG2);
                s32 mapped_ref = dec->ref_frame_map[ref_frame_num];
                //logd("active_reference_frame_num=%d\n", mapped_ref);
                dec->active_ref_idx[i] = mapped_ref;
                dec->ref_frame_sign_bias[i + 1] = SwGetBits(&rb, 1);
            }

            //if(SetupFrameSizeWithRefs(&rb, dec_cont))
            if(SetupFrameSizeWithRefs(&rb, pVp9HwContext))
            {
                loge("****here7:Vp9DecodeFrameTag failed\n");
                return HANTRO_NOK;
            }
            dec->allow_high_precision_mv = SwGetBits(&rb, 1);

            tmp = SwGetBits(&rb, 1);
            if(tmp)
            {
                dec->mcomp_filter_type = SWITCHABLE;
            }
            else
            {
                dec->mcomp_filter_type = SwGetBits(&rb, 2);
            }

            dec->allow_comp_inter_inter = 0;
            for(i = 0; i < ALLOWED_REFS_PER_FRAME; i++)
            {
                dec->allow_comp_inter_inter |=
                    (i > 0) &&
                    (dec->ref_frame_sign_bias[i + 1] != dec->ref_frame_sign_bias[1]);
            }

            /* Setup reference frames for compound prediction. */
            if(dec->allow_comp_inter_inter)
            {
                if (dec->ref_frame_sign_bias[LAST_FRAME] ==
                    dec->ref_frame_sign_bias[GOLDEN_FRAME])
                {
                    dec->comp_fixed_ref = ALTREF_FRAME;
                    dec->comp_var_ref[0] = LAST_FRAME;
                    dec->comp_var_ref[1] = GOLDEN_FRAME;
                }
                else if(dec->ref_frame_sign_bias[LAST_FRAME] ==
                    dec->ref_frame_sign_bias[ALTREF_FRAME])
                {
                    dec->comp_fixed_ref = GOLDEN_FRAME;
                    dec->comp_var_ref[0] = LAST_FRAME;
                    dec->comp_var_ref[1] = ALTREF_FRAME;
                }
                else
                {
                    dec->comp_fixed_ref = LAST_FRAME;
                    dec->comp_var_ref[0] = GOLDEN_FRAME;
                    dec->comp_var_ref[1] = ALTREF_FRAME;
                }
            }
        }
        ExistingRefRegister(dec, dec->refresh_frame_flags);
    }

    if(!dec->error_resilient)
    {
        /* Refresh entropy probs,
        * 0 == this frame probs are used only for this frame decoding,
        * 1 == this frame probs will be stored for future reference */
        dec->refresh_entropy_probs = SwGetBits(&rb, 1);
        dec->frame_parallel_decoding = SwGetBits(&rb, 1);
    }
    else
    {
        dec->refresh_entropy_probs = 0;
        dec->frame_parallel_decoding = 1;
    }
    dec->frame_context_idx = SwGetBits(&rb, NUM_FRAME_CONTEXTS_LG2);
    if(dec->key_frame || dec->error_resilient || dec->intra_only)
    {
        Vp9ResetDecoder(dec, pAnciBuffer);
    }

    Vp9GetProbs(dec);
    DecodeLfParams(&rb, dec);
    /* Quantizers */
    dec->qp_yac = SwGetBits(&rb, 8);
    dec->qp_ydc = DecodeQuantizerDelta(&rb);
    dec->qp_ch_dc = DecodeQuantizerDelta(&rb);
    dec->qp_ch_ac = DecodeQuantizerDelta(&rb);
    dec->lossless = dec->qp_yac == 0 && dec->qp_ydc == 0 &&
        dec->qp_ch_dc == 0 && dec->qp_ch_ac == 0;
    /* Setup segment based adjustments */
    DecodeSegmentationData(&rb, dec);
    /* Tile dimensions */
    GetTileNBits(dec, &dec->log2_tile_columns, &delta_log2_tiles);
    while (delta_log2_tiles--)
    {
        tmp = SwGetBits(&rb, 1);
        if(tmp == END_OF_STREAM)
        {
            loge("****here8:Vp9DecodeFrameTag failed\n");
            return HANTRO_NOK;
        }
        if(tmp)
        {
            dec->log2_tile_columns++;
        }
        else
        {
            break;
        }
    }

    dec->log2_tile_rows = SwGetBits(&rb, 1);
    if(dec->log2_tile_rows)
        dec->log2_tile_rows += SwGetBits(&rb, 1);
    logi("*********xyliu1:tile_cols: show_bits=%x\n", SwShowBits(&rb, 32));

    /* Size of frame headers */
    dec->offset_to_dct_parts = SwGetBits(&rb, 16);
    if(dec->offset_to_dct_parts == END_OF_STREAM)
    {
        loge("****here9:Vp9DecodeFrameTag failed\n");
        return HANTRO_NOK;
    }
    dec->frame_tag_size = (rb.strm_buff_read_bits + 7) / 8;
    return HANTRO_OK;
}

//s32 Vp9DecodeHeaders(void* decContainer, u8* pDataPtr, u32 nDataLen)
s32 Vp9DecodeHeaders(Vp9HwDecodeContext* pVp9HwContext, u8* pDataPtr, u32 nDataLen)
{
    s32 ret = 0;
    Vp9HwDecode*     pVp9HwDec = NULL;
    struct Vp9DecContainer* dec_cont = NULL;
    struct Vp9Decoder* dec = NULL;
    struct DecAsicBuffers* pAnciBuffer = NULL;

    pVp9HwDec = (Vp9HwDecode*)pVp9HwContext->pVp9HwDec;
    dec_cont = (struct Vp9DecContainer*)pVp9HwDec->dec_cont;
    dec = (struct Vp9Decoder*)&dec_cont->decoder;
    pAnciBuffer = (struct DecAsicBuffers*)dec_cont->asic_buff;

    dec_cont->prev_is_key = dec->key_frame;
    dec->prev_is_key_frame = dec->key_frame;
    dec->prev_show_frame = dec->show_frame;
    dec->probs_decoded = 0;

    dec->last_width = dec->width;
    dec->last_height = dec->height;

    /* decode frame tag */
    //ret = Vp9DecodeFrameTag(dec_cont,pDataPtr, nDataLen);
    ret = Vp9DecodeFrameTag(pVp9HwContext,pDataPtr, nDataLen);

    if(ret!=HANTRO_OK)
    {
        loge("here1: decode Vp9DecodeFrameTag failed\n");
    }
    else if(dec->show_existing_frame)
    {
        pAnciBuffer->out_buffer_i = Vp9BufferQueueGetRef(dec_cont->bq,
            dec->show_existing_frame_index);
        logd("********here1: add ref index=%d\n",pAnciBuffer->out_buffer_i);
        Vp9BufferQueueAddRef(dec_cont->bq,pAnciBuffer->out_buffer_i);
        //Vp9SetupPicToOutput(dec_cont);
        pAnciBuffer->out_buffer_i = -1;
        Vp9PicToOutput(dec_cont);
        return DEC_PIC_DECODED;
    }
    /* Decode frame header (now starts bool coder as well) */
    ret = Vp9DecodeFrameHeader(pDataPtr + dec->frame_tag_size,
                             nDataLen - dec->frame_tag_size,
                             &dec_cont->bc, dec);

    if(ret != HANTRO_OK)
    {
        loge("here3: decode Vp9DecodeFrameHeader failed\n");
    }
    else if(dec->refresh_entropy_probs)
    {
        logi("here4:flag the stream as non error-resilient\n");
    }

    ret = Vp9SetPartitionOffsets(pDataPtr,nDataLen,dec);

    /* ignore errors in partition offsets if HW error concealment used
    * (assuming parts of stream missing -> partition start offsets may
    * be larger than amount of stream in the buffer) */

    if(ret != HANTRO_OK)
    {
        loge("here6: decode Vp9SetPartitionOffsets failed\n");
    }
    pAnciBuffer->width = NEXT_MULTIPLE(dec->width, 8);
    pAnciBuffer->height = NEXT_MULTIPLE(dec->height, 8);

    /* If the frame dimensions are not supported by HW,
     release allocated picture buffers and return error */
     //logd("here7: Vp9checksize, %d\n", __LINE__);

    if(dec_cont->width!=0 && dec_cont->height!=0)
    {
        #if 0
        if((dec_cont->width != dec->width) || (dec_cont->height != dec->height)/* &&
          (Vp9CheckSupport(dec_cont) != HANTRO_OK*/)
        {
            //Vp9AsicReleaseFilterBlockMem(dec_cont);
            //Vp9AsicReleasePictures(dec_cont);
            dec_cont->dec_stat = VP9DEC_INITIALIZED;
            logd("here8: return DEC_STREAM_NOT_SUPPORTED, \
                dec_cont->width=%d,dec->width=%d,dec_cont->height=%d, dec->height=%d\n",
                dec_cont->width,dec->width,dec_cont->height, dec->height);
            return DEC_STREAM_NOT_SUPPORTED;
        }
        #endif
    }
    dec_cont->width = dec->width;
    dec_cont->height = dec->height;
    if(dec_cont->dec_stat == VP9DEC_INITIALIZED)
    {
        dec_cont->dec_stat = VP9DEC_NEW_HEADERS;
        logi("here9: return DEC_HDRS_RDY\n");
        return DEC_HDRS_RDY;
    }

    /* If we are here and dimensions are still 0, it means that we have
    * yet to decode a valid keyframe, in which case we must give up. */
    if(dec_cont->width == 0 || dec_cont->height == 0)
    {
        loge("here10: return DEC_STRM_PROCESSED\n");
        return DEC_STRM_PROCESSED;
    }

    /* If output picture is broken and we are not decoding a base frame,
   * don't even start HW, just output same picture again. */
   if(!dec->key_frame && dec_cont->picture_broken &&
      (dec_cont->intra_freeze || dec_cont->force_intra_freeze))
    {
        //Vp9Freeze(dec_cont);
        loge("vp9 dec frame error\n");
        loge("here11: return DEC_PIC_DECODED\n");
        return DEC_PIC_DECODED;
    }
    return DEC_OK;
}

s32 Vp9MallocDecodeBuffer(Vp9HwDecode* pVp9HwDec)
{
    //u8 num_frame_buffers = 12;
    u8 num_frame_buffers = 10;
    //u8 num_frame_buffers = 4;

    u8 use_video_freeze_concealment = DEC_PICTURE_FREEZE;
    u8 output_format = DEC_OUT_FRM_RASTER_SCAN;

    struct Vp9DecContainer* dec_cont = NULL;
    const void *dwl;
    struct DWLInitParam dwl_init;

    /* init struct struct DWL for the specified client */

    dwl_init.client_type = DWL_CLIENT_TYPE_VP9_DEC;
    dwl = DWLInit(&dwl_init);
    if(dwl == NULL)
    {
        return DEC_DWL_ERROR;
    }
    dec_cont = malloc(sizeof(struct Vp9DecContainer));
    if(dec_cont == NULL)
    {
        loge("malloc memory for Vp9DecContainer failed\n");
        return -1;
    }
    memset(dec_cont, 0, sizeof(struct Vp9DecContainer));
    pVp9HwDec->dec_cont = (void*)dec_cont;

    dec_cont->dwl = dwl;
    dec_cont->memops = pVp9HwDec->memops;
    dec_cont->veOpsS = pVp9HwDec->veOpsS;
    dec_cont->pVeOpsSelf = pVp9HwDec->pVeOpsSelf;

    dec_cont->dec_stat = VP9DEC_INITIALIZED;
    dec_cont->checksum = dec_cont; /* save instance as a checksum */

    if(num_frame_buffers > VP9DEC_MAX_PIC_BUFFERS)
    {
        num_frame_buffers = VP9DEC_MAX_PIC_BUFFERS;
    }
    dec_cont->num_buffers = num_frame_buffers;
    if(pVp9HwDec->bThumbnailMode == 1)
    {
        dec_cont->num_buffers  = 1;
    }

    Vp9AsicInit(dec_cont); /* Init ASIC */
    if(Vp9AsicAllocateMem(dec_cont) != 0)
    {
        DWLfree(dec_cont);
        (void)DWLRelease(dwl);
        return DEC_MEMFAIL;
    }

    dec_cont->pic_number = dec_cont->display_number = 1;

    dec_cont->intra_freeze = use_video_freeze_concealment;
    dec_cont->picture_broken = 0;
    dec_cont->decoder.refbu_pred_hits = 0;

    #if 0
    if(FifoInit(VP9DEC_MAX_PIC_BUFFERS, &dec_cont->fifo_out) != FIFO_OK)
    {
        return DEC_MEMFAIL;
    }

    if(FifoInit(VP9DEC_MAX_PIC_BUFFERS, &dec_cont->fifo_display) != FIFO_OK)
    {
        return DEC_MEMFAIL;
    }
    #else
    if(FifoInit(dec_cont->num_buffers, &dec_cont->fifo_out) != FIFO_OK)
    {
        return DEC_MEMFAIL;
    }

    if(FifoInit(dec_cont->num_buffers, &dec_cont->fifo_display) != FIFO_OK)
    {
        return DEC_MEMFAIL;
    }
    #endif

    //DWLmemcpy(&dec_cont->hw_cfg, &hw_cfg, sizeof(DWLHwConfig));
    dec_cont->output_format = output_format;
  /* TODO this limit could later be given through the API. Dynamic reference
     frame allocation can allocate buffers dynamically up to this limit.
     Otherwise sw stops and waits for the free buffer to emerge to the fifo */
    dec_cont->dynamic_buffer_limit = VP9DEC_DYNAMIC_PIC_LIMIT;
    dec_cont->asic_buff->ctx_conter_index = 0;
    return 0;
}

s32 Vp9ReleaseDecodeBuffer(Vp9HwDecode* pVp9HwDec)
{
    if(pVp9HwDec->dec_cont != NULL)
    {
        Vp9DecRelease(pVp9HwDec->dec_cont);
        pVp9HwDec->dec_cont = NULL;
    }

    return 0;
}

void VP9CongigureDisplayParameters(Vp9HwDecodeContext* pVp9HwContext)
{
    VideoPicture*  curFrmInfo = NULL;
    Vp9HwDecode*   pVp9HwDec = NULL;
    struct Vp9DecContainer* dec_cont = NULL;
    struct Vp9Decoder* dec = NULL;

    pVp9HwDec = (Vp9HwDecode*)pVp9HwContext->pVp9HwDec;
    dec_cont = (struct Vp9DecContainer*)pVp9HwDec->dec_cont;
    dec = (struct Vp9Decoder*)&dec_cont->decoder;

    curFrmInfo = pVp9HwDec->pCurFrm;

    curFrmInfo->nTopOffset      = 0;
    curFrmInfo->nLeftOffset     = 0;
    curFrmInfo->nRightOffset    = dec->width;
    curFrmInfo->nBottomOffset   = dec->height;

    curFrmInfo->nFrameRate        = pVp9HwContext->videoStreamInfo.nFrameRate;  // need update
    curFrmInfo->nAspectRatio      = pVp9HwContext->videoStreamInfo.nAspectRatio;

    curFrmInfo->bIsProgressive    = 1;
    //curFrmInfo->ePixelFormat      = pVp9HwContext->vconfig.eOutputPixelFormat;
    curFrmInfo->nStreamIndex      = 0;
    if((pVp9HwDec->CurFramePts <=0) || (pVp9HwDec->CurFramePts <= pVp9HwDec->PreFramePts))
    {
        if(pVp9HwContext->videoStreamInfo.nFrameRate != 0)
        {
           int nFrameRate = 0;
           int duration = 0;
           nFrameRate = pVp9HwContext->videoStreamInfo.nFrameRate;
           duration = 1000/nFrameRate;
           duration *= 1000;
           pVp9HwDec->CurFramePts = pVp9HwDec->PreFramePts + duration;
        }
        else
        {
           pVp9HwDec->CurFramePts = pVp9HwDec->PreFramePts + 33333;
        }
    }
    curFrmInfo->nPts              = pVp9HwDec->CurFramePts;
    pVp9HwDec->PreFramePts        =  curFrmInfo->nPts;

    logv("curFrmInfo=%x, curFrmInfo->bIsProgressive =%d, \
          curFrmInfo->bTopFieldError=%d, \
          curFrmInfo->bBottomFieldError=%d\n",
          curFrmInfo, curFrmInfo->bIsProgressive, curFrmInfo->bTopFieldError,
          curFrmInfo->bBottomFieldError);
}

void Vp9CreateFbmBuffer(Vp9HwDecodeContext* pVp9HwContext)
{
  int width, height;
  s32 nProgressiveFlag = 1;
  FbmCreateInfo mFbmCreateInfo;
  Vp9HwDecode*     pVp9HwDec = NULL;
  struct Vp9DecContainer* dec_cont = NULL;
  struct Vp9Decoder* dec = NULL;

  pVp9HwDec = (Vp9HwDecode*)pVp9HwContext->pVp9HwDec;
  dec_cont = (struct Vp9DecContainer*)pVp9HwDec->dec_cont;
  dec = (struct Vp9Decoder*)&dec_cont->decoder;
  width = dec->width;
  height = dec->height;

  if(pVp9HwContext->pFbm == NULL)
  {
      int w = ((width + 31) & ~31) ;
      int h = ((height + 31) & ~31);

      memset(&mFbmCreateInfo, 0, sizeof(FbmCreateInfo));
      mFbmCreateInfo.nFrameNum          = VP9HW_INI_FBM_NUM;
      mFbmCreateInfo.nWidth             = w;
      mFbmCreateInfo.nHeight            = h;
      //pVp9HwContext->vconfig.eOutputPixelFormat;
      mFbmCreateInfo.ePixelFormat       = PIXEL_FORMAT_NV12;
      mFbmCreateInfo.bThumbnailMode     = pVp9HwContext->vconfig.bThumbnailMode;
      mFbmCreateInfo.bGpuBufValid       = pVp9HwContext->vconfig.bGpuBufValid;
      mFbmCreateInfo.nAlignStride       = pVp9HwContext->vconfig.nAlignStride;
      mFbmCreateInfo.nBufferType        = BUF_TYPE_ONLY_DISP;
      mFbmCreateInfo.bProgressiveFlag   = nProgressiveFlag;
      mFbmCreateInfo.bIsSoftDecoderFlag = pVp9HwContext->vconfig.bIsSoftDecoderFlag;
      mFbmCreateInfo.memops             = pVp9HwContext->vconfig.memops;
      mFbmCreateInfo.b10BitStreamFlag   = 0;
      mFbmCreateInfo.veOpsS             = pVp9HwContext->vconfig.veOpsS;
      mFbmCreateInfo.pVeOpsSelf         = pVp9HwContext->vconfig.pVeOpsSelf;

      #if 1
      if((dec->bit_depth ==10) && (pVp9HwContext->vconfig.bConvertVp910bitTo8bit==0))
      {
            mFbmCreateInfo.b10BitStreamFlag = 1;
          mFbmCreateInfo.ePixelFormat = PIXEL_FORMAT_P010_UV;
      }

      dec_cont->bNeedConvert10bitTo8bit = pVp9HwContext->vconfig.bConvertVp910bitTo8bit;
      #endif

      pVp9HwContext->pFbm = FbmCreate(&mFbmCreateInfo, pVp9HwContext->pFbmInfo);
      if(pVp9HwContext->pFbm == NULL)
      {
           loge("vp9 decoder fbm initial error!");
           return;
      }
      #if 0
      pVp9HwContext->pCurFrm = FbmRequestBuffer(pVp9HwContext->pFbm);
      //*on new-display, we maybe can't FbmRequestBuffer sucesse for the first time.
      int nRequestBufferCount = 0;
      while(pVp9HwContext->pCurFrm == NULL)
      {
          nRequestBufferCount++;
          if(nRequestBufferCount > 400)//* the MaxTime is 2 second!
          {
              loge("vp9 decoder request fbm timeOut! (Just after fbm initial)");
              return;
          }
          usleep(5*1000);
          pVp9HwContext->pCurFrm = FbmRequestBuffer(pVp9HwContext->pFbm);
      }
      #endif
   }
}

//s32 Vp9ProcessHeaders(void* decContainer, u8* pDataPtr, u32 nDataLen)
s32 Vp9ProcessHeaders(Vp9HwDecodeContext* pVp9HwContext, u8* pDataPtr, u32 nDataLen)
{
    Vp9HwDecode*     pVp9HwDec = NULL;
    struct Vp9DecContainer* dec_cont = NULL;
    s32 ret = 0;

    pVp9HwDec = (Vp9HwDecode*)pVp9HwContext->pVp9HwDec;
    dec_cont = (struct Vp9DecContainer*)pVp9HwDec->dec_cont;

    if(dec_cont->dec_stat == VP9DEC_INITIALIZED)
    {
        /* Decode SW part of the frame */
        ret = Vp9DecodeHeaders(pVp9HwContext,pDataPtr,nDataLen);
        if(ret < 0)
        {
            return ret;
        }
    }

    if(dec_cont->dec_stat == VP9DEC_NEW_HEADERS)
    {
        if(Vp9AsicAllocatePictures(dec_cont) != 0)
        {
            return DEC_MEMFAIL;
        }
        dec_cont->dec_stat = VP9DEC_DECODING;
    }
    else if(nDataLen > 0)
    {
        /* Decode SW part of the frame */
        ret = Vp9DecodeHeaders(pVp9HwContext,pDataPtr,nDataLen);
        if(ret)
        {
            return ret;
        }

    }
    else
    {
        loge("missing picture, conceal\n");
    }
    //dec_cont->pack_pixel_to_msbs = DEC_BITPACKING_MSB;
    return ret;
}

void Vp9DecRelease(Vp9DecInst dec_inst)
{
    struct Vp9DecContainer *dec_cont = (struct Vp9DecContainer *)dec_inst;
    const void *dwl;
    struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

    /* Check for valid decoder instance */
    if(dec_cont == NULL || dec_cont->checksum != dec_cont)
    {
        return;
    }

    dwl = dec_cont->dwl;
    if(dec_cont->asic_running)
    {
        DWLDisableHw(dwl, dec_cont->core_id, 1 * 4, 0); /* stop HW */
        DWLReleaseHw(dwl, dec_cont->core_id);           /* release HW lock */
        dec_cont->asic_running = 0;
    }
    Vp9AsicReleaseMem(dec_cont);
    Vp9AsicReleaseFilterBlockMem(dec_cont);
    Vp9AsicReleasePictures(dec_cont);

    if(dec_cont->fifo_out)
    {
        FifoRelease(dec_cont->fifo_out);
    }
    if(dec_cont->fifo_display)
    {
        FifoRelease(dec_cont->fifo_display);
    }
    //pthread_cond_destroy(&dec_cont->sync_out_cv);
    //pthread_mutex_destroy(&dec_cont->sync_out);
    dec_cont->checksum = NULL;
    DWLfree(dec_cont);
    s32 dwlret = DWLRelease(dwl);
    //ASSERT(dwlret == DWL_OK);
    (void)dwlret;
    logd("Vp9DecRelease end line=%d\n", __LINE__);
    return;
}

#if 0
u32 Vp9ProcessDataLength(u8* data,int size, u8* n_frames)
{
    u32 marker = 0;
    u32 nbytes = 0;
    //u32 n_frames = 0;
    u32 idx_sz = 0;
    u32 rd_sz = 0;

    *n_frames = 1;
    rd_sz = size;
    marker = data[size - 1];

    if((marker & 0xe0) == 0xc0)
    {
        nbytes = 1 + ((marker >> 3) & 0x3);
        *n_frames = 1 + (marker & 0x7);

        logd("*************n_frames=%d\n", *n_frames);
        if(*n_frames != 2)
        {
            abort();
        }
        idx_sz = 2 + *n_frames * nbytes;
        if(size >= idx_sz && data[size-idx_sz]==marker)
        {
            u8 *idx = data + size + 1 - idx_sz;

            logd("idx[0]=%x, idx[1]=%x, idx[2]=%x, idx[3]=%x, nbytes=%d\n",
                  idx[0], idx[1], idx[2], idx[3], nbytes);
            switch(nbytes)
            {
                case 1:
                     rd_sz = *idx;
                     break;
                case 2:
                     rd_sz = (idx[1]<<8)|idx[0];
                     break;
                case 3:
                     rd_sz = (idx[2]<<16)|(idx[1]<<8)|idx[0];
                     break;
                case 4:
                     rd_sz = (idx[3]<<24)|(idx[2]<<16)|(idx[1]<<8)|(idx[0]);
                     break;

            }
            #if 0
            while(n_frames--)
            {
                idx += nbytes;
                if(rd_sz > size)
                {
                    loge("the frame size id error\n");
                    abort();
                }

                data += rd_sz;
                size -= rd_sz;
            }
            #endif
        }
    }
    return rd_sz;
}

#endif

#if 0
int64_t VdecoderGetCurrentTime(void)
{
    struct timeval tv;
    int64_t time;
    gettimeofday(&tv,NULL);
    time = tv.tv_sec*1000000 + tv.tv_usec;
    return time;
}
#endif

u32 Vp9ProcessDataLength(u8* data,u32 size, u8* n_frames, u32 realDataLen[])
{
    u32 marker = 0;
    u32 nbytes = 0;
    u32 idx_sz = 0;
    u32 rd_sz = 0;
    u8  nFrameNum = 0;
    *n_frames = 1;
    rd_sz = size;
    marker = data[size - 1];
    realDataLen[0] = rd_sz;

    if((marker & 0xe0) == 0xc0)
    {
        nbytes = 1 + ((marker >> 3) & 0x3);
        *n_frames = 1 + (marker & 0x7);
        nFrameNum = *n_frames;
        if(*n_frames != 2)
        {
            logd("here4\n");
            abort();
        }
        idx_sz = 2 + *n_frames * nbytes;
        if(size >= idx_sz && data[size-idx_sz]==marker)
        {
            u8 *idx = data + size + 1 - idx_sz;
            while(nFrameNum--)
            {
                switch(nbytes)
                {
                    case 1:
                         rd_sz = *idx;
                         break;
                    case 2:
                         rd_sz = (idx[1]<<8)|idx[0];
                         break;
                    case 3:
                         rd_sz = (idx[2]<<16)|(idx[1]<<8)|idx[0];
                         break;
                    case 4:
                         rd_sz = (idx[3]<<24)|(idx[2]<<16)|(idx[1]<<8)|(idx[0]);
                         break;
                }
                idx += nbytes;
                realDataLen[nFrameNum] = (rd_sz <= size)? rd_sz:size;
                size -= rd_sz;
            }
        }
    }
    return 0;
}

#if 0
void Vp9SaveDecodeFrame(void* decContainer)

{
    struct Vp9DecContainer* dec_cont = NULL;
    struct DecAsicBuffers* pAnciBuffer = NULL;
    s32 ret = 0;
    u8* srcPtr = NULL;
    u16* srcPtr1 = NULL;
    u32 i = 0;
    u32 j = 0;
    FILE* fp = NULL;

    SHA1_CTX shaCtx;
    unsigned char sha[20]={0};
    char name[128];
    unsigned int nWidthStride = 0;
    unsigned char buffer[4096]={0};
    FILE* fpy = NULL;
    FILE* fpc = NULL;
    int nYbitDepth = 0;
    int nCbitDepth = 0;
    int nYDataIndex = 0;
    int nCDataIndex = 0;
    u32 number = 2000;

    memset(&shaCtx, 0, sizeof(SHA1_CTX));

    dec_cont = (struct Vp9DecContainer*)decContainer;
    pAnciBuffer = (struct DecAsicBuffers*)dec_cont->asic_buff;
    nWidthStride = (dec_cont->width+31)&~31;

    if((dec_cont->pic_number % number) == 0)
    {
        nYbitDepth = (dec_cont->vp9_regs[8]>>21)&0x0f;
        nCbitDepth = (dec_cont->vp9_regs[8]>>17)&0x0f;
        //sprintf(name, "/mnt/H6/install/vp9_%dx%d_%d.dat",
        //    pAnciBuffer->width,pAnciBuffer->height,dec_cont->pic_number);

        sprintf(name, "/data/camera/vp9_%dx%d_%d.dat",
            pAnciBuffer->width,pAnciBuffer->height,dec_cont->pic_number);

           fp = fopen(name, "wb");
        if(fp != NULL)
        {
            if(nYbitDepth == 8)
            {
                nYDataIndex = 1;
            }
            else if(nYbitDepth > 8)
            {
                nYDataIndex = 2;
            }
            CdcMemFlushCache(dec_cont->memops,
                pAnciBuffer->raster_luma[pAnciBuffer->out_buffer_i].virtual_address,
                pAnciBuffer->raster_luma[pAnciBuffer->out_buffer_i].size);

            fwrite(pAnciBuffer->raster_luma[pAnciBuffer->out_buffer_i].virtual_address, 1,
                pAnciBuffer->width*pAnciBuffer->height, fp);

            if(nCbitDepth == 8)
            {
                nCDataIndex = 1;
            }
            else if(nCbitDepth > 8)
            {
                nCDataIndex = 2;
            }
             CdcMemFlushCache(dec_cont->memops,
                pAnciBuffer->raster_chroma[pAnciBuffer->out_buffer_i].virtual_address,
                pAnciBuffer->raster_chroma[pAnciBuffer->out_buffer_i].size);

            fwrite(pAnciBuffer->raster_chroma[pAnciBuffer->out_buffer_i].virtual_address, 1,
                (pAnciBuffer->width*pAnciBuffer->height)/2, fp);
            fclose(fp);
        }

        #if 0
        nYbitDepth = (dec_cont->vp9_regs[8]>>21)&0x0f;
        nCbitDepth = (dec_cont->vp9_regs[8]>>17)&0x0f;

        //sprintf(name, "/mnt/install/vp9_%d_y.dat", dec_cont->pic_number);
        sprintf(name, "/data/camera/vp9_%dx%d_%d_y.dat",
            pAnciBuffer->width,pAnciBuffer->height,dec_cont->pic_number);
           fp = fopen(name, "wb");
        if(fp != NULL)
        {
            if(nYbitDepth == 8)
            {
                nYDataIndex = 1;
            }
            else if(nYbitDepth > 8)
            {
                nYDataIndex = 2;
            }
            CdcMemFlushCache(dec_cont->memops,
                pAnciBuffer->raster_luma[pAnciBuffer->out_buffer_i].virtual_address,
                pAnciBuffer->raster_luma[pAnciBuffer->out_buffer_i].size);

            #if 0
            fwrite(pAnciBuffer->raster_luma[pAnciBuffer->out_buffer_i].virtual_address, 1,
                pAnciBuffer->raster_luma[pAnciBuffer->out_buffer_i].size, fp);
            #else
            srcPtr = (u8*)pAnciBuffer->raster_luma[pAnciBuffer->out_buffer_i].virtual_address;
            for(i=0; i<dec_cont->height; i++)
            {
                fwrite(srcPtr, 1,dec_cont->width*nYDataIndex, fp);
                srcPtr += nWidthStride*nYDataIndex;
            }
            #endif
            fclose(fp);
        }

    #if 1
        sprintf(name, "/data/camera/vp9_%dx%d_%d_c.dat",
                pAnciBuffer->width,pAnciBuffer->height,dec_cont->pic_number);
        fp = fopen(name, "wb");
        if(fp != NULL)
        {
            if(nCbitDepth == 8)
            {
                nCDataIndex = 1;
            }
            else if(nCbitDepth > 8)
            {
                nCDataIndex = 2;
            }
             CdcMemFlushCache(dec_cont->memops,
                pAnciBuffer->raster_chroma[pAnciBuffer->out_buffer_i].virtual_address,
                pAnciBuffer->raster_chroma[pAnciBuffer->out_buffer_i].size);
            #if 0
            fwrite(pAnciBuffer->raster_chroma[pAnciBuffer->out_buffer_i].virtual_address, 1,
                pAnciBuffer->raster_chroma[pAnciBuffer->out_buffer_i].size, fp);
            #else
            srcPtr = (u8*)pAnciBuffer->raster_chroma[pAnciBuffer->out_buffer_i].virtual_address;
            for(i=0; i<(dec_cont->height+1)/2; i++)
            {
                fwrite(srcPtr,1,dec_cont->width*nCDataIndex, fp);
                srcPtr += nWidthStride*nCDataIndex;
            }
            fclose(fp);
        }
    #endif
    #endif
        #endif
    }

    //* compute the md5
    #if 0

    #define FILENAME "/data/camera/vp9sha.txt"
    if(dec_cont->pic_number == 1)
    {
        remove(FILENAME);
    }

    logd("dec_cont->vp9_regs=%x\n", dec_cont->vp9_regs[8]);

    nYbitDepth = (dec_cont->vp9_regs[8]>>21)&0x0f;
    nCbitDepth = (dec_cont->vp9_regs[8]>>17)&0x0f;

    logd("*******nYbitDepth=%d, nCbitDepth=%d\n", nYbitDepth, nCbitDepth);

    fp = fopen(FILENAME, "ab+");
    logd("**********fp = %p",fp);

    CdcMemFlushCache(dec_cont->memops,
            pAnciBuffer->raster_luma[pAnciBuffer->out_buffer_i].virtual_address,
            pAnciBuffer->raster_luma[pAnciBuffer->out_buffer_i].size);
    CdcMemFlushCache(dec_cont->memops,
            pAnciBuffer->raster_chroma[pAnciBuffer->out_buffer_i].virtual_address,
            pAnciBuffer->raster_chroma[pAnciBuffer->out_buffer_i].size);

    SHA1Init(&shaCtx);
    // copy y

    logd("pAnciBuffer->height=%d, pAnciBuffer->width=%d, nWidthStride=%d\n",
        pAnciBuffer->height,
        pAnciBuffer->width,
        nWidthStride);

    if(nYbitDepth==8 && nCbitDepth==8)
    {
        srcPtr = (u8*)pAnciBuffer->raster_luma[pAnciBuffer->out_buffer_i].virtual_address;

        for(i=0; i<dec_cont->height; i++)
        {
                CdcMemCopy(dec_cont->memops, buffer, srcPtr, pAnciBuffer->width);
                SHA1Update(&shaCtx, buffer, dec_cont->width);
            srcPtr += nWidthStride;
        }

        srcPtr =(u8*)pAnciBuffer->raster_chroma[pAnciBuffer->out_buffer_i].virtual_address;
        for(i=0; i<(dec_cont->height+0)/2 ; i++)
        {
            CdcMemCopy(dec_cont->memops, buffer, srcPtr, pAnciBuffer->width);
                SHA1Update(&shaCtx, buffer,dec_cont->width);
            srcPtr += nWidthStride;
        }
           SHA1Final(sha, &shaCtx);
    }
    else
    {
        srcPtr1 = (u16*)pAnciBuffer->raster_luma[pAnciBuffer->out_buffer_i].virtual_address;
        for(j=0; j<dec_cont->height; j++)
        {
            for(i=0; i< dec_cont->width; i++)
            {
                buffer[i] = *(srcPtr1+i)>>(nYbitDepth-8);
            }
            SHA1Update(&shaCtx, buffer,pAnciBuffer->width);
            srcPtr1 += nWidthStride;
        }

        srcPtr1 = (u16*)pAnciBuffer->raster_chroma[pAnciBuffer->out_buffer_i].virtual_address;
        for(j=0; j<(dec_cont->height+0)/2; j++)
        {
            for(i=0; i< dec_cont->width; i++)
            {
                buffer[i] = *(srcPtr1+i)>>(nCbitDepth-8);
            }
            SHA1Update(&shaCtx, buffer,pAnciBuffer->width);
            srcPtr1 += nWidthStride;
        }
        SHA1Final(sha, &shaCtx);
    }

    if(fp != NULL)
    {
        for(i = 0; i < 20; i++)
        {
            fprintf(fp, "%02x", sha[i]);
        }
        #if 0
        for(i=0; i<2; i++)
        {
            logd("Sha:%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                sha[i*10+0],sha[i*10+1],sha[i*10+2],sha[i*10+3],sha[i*10+4],
                sha[i*10+5],sha[i*10+6],sha[i*10+7],sha[i*10+8],sha[i*10+9]);
        }
        #endif

        fprintf(fp, "\n");
        fclose(fp);
    }
    #endif

}
#endif

#if 0
void Vp9SaveDecodeFrame(void* decContainer)
{
    struct Vp9DecContainer* dec_cont = NULL;
    struct DecAsicBuffers* pAnciBuffer = NULL;
    s32 ret = 0;
    u8* srcPtr = NULL;
    u16* srcPtr1 = NULL;
    u32 i = 0;
    u32 j = 0;
    FILE* fp = NULL;

    SHA1_CTX shaCtx;
    unsigned char sha[20]={0};
    char name[128];
    unsigned int nWidthStride = 0;
    unsigned char buffer[4096]={0};
    FILE* fpy = NULL;
    FILE* fpc = NULL;
    int nYbitDepth = 0;
    int nCbitDepth = 0;
    int nYDataIndex = 0;
    int nCDataIndex = 0;

    memset(&shaCtx, 0, sizeof(SHA1_CTX));
    dec_cont = (struct Vp9DecContainer*)decContainer;
    pAnciBuffer = (struct DecAsicBuffers*)dec_cont->asic_buff;
    nWidthStride = (dec_cont->width+31)&~31;

   if(dec_cont->pic_number >= 0 && (dec_cont->pic_number <=10))
    {
        nYbitDepth = (dec_cont->vp9_regs[8]>>21)&0x0f;
        nCbitDepth = (dec_cont->vp9_regs[8]>>17)&0x0f;
        //sprintf(name, "/data/camera/vp9_%dx%d_%d.dat",
          //  pAnciBuffer->width,pAnciBuffer->height,dec_cont->pic_number);
          sprintf(name, "/data/camera/vp9_%dx%d.dat",
          pAnciBuffer->width,pAnciBuffer->height);

           //fp = fopen(name, "wb");
           fp = fopen(name, "ab");
        if(fp != NULL)
        {
            if(nYbitDepth == 8)
            {
                nYDataIndex = 1;
            }
            else if(nYbitDepth > 8)
            {
                nYDataIndex = 2;
            }
            CdcMemFlushCache(dec_cont->memops,
                pAnciBuffer->raster_luma[pAnciBuffer->out_buffer_i].virtual_address,
                pAnciBuffer->raster_luma[pAnciBuffer->out_buffer_i].size);

            fwrite(pAnciBuffer->raster_luma[pAnciBuffer->out_buffer_i].virtual_address, 1,
                pAnciBuffer->raster_luma[pAnciBuffer->out_buffer_i].size, fp);

            logd("the luma size is:%x\n",pAnciBuffer->raster_luma[pAnciBuffer->out_buffer_i].size);
            if(nCbitDepth == 8)
            {
                nCDataIndex = 1;
            }
            else if(nCbitDepth > 8)
            {
                nCDataIndex = 2;
            }
            CdcMemFlushCache(dec_cont->memops,
                pAnciBuffer->raster_chroma[pAnciBuffer->out_buffer_i].virtual_address,
                pAnciBuffer->raster_chroma[pAnciBuffer->out_buffer_i].size);

           fwrite(pAnciBuffer->raster_chroma[pAnciBuffer->out_buffer_i].virtual_address, 1,
                pAnciBuffer->raster_chroma[pAnciBuffer->out_buffer_i].size, fp);
           logd("the chroma size is:%x\n",
                 pAnciBuffer->raster_chroma[pAnciBuffer->out_buffer_i].size);
            fclose(fp);
        }
     }
 }
#endif

#if 0
void Vp9PrintData(void* decContainer)
{
    struct Vp9DecContainer* dec_cont = NULL;
    struct DecAsicBuffers* pAnciBuffer = NULL;
    u8* srcPtr = NULL;
    u8 i = 0;

    dec_cont = (struct Vp9DecContainer*)decContainer;
    pAnciBuffer = (struct DecAsicBuffers*)dec_cont->asic_buff;
    srcPtr = (u8*)pAnciBuffer->raster_luma[pAnciBuffer->out_buffer_i].virtual_address;
    CdcMemFlushCache(dec_cont->memops,srcPtr,
        pAnciBuffer->raster_luma[pAnciBuffer->out_buffer_i].size);
    for(i=0; i<10; i++)
    {
        logd("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
            srcPtr[i*16+0],srcPtr[i*16+1],srcPtr[i*16+2],srcPtr[i*16+3],
            srcPtr[i*16+4],srcPtr[i*16+5],srcPtr[i*16+6],srcPtr[i*16+7],
            srcPtr[i*16+8],srcPtr[i*16+8],srcPtr[i*16+10],srcPtr[i*16+11],
            srcPtr[i*16+12],srcPtr[i*16+13],srcPtr[i*16+14],srcPtr[i*16+15]);
    }
}
#endif

