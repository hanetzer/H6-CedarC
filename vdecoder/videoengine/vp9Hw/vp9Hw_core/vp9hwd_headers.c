/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Hantro Products Oy.                             --
--                                                                            --
--                   (C) COPYRIGHT 2011 HANTRO PRODUCTS OY                    --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
-
-  Description : ...
-
------------------------------------------------------------------------------*/

#include "vp9hwd_stream.h"
#include "vp9hwd_bool.h"
#include "vp9hwd_container.h"
#include "vp9Hw_dec.h"

extern void Vp9StoreAdaptProbs(struct Vp9Decoder *dec);
extern vp9_prob Vp9ReadProbDiffUpdate(struct VpBoolCoder *bc, int oldp);
extern u32 Vp9DecodeCoeffUpdate(struct VpBoolCoder *bc,
    u8 prob_coeffs[BLOCK_TYPES][REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS]
                  [ENTROPY_NODES_PART1]);
extern u32 Vp9DecodeMvUpdate(struct VpBoolCoder *bc, struct Vp9Decoder *dec);

#define VP9_KEY_FRAME_START_CODE_0 0x49
#define VP9_KEY_FRAME_START_CODE_1 0x83
#define VP9_KEY_FRAME_START_CODE_2 0x42

#define MIN_TILE_WIDTH 256
#define MAX_TILE_WIDTH 4096
#define MIN_TILE_WIDTH_SBS (MIN_TILE_WIDTH >> 6)
#define MAX_TILE_WIDTH_SBS (MAX_TILE_WIDTH >> 6)
#define VP9BORDERINPIXELS      160

void ExistingRefRegister(struct Vp9Decoder *dec, u32 map)
{
    dec->existing_ref_map |= map;
}

u32 CheckSyncCode(struct StrmData *rb)
{
    if(SwGetBits(rb, 8) != VP9_KEY_FRAME_START_CODE_0 ||
        SwGetBits(rb, 8) != VP9_KEY_FRAME_START_CODE_1 ||
        SwGetBits(rb, 8) != VP9_KEY_FRAME_START_CODE_2)
    {
        loge("VP9 Key-frame start code missing or invalid!\n");
        return HANTRO_NOK;
    }
    return HANTRO_OK;
}

//add for creating fbm
//static void apply_frame_size(struct Vp9DecContainer* dec_cont, int width, int height)

//void SetupFrameSize(struct StrmData *rb, struct Vp9Decoder *dec)
void SetupFrameSize(struct StrmData *rb, Vp9HwDecodeContext* pVp9HwContext)
{
    Vp9HwDecode*     pVp9HwDec = NULL;
    struct Vp9DecContainer* dec_cont = NULL;
    struct Vp9Decoder* dec = NULL;

    pVp9HwDec = (Vp9HwDecode*)pVp9HwContext->pVp9HwDec;
    dec_cont = (struct Vp9DecContainer*)pVp9HwDec->dec_cont;
    dec = &(dec_cont->decoder);

    /* Frame width */
    dec->width = SwGetBits(rb, 16) + 1;
    /* Frame height */
    dec->height = SwGetBits(rb, 16) + 1;
    dec->scaling_active = SwGetBits(rb, 1);
    if(dec->scaling_active)
    {
        /* Scaled frame width */
        dec->scaled_width = SwGetBits(rb, 16) + 1;
        logv("scaled_frame_width=%d\n", dec->scaled_width);
        /* Scaled frame height */
        dec->scaled_height = SwGetBits(rb, 16) + 1;
        logv("scaled_frame_height=%d\n", dec->scaled_height);
    }
    else
    {
        dec->scaled_width = dec->width;
        dec->scaled_height = dec->height;
    }
}

//int SetupFrameSizeWithRefs(struct StrmData *rb,
                           // struct Vp9DecContainer *dec_cont)
int SetupFrameSizeWithRefs(struct StrmData *rb,
                            Vp9HwDecodeContext* pVp9HwContext)
{
    Vp9HwDecode*     pVp9HwDec = NULL;
    struct Vp9DecContainer* dec_cont = NULL;
    struct Vp9Decoder* dec = NULL;

    pVp9HwDec = (Vp9HwDecode*)pVp9HwContext->pVp9HwDec;
    dec_cont = (struct Vp9DecContainer*)pVp9HwDec->dec_cont;
    dec = &(dec_cont->decoder);

    struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
    u32 tmp, index;
    s32 found, i;
    u32 prev_width, prev_height;

    found = 0;
    dec->resolution_change = 0;
    prev_width = dec->width;
    prev_height = dec->height;

    for(i = 0; i < ALLOWED_REFS_PER_FRAME; ++i)
    {
        tmp = SwGetBits(rb, 1);
        STREAM_TRACE("use_prev_frame_size", tmp);
        if(tmp)
        {
            found = 1;
            /* Get resolution from frame buffer [active_ref_idx[i]] */
            index = Vp9BufferQueueGetRef(dec_cont->bq, dec->active_ref_idx[i]);
            #if 0
            if(index >= VP9DEC_MAX_PIC_BUFFERS)
            {
                return HANTRO_NOK;
            }
            #else
            if(index >= dec_cont->num_buffers)
            {
                loge("*****get ref frame buffer failed, index=%d\n", index);
                return HANTRO_NOK;
            }
            #endif
            dec->width = asic_buff->picture_info[index].coded_width;
            dec->height = asic_buff->picture_info[index].coded_height;
            STREAM_TRACE("frame_width", dec->width);
            STREAM_TRACE("frame_height", dec->height);
            logi("here3:dec->width=%d, dec->height=%d, index=%d\n", dec->width, dec->height, index);
            break;
        }
        #if 1
        dec->resolution_change = 1; /* Signal resolution change for this frame */
        logd("*********here1: dec->resolution_change = 1\n");
        abort();
        #endif
    }

    if(!found)
    {
        /* Frame width */
        dec->width = SwGetBits(rb, 16) + 1;
        STREAM_TRACE("frame_width", dec->width);

        /* Frame height */
        dec->height = SwGetBits(rb, 16) + 1;
        STREAM_TRACE("frame_height", dec->height);
        logd("here4:dec->width=%d, dec->height=%d\n", dec->width, dec->height);
    }
    if(dec->width != prev_width || dec->height != prev_height)
    {
        dec->resolution_change = 1; /* Signal resolution change for this frame */
       logd("*********here2: dec->resolution_change = 1\n");
       abort();
    }

    //add:created fbm
    //Vp9CreateFbmBuffer(pVp9HwContext, dec->width, dec->height);

#if 0
    DWLHwConfig hw_config;
    DWLReadAsicConfig(&hw_config);

    if((dec->width > hw_config.max_dec_pic_width) ||
      (dec->height > hw_config.max_dec_pic_height) ||
      (dec->width < MIN_PIC_WIDTH) ||
      (dec->height < MIN_PIC_HEIGHT))
    {
        logd("*******here4:error:dec->width=%d, hw_config.max_dec_pic_width=%d\n", \
            dec->width, hw_config.max_dec_pic_width);
        logd("*******here4:error:dec->height=%d, hw_config.max_dec_pic_height=%d\n", \
            dec->height, hw_config.max_dec_pic_height);

        return HANTRO_NOK;
    }
#endif

    dec->scaling_active = SwGetBits(rb, 1);
    STREAM_TRACE("scaling active", dec->scaling_active);

    if(dec->scaling_active)
    {
        /* Scaled frame width */
        dec->scaled_width = SwGetBits(rb, 16) + 1;
        STREAM_TRACE("scaled_frame_width", dec->scaled_width);
        /* Scaled frame height */
        dec->scaled_height = SwGetBits(rb, 16) + 1;
        STREAM_TRACE("scaled_frame_height", dec->scaled_height);
        logd("here3\n");
        abort();
    }
    return HANTRO_OK;
}

void DecodeLfParams(struct StrmData *rb, struct Vp9Decoder *dec)
{
    u32 sign, tmp, j;

    if(dec->key_frame || dec->error_resilient || dec->intra_only)
    {
        memset(dec->mb_ref_lf_delta, 0, sizeof(dec->mb_ref_lf_delta));
        memset(dec->mb_mode_lf_delta, 0, sizeof(dec->mb_mode_lf_delta));
        dec->mb_ref_lf_delta[0] = 1;
        dec->mb_ref_lf_delta[1] = 0;
        dec->mb_ref_lf_delta[2] = -1;
        dec->mb_ref_lf_delta[3] = -1;
    }

    /* Loop filter adjustments */
    dec->loop_filter_level = SwGetBits(rb, 6);
    dec->loop_filter_sharpness = SwGetBits(rb, 3);
    /* Adjustments enabled? */
    dec->mode_ref_lf_enabled = SwGetBits(rb, 1);

    if(dec->mode_ref_lf_enabled)
    {
        /* Mode update? */
        tmp = SwGetBits(rb, 1);
        logv("loop_filter_adj_update=%d\n", tmp);
        if(tmp)
        {
            /* Reference frame deltas */
            for(j = 0; j < MAX_REF_LF_DELTAS; j++)
            {
                tmp = SwGetBits(rb, 1);
                logv("ref_frame_delta_update=%d\n", tmp);
                if(tmp)
                {
                    /* Payload */
                    tmp = SwGetBits(rb, 6);
                    /* Sign */
                    sign = SwGetBits(rb, 1);
                    logv("loop_filter_payload=%d\n", tmp);
                    logv("loop_filter_sign=%d\n", sign);
                    dec->mb_ref_lf_delta[j] = tmp;
                    if(sign)
                        dec->mb_ref_lf_delta[j] = -dec->mb_ref_lf_delta[j];
                }
            }

            /* Mode deltas */
            for(j = 0; j < MAX_MODE_LF_DELTAS; j++)
            {
                tmp = SwGetBits(rb, 1);
                logv("mb_type_delta_update=%d\n", tmp);
                if(tmp)
                {
                    /* Payload */
                    tmp = SwGetBits(rb, 6);
                    /* Sign */
                    sign = SwGetBits(rb, 1);
                    logv("loop_filter_payload=%d\n", tmp);
                    logv("loop_filter_sign=%d\n", sign);
                    dec->mb_mode_lf_delta[j] = tmp;
                    if(sign)
                        dec->mb_mode_lf_delta[j] = -dec->mb_mode_lf_delta[j];
                }
            }
        }
    } /* Mb mode/ref lf adjustment */
}

s32 DecodeQuantizerDelta(struct StrmData *rb)
{
    u32 sign;
    s32 delta;

    if(SwGetBits(rb, 1))
    {
        logv("qp_delta_present=%d\n", 1);
        delta = SwGetBits(rb, 4);
        logv("qp_delta=%d\n", delta);
        sign = SwGetBits(rb, 1);
        logv("qp_delta_sign=%d\n", sign);
        if(sign)
            delta = -delta;
        return delta;
    }
    else
    {
        logv("qp_delta_present=%d\n", 0);
        return 0;
    }
}

void DecodeSegmentationData(struct StrmData *rb, struct Vp9Decoder *dec)
{
    u32 tmp, sign, i, j;
    struct Vp9EntropyProbs *fc = &dec->entropy; /* Frame context */

    /* Segmentation enabled? */
    dec->segment_enabled = SwGetBits(rb, 1);
    logv("segment_enabled=%d\n", dec->segment_enabled);

    dec->segment_map_update = 0;
    dec->segment_map_temporal_update = 0;
    if(!dec->segment_enabled)
        return;

    /* Segmentation map update */
    dec->segment_map_update = SwGetBits(rb, 1);
    logv("segment_map_update=%d\n", dec->segment_map_update);

    if(dec->segment_map_update)
    {
        for(i = 0; i < MB_SEG_TREE_PROBS; i++)
        {
            tmp = SwGetBits(rb, 1);
            logv("segment_tree_prob_update=%d\n", tmp);
            if(tmp)
            {
                fc->mb_segment_tree_probs[i] = SwGetBits(rb, 8);
                logv("segment_tree_prob=%d\n", tmp);
            }
            else
            {
                fc->mb_segment_tree_probs[i] = 255;
            }
        }

        /* Read the prediction probs needed to decode the segment id */
        dec->segment_map_temporal_update = SwGetBits(rb, 1);
        logv("segment_map_temporal_update=%d\n",
                 dec->segment_map_temporal_update);
        for(i = 0; i < PREDICTION_PROBS; i++)
        {
            if(dec->segment_map_temporal_update)
            {
                tmp = SwGetBits(rb, 1);
                logv("segment_pred_prob_update", tmp);
                if(tmp)
                {
                    fc->segment_pred_probs[i] = SwGetBits(rb, 8);
                    logv("segment_pred_prob", tmp);
                }
                else
                {
                    fc->segment_pred_probs[i] = 255;
                }
            }
            else
            {
                fc->segment_pred_probs[i] = 255;
            }
        }
    }

    /* Segment feature data update */
    tmp = SwGetBits(rb, 1);
    logv("segment_data_update=%d\n", tmp);
    if(tmp)
    {
        /* Absolute/relative mode */
        dec->segment_feature_mode = SwGetBits(rb, 1);
        logv("segment_abs_delta=%d\n", dec->segment_feature_mode);

        /* Clear all previous segment data */
        memset(dec->segment_feature_enable, 0,
              sizeof(dec->segment_feature_enable));
        memset(dec->segment_feature_data, 0, sizeof(dec->segment_feature_data));

        for(i = 0; i < MAX_MB_SEGMENTS; i++)
        {
            for(j = 0; j < SEG_LVL_MAX; j++)
            {
                dec->segment_feature_enable[i][j] = SwGetBits(rb, 1);
                logv("segment_feature_enable",
                     dec->segment_feature_enable[i][j]);
                if(dec->segment_feature_enable[i][j])
                {
                    /* Feature data, bits changes for every feature */
                    dec->segment_feature_data[i][j] =
                        SwGetBitsUnsignedMax(rb, vp9_seg_feature_data_max[j]);
                    logv("segment_feature_data=%d\n", dec->segment_feature_data[i][j]);
                    /* Sign if needed */
                    if(vp9_seg_feature_data_signed[j])
                    {
                        sign = SwGetBits(rb, 1);
                        logv("segment_feature_sign=%d\n", sign);
                        if(sign)
                            dec->segment_feature_data[i][j] =
                                -dec->segment_feature_data[i][j];
                    }
                }
            }
        }
    }
}

void GetTileNBits(struct Vp9Decoder *dec, u32 *min_log2_ntiles_ptr,
                  u32 *delta_log2_ntiles)
{
    const int sb_cols = (dec->width + 63) >> 6;
    u32 min_log2_ntiles, max_log2_ntiles;

    for(max_log2_ntiles = 0; (sb_cols >> max_log2_ntiles) >= MIN_TILE_WIDTH_SBS;
        max_log2_ntiles++)
    {

    }
    if(max_log2_ntiles > 0)
        max_log2_ntiles--;
    for(min_log2_ntiles = 0; (MAX_TILE_WIDTH_SBS << min_log2_ntiles) < sb_cols;
       min_log2_ntiles++)
    {

    }
    //ASSERT(max_log2_ntiles >= min_log2_ntiles);
    *min_log2_ntiles_ptr = min_log2_ntiles;
    *delta_log2_ntiles = max_log2_ntiles - min_log2_ntiles;
}

u32 ReadTileSize(const u8 *cx_size)
{
    u32 size;
    size = (u32)(*(cx_size + 3)) + ((u32)(*(cx_size + 2)) << 8) +
         ((u32)(*(cx_size + 1)) << 16) + ((u32)(*(cx_size + 0)) << 24);
    return size;
}

u32 Vp9DecodeFrameHeader(const u8 *stream, u32 strm_len, struct VpBoolCoder *bc,
                         struct Vp9Decoder *dec)
{
    u32 tmp, i, j, k;
    struct Vp9EntropyProbs *fc = &dec->entropy; /* Frame context */

    if(dec->width == 0 || dec->height == 0)
    {
        loge("Invalid size!\n");
        return HANTRO_NOK;
    }

    /* Store probs for context backward adaptation. */
    Vp9StoreAdaptProbs(dec);

    /* Start bool coder */
    Vp9BoolStart(bc, stream, strm_len);

    /* Setup transform mode and probs */
    if(dec->lossless)
    {
        dec->transform_mode = ONLY_4X4;
    }
    else
    {
        dec->transform_mode = Vp9ReadBits(bc, 2);
        STREAM_TRACE("transform_mode", dec->transform_mode);
        if(dec->transform_mode == ALLOW_32X32)
        {
            dec->transform_mode += Vp9ReadBits(bc, 1);
            STREAM_TRACE("transform_mode", dec->transform_mode);
        }
        if(dec->transform_mode == TX_MODE_SELECT)
        {
            for(i = 0; i < TX_SIZE_CONTEXTS; ++i)
            {
                for(j = 0; j < TX_SIZE_MAX_SB - 3; ++j)
                {
                    tmp = Vp9DecodeBool(bc, VP9_DEF_UPDATE_PROB);
                    STREAM_TRACE("tx8x8_prob_update", tmp);
                    if(tmp)
                    {
                        u8 *prob = &fc->a.tx8x8_prob[i][j];
                        *prob = Vp9ReadProbDiffUpdate(bc, *prob);
                        STREAM_TRACE("tx8x8_prob", *prob);
                    }
                }
            }

            for(i = 0; i < TX_SIZE_CONTEXTS; ++i)
            {
                for(j = 0; j < TX_SIZE_MAX_SB - 2; ++j)
                {
                    tmp = Vp9DecodeBool(bc, VP9_DEF_UPDATE_PROB);
                    STREAM_TRACE("tx16x16_prob_update", tmp);
                    if(tmp)
                    {
                        u8 *prob = &fc->a.tx16x16_prob[i][j];
                        *prob = Vp9ReadProbDiffUpdate(bc, *prob);
                        STREAM_TRACE("tx16x16_prob", *prob);
                    }
                }
            }

            for(i = 0; i < TX_SIZE_CONTEXTS; ++i)
            {
                for(j = 0; j < TX_SIZE_MAX_SB - 1; ++j)
                {
                    tmp = Vp9DecodeBool(bc, VP9_DEF_UPDATE_PROB);
                    STREAM_TRACE("tx32x32_prob_update", tmp);
                    if(tmp)
                    {
                        u8 *prob = &fc->a.tx32x32_prob[i][j];
                        *prob = Vp9ReadProbDiffUpdate(bc, *prob);
                        STREAM_TRACE("tx32x32_prob", *prob);
                    }
                }
            }
        }
    }

    /* Coefficient probability update */
    tmp = Vp9DecodeCoeffUpdate(bc, fc->a.prob_coeffs);
    if(tmp != HANTRO_OK)
        return(tmp);
    if(dec->transform_mode > ONLY_4X4)
    {
        tmp = Vp9DecodeCoeffUpdate(bc, fc->a.prob_coeffs8x8);
        if(tmp != HANTRO_OK)
            return (tmp);
    }

    if(dec->transform_mode > ALLOW_8X8)
    {
        tmp = Vp9DecodeCoeffUpdate(bc, fc->a.prob_coeffs16x16);
        if(tmp != HANTRO_OK)
            return(tmp);
    }
    if(dec->transform_mode > ALLOW_16X16)
    {
        tmp = Vp9DecodeCoeffUpdate(bc, fc->a.prob_coeffs32x32);
        if(tmp != HANTRO_OK)
            return (tmp);
    }

    dec->probs_decoded = 1;
    for(k = 0; k < MBSKIP_CONTEXTS; ++k)
    {
        tmp = Vp9DecodeBool(bc, VP9_DEF_UPDATE_PROB);
        STREAM_TRACE("mbskip_prob_update", tmp);
        if(tmp)
        {
            fc->a.mbskip_probs[k] = Vp9ReadProbDiffUpdate(bc, fc->a.mbskip_probs[k]);
            STREAM_TRACE("mbskip_prob", fc->a.mbskip_probs[k]);
        }
    }

    if(!dec->key_frame && !dec->intra_only)
    {
        for(i = 0; i < INTER_MODE_CONTEXTS; i++)
        {
            for(j = 0; j < VP9_INTER_MODES - 1; j++)
            {
                tmp = Vp9DecodeBool(bc, VP9_DEF_UPDATE_PROB);
                STREAM_TRACE("inter_mode_prob_update", tmp);
                if(tmp)
                {
                    u8 *prob = &fc->a.inter_mode_prob[i][j];
                    *prob = Vp9ReadProbDiffUpdate(bc, *prob);
                    STREAM_TRACE("inter_mode_prob", *prob);
                }
            }
        }

        if(dec->mcomp_filter_type == SWITCHABLE)
        {
            for(j = 0; j < VP9_SWITCHABLE_FILTERS + 1; ++j)
            {
                for(i = 0; i < VP9_SWITCHABLE_FILTERS - 1; ++i)
                {
                    tmp = Vp9DecodeBool(bc, VP9_DEF_UPDATE_PROB);
                    STREAM_TRACE("switchable_filter_prob_update", tmp);
                    if(tmp)
                    {
                        u8 *prob = &fc->a.switchable_interp_prob[j][i];
                        *prob = Vp9ReadProbDiffUpdate(bc, *prob);
                        STREAM_TRACE("switchable_interp_prob", *prob);
                    }
                }
            }
        }

        for(i = 0; i < INTRA_INTER_CONTEXTS; i++)
        {
            tmp = Vp9DecodeBool(bc, VP9_DEF_UPDATE_PROB);
            STREAM_TRACE("intra_inter_prob_update", tmp);
            if(tmp)
            {
                u8 *prob = &fc->a.intra_inter_prob[i];
                *prob = Vp9ReadProbDiffUpdate(bc, *prob);
                STREAM_TRACE("intra_inter_prob", *prob);
            }
        }

        /* Compound prediction mode probabilities */
        if(dec->allow_comp_inter_inter)
        {
            tmp = Vp9ReadBits(bc, 1);
            STREAM_TRACE("comp_pred_mode", tmp);
            dec->comp_pred_mode = tmp;
            if(tmp)
            {
                tmp = Vp9ReadBits(bc, 1);
                STREAM_TRACE("comp_pred_mode_hybrid", tmp);
                dec->comp_pred_mode += tmp;
                if(dec->comp_pred_mode == HYBRID_PREDICTION)
                {
                    for(i = 0; i < COMP_INTER_CONTEXTS; i++)
                    {
                        tmp = Vp9DecodeBool(bc, VP9_DEF_UPDATE_PROB);
                        STREAM_TRACE("comp_inter_prob_update", tmp);
                        if(tmp)
                        {
                            u8 *prob = &fc->a.comp_inter_prob[i];
                            *prob = Vp9ReadProbDiffUpdate(bc, *prob);
                            STREAM_TRACE("comp_inter_prob", *prob);
                        }
                    }
                }
            }
        }
        else
        {
            dec->comp_pred_mode = SINGLE_PREDICTION_ONLY;
        }

        if(dec->comp_pred_mode != COMP_PREDICTION_ONLY)
        {
            for(i = 0; i < REF_CONTEXTS; i++)
            {
                tmp = Vp9DecodeBool(bc, VP9_DEF_UPDATE_PROB);
                STREAM_TRACE("single_ref_prob_update", tmp);
                if(tmp)
                {
                    u8 *prob = &fc->a.single_ref_prob[i][0];
                    *prob = Vp9ReadProbDiffUpdate(bc, *prob);
                    STREAM_TRACE("single_ref_prob", *prob);
                }
                tmp = Vp9DecodeBool(bc, VP9_DEF_UPDATE_PROB);
                STREAM_TRACE("single_ref_prob_update", tmp);
                if(tmp)
                {
                    u8 *prob = &fc->a.single_ref_prob[i][1];
                    *prob = Vp9ReadProbDiffUpdate(bc, *prob);
                    STREAM_TRACE("single_ref_prob", *prob);
                }
            }
        }

        if(dec->comp_pred_mode != SINGLE_PREDICTION_ONLY)
        {
            for(i = 0; i < REF_CONTEXTS; i++)
            {
                tmp = Vp9DecodeBool(bc, VP9_DEF_UPDATE_PROB);
                STREAM_TRACE("comp_ref_prob_update", tmp);
                if(tmp)
                {
                    u8 *prob = &fc->a.comp_ref_prob[i];
                    *prob = Vp9ReadProbDiffUpdate(bc, *prob);
                    STREAM_TRACE("comp_ref_prob", *prob);
                }
            }
        }

        /* Superblock intra luma pred mode probabilities */
        for(j = 0; j < BLOCK_SIZE_GROUPS; ++j)
        {
            for(i = 0; i < 8; ++i)
            {
                tmp = Vp9DecodeBool(bc, VP9_DEF_UPDATE_PROB);
                STREAM_TRACE("ymode_prob_update", tmp);
                if(tmp)
                {
                    fc->a.sb_ymode_prob[j][i] =
                        Vp9ReadProbDiffUpdate(bc, fc->a.sb_ymode_prob[j][i]);
                    STREAM_TRACE("ymode_prob", fc->a.sb_ymode_prob[j][i]);
                }
            }
            tmp = Vp9DecodeBool(bc, VP9_DEF_UPDATE_PROB);
            STREAM_TRACE("ymode_prob_update", tmp);
            if(tmp)
            {
                fc->a.sb_ymode_prob_b[j][0] =
                    Vp9ReadProbDiffUpdate(bc, fc->a.sb_ymode_prob_b[j][0]);
                STREAM_TRACE("ymode_prob", fc->a.sb_ymode_prob_b[j][0]);
            }
        }

        for(j = 0; j < NUM_PARTITION_CONTEXTS; j++)
        {
            for(i = 0; i < PARTITION_TYPES - 1; i++)
            {
                tmp = Vp9DecodeBool(bc, VP9_DEF_UPDATE_PROB);
                STREAM_TRACE("partition_prob_update", tmp);
                if(tmp)
                {
                    u8 *prob = &fc->a.partition_prob[INTER_FRAME][j][i];
                    *prob = Vp9ReadProbDiffUpdate(bc, *prob);
                    STREAM_TRACE("partition_prob", *prob);
                }
            }
        }
        /* Motion vector tree update */
        tmp = Vp9DecodeMvUpdate(bc, dec);
        if(tmp != HANTRO_OK)
            return (tmp);
    }

    STREAM_TRACE("decoded_header_bytes", bc->pos);
    /* When first tile row has zero-height skip it. Cannot be done when pic height
    * smaller than 3 SB rows, in this case more than one tile rows may be skipped
    * and needs to be handled in stream parsing. */
    if((dec->height + 63) / 64 >= 3)
    {
        const u8 *strm = stream + dec->offset_to_dct_parts;
        if((u32)(1 << dec->log2_tile_rows) > (u32)(dec->height + 63) / 64)
        {
            for(j = (1 << dec->log2_tile_columns); j; j--)
            {
                tmp = ReadTileSize(strm);
                strm += 4 + tmp;
                dec->offset_to_dct_parts += 4 + tmp;
                STREAM_TRACE("Tile row with h=0, skipping", tmp);
                if(strm > stream + strm_len)
                    return HANTRO_NOK;
            }
        }
    }

    if(bc->strm_error)
        return (HANTRO_NOK);
    return (HANTRO_OK);
}

u32 Vp9SetPartitionOffsets(const u8 *stream, u32 len, struct Vp9Decoder *dec)
{
    u32 offset = 0;
    u32 base_offset;
    u32 ret_val = HANTRO_OK;
    stream;

    //stream += dec->frame_tag_size;
    //stream += dec->offset_to_dct_parts;

    //logd("here2: stream=%x, %x %x %x %x %x %x %x %x\n",
    //        stream, stream[0], stream[1], stream[2], stream[3]);

    base_offset = dec->frame_tag_size + dec->offset_to_dct_parts;
    dec->dct_partition_offsets[0] = base_offset + offset;
    if(dec->dct_partition_offsets[0] >= len)
    {
        dec->dct_partition_offsets[0] = len - 1;
        ret_val = HANTRO_NOK;
    }
    return ret_val;
}

