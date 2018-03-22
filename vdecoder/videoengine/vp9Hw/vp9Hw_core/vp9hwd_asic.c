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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

#include "vp9hwd_regdrv.h"
#include "vp9hwd_container.h"
#include "vp9hwd_deccfg.h"
#include "vp9hwd_treecoder.h"
#include "vp9hwd_probs.h"
#include "vp9hwd_entropymv.h"
#include "vp9hwd_buffer_queue.h"
#include "vp9hwd_stream.h"
#include "vp9hwd_dectypes.h"
#include "vp9hwd_output.h"
#include "vp9hwd_regdrv.h"
#include "vp9hwd_commonconfig.h"
#include "vp9Hw_dec.h"

#ifdef ASIC_TRACE_SUPPORT
  extern u32 ref_frame_mark;
#define TRACE_REF_FRAME_ADDR(v) ref_frame_mark = v;
#else
#define TRACE_REF_FRAME_ADDR(v)
#endif

#define MAX_TILE_COLS 20
#define MAX_TILE_ROWS 22

s32 Vp9ReallocateSegmentMap(struct Vp9DecContainer *dec_cont);

void Vp9AsicProbUpdate(void* decCont)
{
    u8 *asic_prob_base = NULL;
    struct Vp9Decoder *dec = NULL;
    struct DecAsicBuffers* pAnciBuffer = NULL;
    struct Vp9DecContainer* dec_cont = NULL;
    struct DWLLinearMem *segment_map = NULL;
    //FILE *Prob_table_fp;
    //u32 i;

    dec_cont = (struct Vp9DecContainer*)decCont;
    dec = (struct Vp9Decoder*)&dec_cont->decoder;
    pAnciBuffer = (struct DecAsicBuffers*)dec_cont->asic_buff;

    segment_map = pAnciBuffer->segment_map;
    asic_prob_base = (u8 *)pAnciBuffer->prob_tbl.virtual_address;

    /* Write probability tables to HW memory */
    #if 0
    CdcMemCopy(dec_cont->memops, asic_prob_base, &dec->entropy, sizeof(struct Vp9EntropyProbs));
    #else
    CdcMemCopy(dec_cont->memops, asic_prob_base, &dec->entropy, sizeof(struct Vp9EntropyProbs));
    CdcMemFlushCache(dec_cont->memops, asic_prob_base, sizeof(struct Vp9EntropyProbs));
    #endif

    //yangsj add
    #if 0
    if(dec_cont->pic_number == 1)
    {
        logd("************here2: save the vp9_prob_table.txt\n");
        Prob_table_fp = fopen("/tmp/vp9_prob_table.txt", "w+");
        if(Prob_table_fp != NULL)
        {

            for(i=0;i<(sizeof(struct Vp9EntropyProbs) + 3)/4 ;i++)
                fprintf(Prob_table_fp, "%08x\n",*(pAnciBuffer->prob_tbl.virtual_address+i));

            fclose(Prob_table_fp);
        }
    }
    #endif

    SetDecRegister(dec_cont->vp9_regs, HWIF_PROB_TAB_BASE_LSB,
        pAnciBuffer->prob_tbl.bus_address);
    SetDecRegister(dec_cont->vp9_regs, HWIF_CTX_COUNTER_BASE_LSB,
        pAnciBuffer->ctx_counters[pAnciBuffer->ctx_conter_index].bus_address);
    SetDecRegister(dec_cont->vp9_regs, HWIF_SEGMENT_READ_BASE_LSB,
        segment_map[dec_cont->active_segment_map].bus_address);
    SetDecRegister(dec_cont->vp9_regs, HWIF_SEGMENT_WRITE_BASE_LSB,
        segment_map[1 - dec_cont->active_segment_map].bus_address);

    /* Update active segment map for next frame */
    if(dec->segment_map_update)
        dec_cont->active_segment_map = 1 - dec_cont->active_segment_map;

}

void Vp9AsicSetOutput(void* decCont)
{
    struct Vp9Decoder *dec = NULL;
    struct DecAsicBuffers* pAnciBuffer = NULL;
    struct Vp9DecContainer* dec_cont = NULL;

    dec_cont = (struct Vp9DecContainer*)decCont;
    dec = (struct Vp9Decoder*)&dec_cont->decoder;
    pAnciBuffer = (struct DecAsicBuffers*)dec_cont->asic_buff;

    SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_OUT_DIS, 0);
    SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_OUT_YBASE_LSB,
                 pAnciBuffer->pictures[pAnciBuffer->out_buffer_i].bus_address);

    SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_OUT_CBASE_LSB,
                 pAnciBuffer->pictures_c[pAnciBuffer->out_buffer_i].bus_address);

    SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_OUT_DBASE_LSB,
                 pAnciBuffer->dir_mvs[pAnciBuffer->out_buffer_i].bus_address);

    /* Raster output configuration. */
    /* TODO(vmr): Add inversion for first version if needed and
     comment well. */

     SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_OUT_RS_E,
         dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN);

     if(dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN)
     {
        SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_RSY_BASE_LSB,
                   pAnciBuffer->raster_luma[pAnciBuffer->out_buffer_i].bus_address);
        SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_RSC_BASE_LSB,
                   pAnciBuffer->raster_chroma[pAnciBuffer->out_buffer_i].bus_address);
    }
}

void Vp9AsicSetReferenceFrames(void* decCont)
{
    u32 tmp1, tmp2, i;
    u32 cur_height, cur_width;
    u32 index_ref[VP9_ACTIVE_REFS];
    struct Vp9Decoder *dec = NULL;
    struct DecAsicBuffers* pAnciBuffer = NULL;
    struct Vp9DecContainer* dec_cont = NULL;
    //FILE* fp = NULL;

    dec_cont = (struct Vp9DecContainer*)decCont;
    dec = (struct Vp9Decoder*)&dec_cont->decoder;
    pAnciBuffer = (struct DecAsicBuffers*)dec_cont->asic_buff;

    for(i = 0; i < VP9_ACTIVE_REFS; i++)
    {
        index_ref[i] = Vp9BufferQueueGetRef(dec_cont->bq, dec->active_ref_idx[i]);
    }

    /* unrounded picture dimensions */
    cur_width = dec->width;
    cur_height = dec->height;

    /* last reference */
    tmp1 = pAnciBuffer->picture_info[index_ref[0]].coded_width;
    tmp2 = pAnciBuffer->picture_info[index_ref[0]].coded_height;
    SetDecRegister(dec_cont->vp9_regs, HWIF_LREF_WIDTH, tmp1);
    SetDecRegister(dec_cont->vp9_regs, HWIF_LREF_HEIGHT, tmp2);

    tmp1 = (tmp1 << VP9_REF_SCALE_SHIFT) / cur_width;
    tmp2 = (tmp2 << VP9_REF_SCALE_SHIFT) / cur_height;

    SetDecRegister(dec_cont->vp9_regs, HWIF_LREF_HOR_SCALE, tmp1);
    SetDecRegister(dec_cont->vp9_regs, HWIF_LREF_VER_SCALE, tmp2);

    SetDecRegister(dec_cont->vp9_regs, HWIF_REFER0_YBASE_LSB,
                 pAnciBuffer->pictures[index_ref[0]].bus_address);

    SetDecRegister(dec_cont->vp9_regs, HWIF_REFER0_CBASE_LSB,
                 pAnciBuffer->pictures_c[index_ref[0]].bus_address);

    /* Colocated MVs are always from previous decoded frame */
    SetDecRegister(dec_cont->vp9_regs, HWIF_REFER0_DBASE_LSB,
                 pAnciBuffer->dir_mvs[pAnciBuffer->prev_out_buffer_i].bus_address);
    SetDecRegister(dec_cont->vp9_regs, HWIF_LAST_SIGN_BIAS,
                 dec->ref_frame_sign_bias[LAST_FRAME]);

    /* golden reference */
    tmp1 = pAnciBuffer->picture_info[index_ref[1]].coded_width;
    tmp2 = pAnciBuffer->picture_info[index_ref[1]].coded_height;
    SetDecRegister(dec_cont->vp9_regs, HWIF_GREF_WIDTH, tmp1);
    SetDecRegister(dec_cont->vp9_regs, HWIF_GREF_HEIGHT, tmp2);

    tmp1 = (tmp1 << VP9_REF_SCALE_SHIFT) / cur_width;
    tmp2 = (tmp2 << VP9_REF_SCALE_SHIFT) / cur_height;
    SetDecRegister(dec_cont->vp9_regs, HWIF_GREF_HOR_SCALE, tmp1);
    SetDecRegister(dec_cont->vp9_regs, HWIF_GREF_VER_SCALE, tmp2);

    SetDecRegister(dec_cont->vp9_regs, HWIF_REFER4_YBASE_LSB,
                 pAnciBuffer->pictures[index_ref[1]].bus_address);
    SetDecRegister(dec_cont->vp9_regs, HWIF_REFER4_CBASE_LSB,
                 pAnciBuffer->pictures_c[index_ref[1]].bus_address);
    SetDecRegister(dec_cont->vp9_regs, HWIF_GREF_SIGN_BIAS,
                 dec->ref_frame_sign_bias[GOLDEN_FRAME]);

    /* alternate reference */
    tmp1 = pAnciBuffer->picture_info[index_ref[2]].coded_width;
    tmp2 = pAnciBuffer->picture_info[index_ref[2]].coded_height;
    SetDecRegister(dec_cont->vp9_regs, HWIF_AREF_WIDTH, tmp1);
    SetDecRegister(dec_cont->vp9_regs, HWIF_AREF_HEIGHT, tmp2);

    tmp1 = (tmp1 << VP9_REF_SCALE_SHIFT) / cur_width;
    tmp2 = (tmp2 << VP9_REF_SCALE_SHIFT) / cur_height;
    SetDecRegister(dec_cont->vp9_regs, HWIF_AREF_HOR_SCALE, tmp1);
    SetDecRegister(dec_cont->vp9_regs, HWIF_AREF_VER_SCALE, tmp2);

    SetDecRegister(dec_cont->vp9_regs, HWIF_REFER5_YBASE_LSB,
                 pAnciBuffer->pictures[index_ref[2]].bus_address);
    SetDecRegister(dec_cont->vp9_regs, HWIF_REFER5_CBASE_LSB,
                 pAnciBuffer->pictures_c[index_ref[2]].bus_address);
    SetDecRegister(dec_cont->vp9_regs, HWIF_AREF_SIGN_BIAS,
                 dec->ref_frame_sign_bias[ALTREF_FRAME]);
}

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

void Vp9AsicSetTileInfo(void* decCont)
{
    struct Vp9Decoder *dec = NULL;
    struct DecAsicBuffers* pAnciBuffer = NULL;
    struct Vp9DecContainer* dec_cont = NULL;
    u32 size/*,i*/;
    //FILE *Tile_info_fp;

    dec_cont = (struct Vp9DecContainer*)decCont;
    dec = (struct Vp9Decoder*)&dec_cont->decoder;
    pAnciBuffer = (struct DecAsicBuffers*)dec_cont->asic_buff;

    SetDecRegister(dec_cont->vp9_regs, HWIF_TILE_BASE_LSB,
        pAnciBuffer->tile_info.bus_address);
    SetDecRegister(dec_cont->vp9_regs, HWIF_TILE_ENABLE,
        dec->log2_tile_columns || dec->log2_tile_rows);

    if(dec->log2_tile_columns || dec->log2_tile_rows)
    {
        u32 tile_rows = (1 << dec->log2_tile_rows);
        u32 tile_cols = (1 << dec->log2_tile_columns);
        u32 i, j, h, tmp, prev_h, prev_w;
        u16 *p = (u16 *)pAnciBuffer->tile_info.virtual_address;
        u32 w_sbs = (pAnciBuffer->width + 63) / 64;
        u32 h_sbs = (pAnciBuffer->height + 63) / 64;

        /* write width + height for each tile in pic */
        for(i = 0, prev_h = 0; i < tile_rows; i++)
        {
            tmp = (i + 1) * h_sbs / tile_rows;
            h = tmp - prev_h;
            prev_h = tmp;

            /* When first tile row has zero height it's skipped by SW.
            * When pic height (rounded up) is less than 3 SB rows, more than one
            * tile row may be skipped and needs to be handled in sys model stream
            * parsing. HW does not support pic heights < 180 -> does not affect HW. */
            if (h_sbs >= 3 && !i && !h) continue;

            for(j = 0, prev_w = 0; j < tile_cols; j++)
            {
                tmp = (j + 1) * w_sbs / tile_cols;
                *p++ = tmp - prev_w;
                *p++ = h;
                prev_w = tmp;
            }
        }
        SetDecRegister(dec_cont->vp9_regs, HWIF_NUM_TILE_COLS, tile_cols);
        if(h_sbs >= 3)
            SetDecRegister(dec_cont->vp9_regs, HWIF_NUM_TILE_ROWS,MIN(tile_rows, h_sbs));
        else
            SetDecRegister(dec_cont->vp9_regs, HWIF_NUM_TILE_ROWS, tile_rows);
    }
    else
    {
        /* just one "tile", dimensions equal to pic size in SBs */
        u16 *p = (u16 *)pAnciBuffer->tile_info.virtual_address;
        p[0] = (pAnciBuffer->width + 63) / 64;
        p[1] = (pAnciBuffer->height + 63) / 64;
        SetDecRegister(dec_cont->vp9_regs, HWIF_NUM_TILE_COLS, 1);
        SetDecRegister(dec_cont->vp9_regs, HWIF_NUM_TILE_ROWS, 1);
    }

    //yangsj add

    size = (MAX_TILE_COLS * MAX_TILE_ROWS * 2 * sizeof(u16) + 15 + 16) & ~0xF;
    CdcMemFlushCache(dec_cont->memops, pAnciBuffer->tile_info.virtual_address, size);

    #if 0
    if(dec_cont->pic_number == 1)
    {
        logd("***********here3:save the vp9_Tile_info.txt\n");
        Tile_info_fp = fopen("/tmp/vp9_Tile_info.txt", "w+");
        if(Tile_info_fp != NULL)
        {

            for(i=0;i<(size/4);i++)
                fprintf(Tile_info_fp, "%08x\n",*(pAnciBuffer->tile_info.virtual_address+i));
            fclose(Tile_info_fp);
        }
    }
    #endif
}

/* macro to clip a value z, so that x <= z =< y */
#define CLIP3(x, y, z) (((z) < (x)) ? (x) : (((z) > (y)) ? (y) : (z)))

void Vp9AsicSetSegmentation(void* decCont)
{
    u32 *vp9_regs = NULL;
    u32 s;
    u32 segval[MAX_MB_SEGMENTS][SEG_LVL_MAX];
    //s32 segdata[SEG_LVL_MAX][4];

    struct DecAsicBuffers* pAnciBuffer = NULL;
    struct Vp9DecContainer* dec_cont = NULL;
    struct Vp9Decoder* dec = NULL;


    dec_cont = (struct Vp9DecContainer*)decCont;
    dec = (struct Vp9Decoder*)&dec_cont->decoder;
    pAnciBuffer = (struct DecAsicBuffers*)dec_cont->asic_buff;

    vp9_regs = dec_cont->vp9_regs;
    /* Resolution change will clear any previous segment map. */
    if(dec->resolution_change)
    {
        #if 0
        memset(pAnciBuffer->segment_map[0].virtual_address, 0,
              pAnciBuffer->segment_map_size);
        memset(pAnciBuffer->segment_map[1].virtual_address, 0,
              pAnciBuffer->segment_map_size);
        #else
        CdcMemSet(dec_cont->memops,pAnciBuffer->segment_map[0].virtual_address, 0,
                     pAnciBuffer->segment_map_size);
        CdcMemSet(dec_cont->memops, pAnciBuffer->segment_map[1].virtual_address, 0,
                     pAnciBuffer->segment_map_size);
        CdcMemFlushCache(dec_cont->memops,pAnciBuffer->segment_map[0].virtual_address,
                     pAnciBuffer->segment_map_size);
        CdcMemFlushCache(dec_cont->memops, pAnciBuffer->segment_map[1].virtual_address,
                    pAnciBuffer->segment_map_size);
        #endif
    }

    /* Segmentation */
    SetDecRegister(vp9_regs, HWIF_SEGMENT_E, dec->segment_enabled);
    SetDecRegister(vp9_regs, HWIF_SEGMENT_UPD_E, dec->segment_map_update);
    SetDecRegister(vp9_regs, HWIF_SEGMENT_TEMP_UPD_E,
                   dec->segment_map_temporal_update);

    /* Set filter level and QP for every segment ID. Initialize all
    * segments with default QP and filter level. */
    for(s = 0; s < MAX_MB_SEGMENTS; s++)
    {
        segval[s][0] = dec->qp_yac;
        segval[s][1] = dec->loop_filter_level;
        segval[s][2] = 0; /* segment ref_frame disabled */
        segval[s][3] = 0; /* segment skip disabled */
    }

    logi("here2: segval[0][0]=%x, segval[0][1]=%x, segval[0][2]=%x, \
         segval[0][3]=%x\n",   segval[0][0], segval[0][1], segval[0][2],
         segval[0][3]);

    /* If a feature is enabled for a segment, overwrite the default. */
    if(dec->segment_enabled)
    {
        s32(*segdata)[SEG_LVL_MAX] = NULL;
        segdata = dec->segment_feature_data;

        //segdata[SEG_LVL_MAX] = dec->segment_feature_data;
        if(dec->segment_feature_mode == VP9_SEG_FEATURE_ABS)
        {
            for(s = 0; s < MAX_MB_SEGMENTS; s++)
            {
                if(dec->segment_feature_enable[s][0]) segval[s][0] = segdata[s][0];
                if(dec->segment_feature_enable[s][1]) segval[s][1] = segdata[s][1];
                if(!dec->key_frame && dec->segment_feature_enable[s][2])
                    segval[s][2] = segdata[s][2] + 1;
                if(dec->segment_feature_enable[s][3])
                    segval[s][3] = 1;
            }
        }
        else /* delta mode */
        {
            for(s = 0; s < MAX_MB_SEGMENTS; s++)
            {
                if(dec->segment_feature_enable[s][0])
                    segval[s][0] = CLIP3(0, 255, dec->qp_yac + segdata[s][0]);
                if(dec->segment_feature_enable[s][1])
                    segval[s][1] = CLIP3(0, 63, dec->loop_filter_level + segdata[s][1]);
                if(!dec->key_frame && dec->segment_feature_enable[s][2])
                    segval[s][2] = segdata[s][2] + 1;
                if(dec->segment_feature_enable[s][3])
                    segval[s][3] = 1;
            }
        }
    }

    logi("here3: segval[0][0]=%x, segval[0][1]=%x, segval[0][2]=%x, \
         segval[0][3]=%x\n",   segval[0][0], segval[0][1], segval[0][2],
         segval[0][3]);

    /* Write QP, filter level, ref frame and skip for every segment */
    SetDecRegister(vp9_regs, HWIF_QUANT_SEG0, segval[0][0]);
    SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG0, segval[0][1]);
    SetDecRegister(vp9_regs, HWIF_REFPIC_SEG0, segval[0][2]);
    SetDecRegister(vp9_regs, HWIF_SKIP_SEG0, segval[0][3]);
    SetDecRegister(vp9_regs, HWIF_QUANT_SEG1, segval[1][0]);
    SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG1, segval[1][1]);
    SetDecRegister(vp9_regs, HWIF_REFPIC_SEG1, segval[1][2]);
    SetDecRegister(vp9_regs, HWIF_SKIP_SEG1, segval[1][3]);
    SetDecRegister(vp9_regs, HWIF_QUANT_SEG2, segval[2][0]);
    SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG2, segval[2][1]);
    SetDecRegister(vp9_regs, HWIF_REFPIC_SEG2, segval[2][2]);
    SetDecRegister(vp9_regs, HWIF_SKIP_SEG2, segval[2][3]);
    SetDecRegister(vp9_regs, HWIF_QUANT_SEG3, segval[3][0]);
    SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG3, segval[3][1]);
    SetDecRegister(vp9_regs, HWIF_REFPIC_SEG3, segval[3][2]);
    SetDecRegister(vp9_regs, HWIF_SKIP_SEG3, segval[3][3]);
    SetDecRegister(vp9_regs, HWIF_QUANT_SEG4, segval[4][0]);
    SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG4, segval[4][1]);
    SetDecRegister(vp9_regs, HWIF_REFPIC_SEG4, segval[4][2]);
    SetDecRegister(vp9_regs, HWIF_SKIP_SEG4, segval[4][3]);
    SetDecRegister(vp9_regs, HWIF_QUANT_SEG5, segval[5][0]);
    SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG5, segval[5][1]);
    SetDecRegister(vp9_regs, HWIF_REFPIC_SEG5, segval[5][2]);
    SetDecRegister(vp9_regs, HWIF_SKIP_SEG5, segval[5][3]);
    SetDecRegister(vp9_regs, HWIF_QUANT_SEG6, segval[6][0]);
    SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG6, segval[6][1]);
    SetDecRegister(vp9_regs, HWIF_REFPIC_SEG6, segval[6][2]);
    SetDecRegister(vp9_regs, HWIF_SKIP_SEG6, segval[6][3]);
    SetDecRegister(vp9_regs, HWIF_QUANT_SEG7, segval[7][0]);
    SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG7, segval[7][1]);
    SetDecRegister(vp9_regs, HWIF_REFPIC_SEG7, segval[7][2]);
    SetDecRegister(vp9_regs, HWIF_SKIP_SEG7, segval[7][3]);
}

void Vp9AsicSetLoopFilter(void* decCont)
{
    u32 *vp9_regs = NULL;
    struct Vp9Decoder *dec = NULL;
    struct DecAsicBuffers* pAnciBuffer = NULL;
    struct Vp9DecContainer* dec_cont = NULL;

    dec_cont = (struct Vp9DecContainer*)decCont;
    dec = (struct Vp9Decoder*)&dec_cont->decoder;
    pAnciBuffer = (struct DecAsicBuffers*)dec_cont->asic_buff;
    vp9_regs = dec_cont->vp9_regs;

    /* loop filter */
    SetDecRegister(vp9_regs, HWIF_FILT_LEVEL, dec->loop_filter_level);
    SetDecRegister(vp9_regs, HWIF_FILTERING_DIS, dec->loop_filter_level == 0);
    SetDecRegister(vp9_regs, HWIF_FILT_SHARPNESS, dec->loop_filter_sharpness);

    if(dec->mode_ref_lf_enabled)
    {
        SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_0, dec->mb_ref_lf_delta[0]);
        SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_1, dec->mb_ref_lf_delta[1]);
        SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_2, dec->mb_ref_lf_delta[2]);
        SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_3, dec->mb_ref_lf_delta[3]);
        SetDecRegister(vp9_regs, HWIF_FILT_MB_ADJ_0, dec->mb_mode_lf_delta[0]);
        SetDecRegister(vp9_regs, HWIF_FILT_MB_ADJ_1, dec->mb_mode_lf_delta[1]);
    }
    else
    {
        SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_0, 0);
        SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_1, 0);
        SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_2, 0);
        SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_3, 0);
        SetDecRegister(vp9_regs, HWIF_FILT_MB_ADJ_0, 0);
        SetDecRegister(vp9_regs, HWIF_FILT_MB_ADJ_1, 0);
    }

    SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_VERT_FILT_BASE_LSB,
            pAnciBuffer->filter_mem.bus_address);
    SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_BSD_CTRL_BASE_LSB,
            pAnciBuffer->bsd_control_mem.bus_address);
}

#define NEXT_MULTIPLE(value, n) (((value) + (n) - 1) & ~((n) - 1))

void Vp9AsicSetPictureDimensions(void* decCont)
{
    struct Vp9Decoder *dec = NULL;
    struct DecAsicBuffers* pAnciBuffer = NULL;
    struct Vp9DecContainer* dec_cont = NULL;

    dec_cont = (struct Vp9DecContainer*)decCont;
    dec = (struct Vp9Decoder*)&dec_cont->decoder;
    pAnciBuffer = (struct DecAsicBuffers*)dec_cont->asic_buff;

    /* Write dimensions for the current picture
     (This is needed when scaling is used) */

    SetDecRegister(dec_cont->vp9_regs, HWIF_PIC_WIDTH_IN_CBS,
                 (dec->width + 7) / 8);
    SetDecRegister(dec_cont->vp9_regs, HWIF_PIC_HEIGHT_IN_CBS,
                 (dec->height + 7) / 8);
    SetDecRegister(dec_cont->vp9_regs, HWIF_PIC_WIDTH_4X4,
                 NEXT_MULTIPLE(dec->width, 8) >> 2);
    SetDecRegister(dec_cont->vp9_regs, HWIF_PIC_HEIGHT_4X4,
                 NEXT_MULTIPLE(dec->height, 8) >> 2);
}

void Vp9AsicInitPicture(void* decCont)
{
    u32 *vp9_regs = NULL;
    struct Vp9Decoder *dec = NULL;
    struct DecAsicBuffers* pAnciBuffer = NULL;
    struct Vp9DecContainer* dec_cont = NULL;

    dec_cont = (struct Vp9DecContainer*)decCont;
    dec = (struct Vp9Decoder*)&dec_cont->decoder;
    pAnciBuffer = (struct DecAsicBuffers*)dec_cont->asic_buff;

    vp9_regs = dec_cont->vp9_regs;

#ifdef SET_EMPTY_PICTURE_DATA /* USE THIS ONLY FOR DEBUGGING PURPOSES */
    Vp9SetEmptyPictureData(dec_cont);
#endif

    Vp9AsicSetOutput(decCont);

    if(!dec->key_frame && !dec->intra_only)
    {
        Vp9AsicSetReferenceFrames(decCont);
    }

    Vp9AsicSetTileInfo(decCont);

    Vp9AsicSetSegmentation(decCont);

    Vp9AsicSetLoopFilter(decCont);

    Vp9AsicSetPictureDimensions(decCont);

    SetDecRegister(vp9_regs, HWIF_BITDEPTH_Y, dec->bit_depth);
    SetDecRegister(vp9_regs, HWIF_BITDEPTH_C, dec->bit_depth);

    u32 rs_out_bit_depth;

    rs_out_bit_depth = dec->bit_depth;
    if(dec_cont->bNeedConvert10bitTo8bit == 1)
    {
        rs_out_bit_depth = 8;
    }

    logv("dec_cont->bNeedConvert10bitTo8bit = %d, rs_out_bit_depth=%d\n",
          dec_cont->bNeedConvert10bitTo8bit,rs_out_bit_depth);
    SetDecRegister(vp9_regs, HWIF_RS_OUT_BITDEPTH, rs_out_bit_depth);

    u32 ref_shift = 0 , pp_shift = 0;

    dec_cont->pack_pixel_to_msbs
        = (rs_out_bit_depth>8)? DEC_BITPACKING_MSB : DEC_BITPACKING_LSB;
    //logd("*******  dec_cont->pack_pixel_to_msbs =%d,rs_out_bit_depth=%d\n",
    //       dec_cont->pack_pixel_to_msbs,rs_out_bit_depth );
    if(dec_cont->pack_pixel_to_msbs)
    {
        pp_shift = 16 - rs_out_bit_depth;
    }

    SetDecRegister(vp9_regs, HWIF_PIX_SHIFT, ref_shift);
    SetDecRegister(vp9_regs, HWIF_PP_PIX_SHIFT, pp_shift);

    /* In case of 16-bit content and big-endian architecture we need to prevent
    * hardware from swapping the bytes within 16-bit pixel values written out. */
    int x = 1;
    u8 big_endian = *(char*)&x != 1;

    if(big_endian && dec->bit_depth > 8)
    {
        SetDecRegister(vp9_regs, HWIF_DEC_PIC_SWAP,
                       GetDecRegister(vp9_regs, HWIF_DEC_PIC_SWAP) ^ 0x1);
        if(rs_out_bit_depth > 8)
        {
            SetDecRegister(vp9_regs, HWIF_DEC_RSCAN_SWAP,
                         GetDecRegister(vp9_regs, HWIF_DEC_RSCAN_SWAP) ^ 0x1);
        }
    }

    /* QP deltas applied after choosing base QP based on segment ID. */
    SetDecRegister(vp9_regs, HWIF_QP_DELTA_Y_DC, dec->qp_ydc);
    SetDecRegister(vp9_regs, HWIF_QP_DELTA_CH_DC, dec->qp_ch_dc);
    SetDecRegister(vp9_regs, HWIF_QP_DELTA_CH_AC, dec->qp_ch_ac);
    SetDecRegister(vp9_regs, HWIF_LOSSLESS_E, dec->lossless);

    /* Mark intra_only frame also a keyframe but copy inter probabilities to
     partition probs for the stream decoding. */
    SetDecRegister(vp9_regs, HWIF_IDR_PIC_E, (dec->key_frame || dec->intra_only));
    SetDecRegister(vp9_regs, HWIF_TRANSFORM_MODE, dec->transform_mode);
    SetDecRegister(vp9_regs, HWIF_MCOMP_FILT_TYPE, dec->mcomp_filter_type);
    SetDecRegister(vp9_regs, HWIF_HIGH_PREC_MV_E,
                     !dec->key_frame && dec->allow_high_precision_mv);
    SetDecRegister(vp9_regs, HWIF_COMP_PRED_MODE, dec->comp_pred_mode);

    logi("********error:dec->error_resilient=%d, dec->key_frame=%d,dec->prev_is_key_frame=%d, \
        dec->intra_only=%d, dec->resolution_change=%d, dec->prev_show_frame=%d\n", \
        dec->error_resilient, dec->key_frame,dec->prev_is_key_frame, \
        dec->intra_only, dec->resolution_change, dec->prev_show_frame); \

    SetDecRegister(vp9_regs, HWIF_TEMPOR_MVP_E,
                     !dec->error_resilient && !dec->key_frame &&
                     !dec->prev_is_key_frame && !dec->intra_only &&
                     !dec->resolution_change && dec->prev_show_frame);

    SetDecRegister(vp9_regs, HWIF_COMP_PRED_FIXED_REF, dec->comp_fixed_ref);
    SetDecRegister(vp9_regs, HWIF_COMP_PRED_VAR_REF0, dec->comp_var_ref[0]);
    SetDecRegister(vp9_regs, HWIF_COMP_PRED_VAR_REF1, dec->comp_var_ref[1]);

    if(!dec_cont->conceal)
    {
        if(dec->key_frame)
            SetDecRegister(dec_cont->vp9_regs, HWIF_WRITE_MVS_E, 0);
        else
            SetDecRegister(dec_cont->vp9_regs, HWIF_WRITE_MVS_E, 1);
    }
}

void Vp9AsicStrmPosUpdate(void* decCont, u32 strm_bus_address, u32 data_len)
{
    u32 tmp, hw_bit_pos, tmp_addr;
    struct Vp9Decoder *dec = NULL;
    struct DecAsicBuffers* pAnciBuffer = NULL;
    struct Vp9DecContainer* dec_cont = NULL;
    //FILE * Bistream_fp;
    //u32 i;
    //u32* tmp_addr1= NULL;
    //u8* tmp_addr2= NULL;

    dec_cont = (struct Vp9DecContainer*)decCont;
    dec = (struct Vp9Decoder*)&dec_cont->decoder;
    pAnciBuffer = (struct DecAsicBuffers*)dec_cont->asic_buff;
    tmp = dec->frame_tag_size + dec->offset_to_dct_parts;

    logi("*******dec->frame_tag_size=%d,  dec->offset_to_dct_parts=%d\n",
         dec->frame_tag_size, dec->offset_to_dct_parts);

    tmp_addr = strm_bus_address + tmp;
    hw_bit_pos = (tmp_addr & DEC_HW_ALIGN_MASK) * 8;
    tmp_addr &= (~DEC_HW_ALIGN_MASK); /* align the base */
    SetDecRegister(dec_cont->vp9_regs, HWIF_STRM_START_BIT, hw_bit_pos);
    SetDecRegister(dec_cont->vp9_regs, HWIF_STREAM_BASE_LSB, tmp_addr);
    /* Total stream length passed to HW. */
    tmp = data_len - (tmp_addr - strm_bus_address);
    SetDecRegister(dec_cont->vp9_regs, HWIF_STREAM_LEN, tmp);
}

#if 0
void Vp9AsicStrmPosUpdate_sbmback(void* decCont,u32 strm_bus_address, u32 data_len)
{
    u32 tmp, hw_bit_pos, tmp_addr;
    struct Vp9Decoder *dec = NULL;
    struct Vp9DecContainer* dec_cont = NULL;

    dec_cont = (struct Vp9DecContainer*)decCont;
    dec = (struct Vp9Decoder*)&dec_cont->decoder;
    tmp = dec->frame_tag_size + dec->offset_to_dct_parts;

    //logd("*******dec->frame_tag_size=%d,  dec->offset_to_dct_parts=%d\n",
    //     dec->frame_tag_size, dec->offset_to_dct_parts);

    tmp_addr = strm_bus_address + tmp;
    hw_bit_pos = (tmp_addr & DEC_HW_ALIGN_MASK) * 8;
    tmp_addr &= (~DEC_HW_ALIGN_MASK); /* align the base */
    SetDecRegister(dec_cont->vp9_regs, HWIF_STRM_START_BIT, hw_bit_pos);
    SetDecRegister(dec_cont->vp9_regs, HWIF_STREAM_BASE_LSB, tmp_addr);
    tmp = data_len - (tmp_addr - strm_bus_address);
    SetDecRegister(dec_cont->vp9_regs, HWIF_STREAM_LEN, tmp);
}
#endif

//* wait ve interrupt
u32 Vp9AsicSync(struct Vp9DecContainer *dec_cont)
{
    u32 asic_status = 0;
    s32 ret;

    ret = DWL_HW_WAIT_OK;
    if(ret != DWL_HW_WAIT_OK)
    {
        //loge("DWLWaitHwReady\n");
        //logd("DWLWaitHwReady returned: %d\n", ret);
        /* Reset HW */
        SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_IRQ_STAT, 0);
        SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_IRQ, 0);
        SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_E, 0);
        DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1, 0);
        dec_cont->asic_running = 0;
        //logd("dec_cont->asic_running=%d, %d\n", dec_cont->asic_running, __LINE__);
        DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
        return(ret == DWL_HW_WAIT_ERROR) ? VP9HWDEC_SYSTEM_ERROR
                                      : VP9HWDEC_SYSTEM_TIMEOUT;
    }
    RefreshDecRegisters(dec_cont->dwl, dec_cont->core_id, dec_cont->vp9_regs);

    /* React to the HW return value */
    asic_status = GetDecRegister(dec_cont->vp9_regs, HWIF_DEC_IRQ_STAT);
    SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_IRQ, 0); /* just in case */
    /* HW done, release it! */
    DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1, dec_cont->vp9_regs[1]);
    dec_cont->asic_running = 0;
    DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
    return asic_status;
}

const vp9_tree_index vp9_coefmodel_tree[6] = {
    -DCT_EOB_MODEL_TOKEN, 2,          /* 0 = EOB */
    -ZERO_TOKEN,          4,          /* 1 = ZERO */
    -ONE_TOKEN,           -TWO_TOKEN, /* 2 = ONE */
};

static void UpdateCoefProbs(
    u8 dst_coef_probs[BLOCK_TYPES][REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS]
                     [ENTROPY_NODES_PART1],
    u8 pre_coef_probs[BLOCK_TYPES][REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS]
                     [ENTROPY_NODES_PART1],
    vp9_coeff_count *coef_counts,
    u32 (*eob_counts)[REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS], int count_sat,
    int update_factor)
{
    int t, i, j, k, l, count;
    unsigned int branch_ct[ENTROPY_NODES][2];
    vp9_prob coef_probs[ENTROPY_NODES];
    int factor;

    for(i = 0; i < BLOCK_TYPES; ++i)
    {
        for (j = 0; j < REF_TYPES; ++j)
        {
            for (k = 0; k < COEF_BANDS; ++k)
            {
                for(l = 0; l < PREV_COEF_CONTEXTS; ++l)
                {
                    if(l >= 3 && k == 0)
                        continue;
                    Vp9TreeProbsFromDistribution(vp9_coefmodel_tree, coef_probs,
                                       branch_ct, coef_counts[i][j][k][l], 0);
                    branch_ct[0][1] = eob_counts[i][j][k][l] - branch_ct[0][0];
                    coef_probs[0] = GetBinaryProb(branch_ct[0][0], branch_ct[0][1]);
                    for(t = 0; t < UNCONSTRAINED_NODES; ++t)
                    {
                        count = branch_ct[t][0] + branch_ct[t][1];
                        count = count > count_sat ? count_sat : count;
                        factor = (update_factor * count / count_sat);
                        dst_coef_probs[i][j][k][l][t] = WeightedProb(
                            pre_coef_probs[i][j][k][l][t], coef_probs[t], factor);
                    }
                }
             }
        }
    }
}

#define COEF_COUNT_SAT 24
#define COEF_MAX_UPDATE_FACTOR 112
#define COEF_COUNT_SAT_KEY 24
#define COEF_MAX_UPDATE_FACTOR_KEY 112
#define COEF_COUNT_SAT_AFTER_KEY 24
#define COEF_MAX_UPDATE_FACTOR_AFTER_KEY 128

void Vp9AdaptCoefProbs(struct Vp9Decoder *cm)
{
    int count_sat;
    int update_factor; /* denominator 256 */

    if(cm->key_frame || cm->intra_only)
    {
        update_factor = COEF_MAX_UPDATE_FACTOR_KEY;
        count_sat = COEF_COUNT_SAT_KEY;
    }
    else if(cm->prev_is_key_frame)
    {
        update_factor = COEF_MAX_UPDATE_FACTOR_AFTER_KEY; /* adapt quickly */
        count_sat = COEF_COUNT_SAT_AFTER_KEY;
    }
    else
    {
        update_factor = COEF_MAX_UPDATE_FACTOR;
        count_sat = COEF_COUNT_SAT;
    }

    UpdateCoefProbs(cm->entropy.a.prob_coeffs, cm->prev_ctx.prob_coeffs,
                  cm->ctx_ctr.count_coeffs, cm->ctx_ctr.count_eobs[TX_4X4],
                  count_sat, update_factor);
    UpdateCoefProbs(cm->entropy.a.prob_coeffs8x8, cm->prev_ctx.prob_coeffs8x8,
                  cm->ctx_ctr.count_coeffs8x8, cm->ctx_ctr.count_eobs[TX_8X8],
                  count_sat, update_factor);
    UpdateCoefProbs(cm->entropy.a.prob_coeffs16x16, cm->prev_ctx.prob_coeffs16x16,
                  cm->ctx_ctr.count_coeffs16x16,
                  cm->ctx_ctr.count_eobs[TX_16X16], count_sat, update_factor);
    UpdateCoefProbs(cm->entropy.a.prob_coeffs32x32, cm->prev_ctx.prob_coeffs32x32,
                  cm->ctx_ctr.count_coeffs32x32,
                  cm->ctx_ctr.count_eobs[TX_32X32], count_sat, update_factor);
}

void Vp9UpdateProbabilities(struct Vp9DecContainer *dec_cont)
{
    //int i = 0, i0=0, i1=0, i2=0, i3=0;
    //int j = 0;
    //FILE *fp = NULL;
    /* Read context counters from HW output memory. */

    #if 0
    CdcMemFlushCache(dec_cont->memops,
                dec_cont->asic_buff->ctx_counters[dec_cont->asic_buff->ctx_conter_index].virtual_address,
                sizeof(struct Vp9EntropyCounts));
    #endif
    DWLmemcpy(&dec_cont->decoder.ctx_ctr,
            dec_cont->asic_buff->ctx_counters[dec_cont->asic_buff->ctx_conter_index].virtual_address,
            sizeof(struct Vp9EntropyCounts));

    dec_cont->asic_buff->ctx_conter_index++;
    if(dec_cont->asic_buff->ctx_conter_index == 5)
    {
        dec_cont->asic_buff->ctx_conter_index = 0;
    }
    /* Backward adaptation of probs based on context counters. */
    if(!dec_cont->decoder.error_resilient &&
      !dec_cont->decoder.frame_parallel_decoding)
    {
        Vp9AdaptCoefProbs(&dec_cont->decoder);
        if(!dec_cont->decoder.key_frame && !dec_cont->decoder.intra_only)
        {
            Vp9AdaptModeProbs(&dec_cont->decoder);
            Vp9AdaptModeContext(&dec_cont->decoder);
            Vp9AdaptNmvProbs(&dec_cont->decoder);
        }
    }

    /* Store the adapted probs as base for following frames. */
    Vp9StoreProbs(&dec_cont->decoder);
}

void Vp9UpdateRefs(struct Vp9DecContainer *dec_cont, u32 corrupted)
{
    struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
    //int i;

    if(!corrupted || (corrupted && dec_cont->pic_number != 1))
    {
        if(dec_cont->decoder.reset_frame_flags)
        {
            Vp9BufferQueueUpdateRef(dec_cont->bq, (1 << NUM_REF_FRAMES) - 1,
                REFERENCE_NOT_SET);
            dec_cont->decoder.reset_frame_flags = 0;
        }
        Vp9BufferQueueUpdateRef(dec_cont->bq,
                                dec_cont->decoder.refresh_frame_flags,
                                asic_buff->out_buffer_i);
    }

    if(!dec_cont->decoder.show_frame ||
        (dec_cont->pic_number != 1 && corrupted))
    {
        /* If the picture will not be outputted, we need to remove ref used to
          protect the output. */
        //logd("**********remove ref:index=%d\n", asic_buff->out_buffer_i);
        Vp9BufferQueueRemoveRef(dec_cont->bq, asic_buff->out_buffer_i);
    }

    //logd("***********\n\n print the ref status\n");
    //printf_ref_status(dec_cont->bq);

}

s32 Vp9FreeRefFrm(struct Vp9DecContainer *dec_cont, u32 index)
{
    struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
    if(asic_buff->pictures[index].virtual_address != NULL)
        DWLFreeRefFrm(dec_cont->dwl, &asic_buff->pictures[index], dec_cont->memops,
                                                            dec_cont->veOpsS,
                                                            dec_cont->pVeOpsSelf);
    if(asic_buff->pictures_c[index].virtual_address != NULL)
        DWLFreeRefFrm(dec_cont->dwl, &asic_buff->pictures_c[index], dec_cont->memops,
                                                            dec_cont->veOpsS,
                                                            dec_cont->pVeOpsSelf);
    if(asic_buff->dir_mvs[index].virtual_address != NULL)
        DWLFreeRefFrm(dec_cont->dwl, &asic_buff->dir_mvs[index], dec_cont->memops,
                                                            dec_cont->veOpsS,
                                                            dec_cont->pVeOpsSelf);
    if(asic_buff->raster_luma[index].virtual_address != NULL)
    {
        //DWLFreeRefFrm(dec_cont->dwl, &asic_buff->raster_luma[index], dec_cont->memops);
        asic_buff->raster_luma[index].virtual_address = NULL;
    }
    if (asic_buff->raster_chroma[index].virtual_address != NULL)
    {
        //DWLFreeRefFrm(dec_cont->dwl, &asic_buff->raster_chroma[index], dec_cont->memops);
        asic_buff->raster_chroma[index].virtual_address = NULL;
    }
    return HANTRO_OK;
}

s32 Vp9FreeSegmentMap(struct Vp9DecContainer *dec_cont)
{
    u32 i;
    struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

    for(i = 0; i < 2; i++)
    {
        if(asic_buff->segment_map[i].virtual_address != NULL)
        {
            DWLFreeLinear(dec_cont->dwl, &asic_buff->segment_map[i], dec_cont->memops,
                                                            dec_cont->veOpsS,
                                                            dec_cont->pVeOpsSelf);
        }
    }
    #if 0
    DWLmemset(asic_buff->segment_map, 0, sizeof(asic_buff->segment_map));
    #else
    CdcMemSet(dec_cont->memops, asic_buff->segment_map, 0, sizeof(asic_buff->segment_map));
    #endif
    return HANTRO_OK;
}

void Vp9AsicReleasePictures(struct Vp9DecContainer *dec_cont)
{
    u32 i;
    struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

    for(i = 0; i < dec_cont->num_buffers; i++)
    {

        Vp9FreeRefFrm(dec_cont, i);
    }

    if(dec_cont->bq)
    {
        Vp9BufferQueueRelease(dec_cont->bq);
        dec_cont->bq = NULL;
    }
    DWLmemset(asic_buff->pictures, 0, sizeof(asic_buff->pictures));
    Vp9FreeSegmentMap(dec_cont);
}

s32 Vp9AllocateSegmentMap(struct Vp9DecContainer *dec_cont)
{
    s32 dwl_ret;
    u32 num_ctbs, memory_size;
    const void *dwl = dec_cont->dwl;
    struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

    num_ctbs = ((asic_buff->width + 63) / 64) * ((asic_buff->height + 63) / 64);
    memory_size = num_ctbs * 32; /* Segment map uses 32 bytes / CTB */
    dwl_ret = DWLMallocLinear(dwl, memory_size, &asic_buff->segment_map[0], dec_cont->memops,
                                                            (void *)dec_cont->veOpsS,
                                                            dec_cont->pVeOpsSelf);
    dwl_ret |= DWLMallocLinear(dwl, memory_size, &asic_buff->segment_map[1], dec_cont->memops,
                                                            (void *)dec_cont->veOpsS,
                                                            dec_cont->pVeOpsSelf);

    if(dwl_ret != DWL_OK)
    {
        logd("here3 Vp9AsicReleasePictures\n");
        Vp9AsicReleasePictures(dec_cont);
        return HANTRO_NOK;
    }

    asic_buff->segment_map_size = memory_size;
    #if 0
    DWLmemset(asic_buff->segment_map[0].virtual_address, 0,
            asic_buff->segment_map_size);
    DWLmemset(asic_buff->segment_map[1].virtual_address, 0,
            asic_buff->segment_map_size);
    #else
    CdcMemSet(dec_cont->memops, asic_buff->segment_map[0].virtual_address,
              0,asic_buff->segment_map_size);
    CdcMemSet(dec_cont->memops, asic_buff->segment_map[1].virtual_address,
              0,asic_buff->segment_map_size);
    CdcMemFlushCache(dec_cont->memops,
              asic_buff->segment_map[0].virtual_address,
              asic_buff->segment_map_size);
    CdcMemFlushCache(dec_cont->memops,
              asic_buff->segment_map[1].virtual_address,
             asic_buff->segment_map_size);
    #endif
    return HANTRO_OK;
}

extern s32 DWLMallocDispFrm(const void *instance, u32 size,
                                   struct DWLLinearMem *info,struct ScMemOpsS *memops);

s32 Vp9MallocRefFrm(struct Vp9DecContainer *dec_cont, u32 index)
{
    struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
    u32 num_ctbs, luma_size, chroma_size, dir_mvs_size;
    u32 raster_luma_size, raster_chroma_size;
    s32 dwl_ret;

    u32 bytes_per_sample =(dec_cont->decoder.bit_depth > 8) ? 2 : 1;
    luma_size = asic_buff->width * asic_buff->height * bytes_per_sample;
    chroma_size = asic_buff->width * (asic_buff->height / 2) * bytes_per_sample;
    num_ctbs = ((asic_buff->width + 63) / 64) * ((asic_buff->height + 63) / 64);
    dir_mvs_size = num_ctbs * 64 * 16; /* MVs (16 MBs / CTB * 16 bytes / MB) */
    raster_luma_size = NEXT_MULTIPLE(asic_buff->width, 32) * asic_buff->height *bytes_per_sample;
    raster_chroma_size = raster_luma_size / 2;

    TRACE_REF_FRAME_ADDR(1);

    /* luma */
    dwl_ret =
         DWLMallocRefFrm(dec_cont->dwl, luma_size, &asic_buff->pictures[index], dec_cont->memops,
                                                                            dec_cont->veOpsS,
                                                                            dec_cont->pVeOpsSelf);
    /* chroma */
    dwl_ret |=
         DWLMallocRefFrm(dec_cont->dwl, chroma_size, &asic_buff->pictures_c[index],dec_cont->memops,
                                                                            dec_cont->veOpsS,
                                                                            dec_cont->pVeOpsSelf);

    TRACE_REF_FRAME_ADDR(0);

    /* colocated mvs */
    dwl_ret |=
         DWLMallocRefFrm(dec_cont->dwl, dir_mvs_size, &asic_buff->dir_mvs[index],
                         dec_cont->memops,dec_cont->veOpsS,dec_cont->pVeOpsSelf);

    #if 1
            if(dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN)
            {
                /* raster luma */
                dwl_ret |= DWLMallocDispFrm(dec_cont->dwl, raster_luma_size,
                            &asic_buff->raster_luma[index], dec_cont->memops);
                /* raster chroma */
                dwl_ret |= DWLMallocDispFrm(dec_cont->dwl, raster_chroma_size,
                            &asic_buff->raster_chroma[index], dec_cont->memops);
            }
    #endif

    if(dwl_ret != DWL_OK)
    {
        return HANTRO_NOK;
    }
    return HANTRO_OK;
}

s32 Vp9AsicAllocatePictures(struct Vp9DecContainer *dec_cont)
{
    u32 i;
    struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

    dec_cont->active_segment_map = 0;
    Vp9AllocateSegmentMap(dec_cont);
    DWLmemset(asic_buff->pictures, 0, sizeof(asic_buff->pictures));
    dec_cont->bq = Vp9BufferQueueInitialize(dec_cont->num_buffers);
    if(dec_cont->bq == NULL)
    {
        Vp9AsicReleasePictures(dec_cont);
        logd("****here1: Vp9AsicAllocatePictures failed\n");
        return -1;
    }

    for(i = 0; i < dec_cont->num_buffers; i++)
    {
        if(Vp9MallocRefFrm(dec_cont, i))
        {
            Vp9AsicReleasePictures(dec_cont);
            logd("****here2: Vp9AsicAllocatePictures failed\n");
            return -1;
        }
    }
    //ASSERT(asic_buff->width / 4 < 0x1FFF);
    //ASSERT(asic_buff->height / 4 < 0x1FFF);
    SetDecRegister(dec_cont->vp9_regs, HWIF_MAX_CB_SIZE, 6); /* 64x64 */
    SetDecRegister(dec_cont->vp9_regs, HWIF_MIN_CB_SIZE, 3); /* 8x8 */
    asic_buff->out_buffer_i = -1;
    return 0;
}

#if 0
u32 RequiredBufferCount(struct Vp9DecContainer *dec_cont)
{
    return (Vp9BufferQueueCountReferencedBuffers(dec_cont->bq, (int)dec_cont->num_buffers) + 2);
}
#endif

#if 1
s32 Vp9AllocateFrame(struct Vp9DecContainer *dec_cont, u32 index)
{
    if(Vp9MallocRefFrm(dec_cont, index))
    {
        return HANTRO_NOK;
    }
    dec_cont->num_buffers++;
    Vp9BufferQueueAddBuffer(dec_cont->bq);
    return HANTRO_OK;
}
#endif

s32 Vp9ReallocateFrame(struct Vp9DecContainer *dec_cont, u32 index)
{
    s32 ret = 0;
    Vp9FreeRefFrm(dec_cont, index);
    ret = Vp9MallocRefFrm(dec_cont, index);
    return ret;
}

s32 Vp9GetRefFrm(Vp9HwDecodeContext* pVp9HwContext)
{
    Vp9HwDecode*    pVp9HwDec = NULL;
    //struct ScMemOpsS *_memops = NULL;
    struct Vp9DecContainer* dec_cont = NULL;
    pVp9HwDec = (Vp9HwDecode*)pVp9HwContext->pVp9HwDec;
    dec_cont = (struct Vp9DecContainer*)pVp9HwDec->dec_cont;
    //_memops = pVp9HwContext->vconfig.memops;

    //u32 limit = dec_cont->dynamic_buffer_limit;
    struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
    u32 num_sbs = ((asic_buff->width + 63) / 64) * ((asic_buff->height + 63) / 64);
    u32 bytes_per_sample = (dec_cont->decoder.bit_depth > 8) ? 2 : 1;
    u32 ref_frame_size = asic_buff->width * asic_buff->height * bytes_per_sample;
    u32 raster_luma_size = NEXT_MULTIPLE(asic_buff->width, 32) *
                         asic_buff->height * bytes_per_sample;
    //u32 bytes_offset = num_sbs*64*64;
    u32 limit = 0;

    #if 0
    if(RequiredBufferCount(dec_cont) < limit)
    {
        limit = RequiredBufferCount(dec_cont);
    }
    #endif

    limit = dec_cont->num_buffers;
    asic_buff->out_buffer_i = Vp9BufferQueueGetBuffer(dec_cont->bq, limit);

    if(asic_buff->out_buffer_i < 0)
    {
        if(Vp9AllocateFrame(dec_cont, dec_cont->num_buffers))
        {
            loge("********Vp9AllocateFrame error: return DEC_MEMFAIL\n");
            return DEC_MEMFAIL;
        }
        asic_buff->out_buffer_i = Vp9BufferQueueGetBuffer(dec_cont->bq, limit);
    }

    /* Reallocate picture memories if needed */
     if(asic_buff->pictures[asic_buff->out_buffer_i].logical_size <
        ref_frame_size ||
      asic_buff->dir_mvs[asic_buff->out_buffer_i].logical_size <
        num_sbs * 64 * 16 ||
      (dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN &&
       asic_buff->raster_luma[asic_buff->out_buffer_i].logical_size <
         raster_luma_size))
    {
        /* Reallocate bigger picture buffer into current index */
        logd("**********need realloc frame buffer\n");
        if(Vp9ReallocateFrame(dec_cont, asic_buff->out_buffer_i))
        {
            loge("****faile: Vp9ReallocateFrame error: return DEC_MEMFAIL\n");
            return DEC_MEMFAIL;
        }
    }
    //add for fbm
   #if 0
    //if(dec_cont->pic_number >= 1 && dec_cont->pic_number <= 5)
     {
        logd("raster!!!!\n");
        struct Vp9Decoder* dec = NULL;
        dec = (struct Vp9Decoder*)&dec_cont->decoder;
        int width = dec->width;
        int height = dec->height;
        int w = ((width + 31) & ~31) ;
        int h = ((height + 31) & ~31);
        int size = w*h*3;
        asic_buff->raster_luma[asic_buff->out_buffer_i].virtual_address
            =  (u32*)CdcMemPalloc(_memops, size);
        if(asic_buff->raster_luma[asic_buff->out_buffer_i].virtual_address == NULL)
         {
            loge("malloc memory failed\n");
            return DWL_ERROR;
         }
        asic_buff->raster_chroma[asic_buff->out_buffer_i].virtual_address = w*h*2 +
             asic_buff->raster_luma[asic_buff->out_buffer_i].virtual_address;

        asic_buff->raster_luma[asic_buff->out_buffer_i].bus_address =
            (u32)CdcMemGetPhysicAddress(_memops,
                 asic_buff->raster_luma[asic_buff->out_buffer_i].virtual_address);

        asic_buff->raster_chroma[asic_buff->out_buffer_i].bus_address = w*h*2 +
                asic_buff->raster_luma[asic_buff->out_buffer_i].bus_address;
        asic_buff->raster_luma[asic_buff->out_buffer_i].size = w*h*2;
        asic_buff->raster_chroma[asic_buff->out_buffer_i].size = w*h;
    }
    #else
    {
        raster_luma_size = (u32)( pVp9HwDec->pCurFrm->pData1- pVp9HwDec->pCurFrm->pData0);
        asic_buff->raster_luma[asic_buff->out_buffer_i].virtual_address
            = (u32 *)( pVp9HwDec->pCurFrm->pData0);
        asic_buff->raster_chroma[asic_buff->out_buffer_i].virtual_address
            = (u32 *)( pVp9HwDec->pCurFrm->pData1);

        asic_buff->raster_luma[asic_buff->out_buffer_i].bus_address
            =  pVp9HwDec->pCurFrm->phyYBufAddr;
        asic_buff->raster_chroma[asic_buff->out_buffer_i].bus_address
            = raster_luma_size+ pVp9HwDec->pCurFrm->phyYBufAddr;
    }
    #endif

    if(Vp9ReallocateSegmentMap(dec_cont))
    {
        loge(" faile Vp9ReallocateSegmentMap error: return DEC_MEMFAIL\n");
        return DEC_MEMFAIL;
    }
    return HANTRO_OK;
}

/* Reallocates segment maps if resolution changes bigger than initial
   resolution. Needs synchronization if SW is running parallel with HW */
s32 Vp9ReallocateSegmentMap(struct Vp9DecContainer *dec_cont)
{
    s32 dwl_ret;
    u32 num_ctbs, memory_size;
    const void *dwl = dec_cont->dwl;
    struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
    struct DWLLinearMem segment_map[2];
    struct Vp9Decoder *dec = &dec_cont->decoder;
    u32 i = 0;

    num_ctbs = ((asic_buff->width + 63) / 64) * ((asic_buff->height + 63) / 64);
    memory_size = num_ctbs * 32; /* Segment map uses 32 bytes / CTB */
    /* Do nothing if we have big enough buffers for segment maps */

    if(dec->key_frame || dec->resolution_change)
    {
        for(i = 0; i < 2; i++)
        {
            struct DWLLinearMem mem = asic_buff->segment_map[i];
            if(mem.virtual_address)
            {
                CdcMemSet(dec_cont->memops,mem.virtual_address, 0, mem.size);
                CdcMemFlushCache(dec_cont->memops,mem.virtual_address, mem.size);
            }
        }
    }
    if(memory_size <= asic_buff->segment_map_size)
    {
        logi("*******not need Allocate new segment maps\n");
        return HANTRO_OK;
    }

    /* Allocate new segment maps for larger resolution */
    dwl_ret = DWLMallocLinear(dwl, memory_size, &segment_map[0], dec_cont->memops,
                                                            (void *)dec_cont->veOpsS,
                                                            dec_cont->pVeOpsSelf);
    dwl_ret |= DWLMallocLinear(dwl, memory_size, &segment_map[1], dec_cont->memops,
                                                            (void *)dec_cont->veOpsS,
                                                            dec_cont->pVeOpsSelf);
    if(dwl_ret != DWL_OK)
    {
        loge("**********faile:Allocate new segment maps for larger resolution\n");
        Vp9AsicReleasePictures(dec_cont);
        return HANTRO_NOK;
    }

    #if 0
    DWLmemset(segment_map[0].virtual_address, 0, memory_size);
    DWLmemset(segment_map[1].virtual_address, 0, memory_size);

    /* Copy existing segment maps into new buffers */
    DWLmemcpy(segment_map[0].virtual_address,
            asic_buff->segment_map[0].virtual_address,
            asic_buff->segment_map[0].size);
    DWLmemcpy(segment_map[1].virtual_address,
            asic_buff->segment_map[1].virtual_address,
            asic_buff->segment_map[1].size);
    #else
    //CdcMemSet(dec_cont->memops, segment_map[0].virtual_address, 0, memory_size);
    //CdcMemSet(dec_cont->memops, segment_map[1].virtual_address, 0, memory_size);

    /* Copy existing segment maps into new buffers */
    CdcMemCopy(dec_cont->memops, segment_map[0].virtual_address,
            asic_buff->segment_map[0].virtual_address,
            asic_buff->segment_map[0].size);
    CdcMemCopy(dec_cont->memops, segment_map[1].virtual_address,
            asic_buff->segment_map[1].virtual_address,
            asic_buff->segment_map[1].size);
    #endif
    /* Free old segment maps */
    Vp9FreeSegmentMap(dec_cont);
    asic_buff->segment_map_size = memory_size;
    asic_buff->segment_map[0] = segment_map[0];
    asic_buff->segment_map[1] = segment_map[1];
    return HANTRO_OK;
}

void Vp9AsicReleaseFilterBlockMem(struct Vp9DecContainer *dec_cont)
{
    struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

    if(asic_buff->filter_mem.virtual_address != NULL)
    {
        DWLFreeLinear(dec_cont->dwl, &asic_buff->filter_mem, dec_cont->memops,
                                                            dec_cont->veOpsS,
                                                            dec_cont->pVeOpsSelf);
        asic_buff->filter_mem.virtual_address = NULL;
        asic_buff->filter_mem.size = 0;
    }
    if(asic_buff->bsd_control_mem.virtual_address != NULL)
    {
        DWLFreeLinear(dec_cont->dwl, &asic_buff->bsd_control_mem, dec_cont->memops,
                                                            dec_cont->veOpsS,
                                                            dec_cont->pVeOpsSelf);
        asic_buff->bsd_control_mem.virtual_address = NULL;
        asic_buff->bsd_control_mem.size = 0;
    }
}

/* Allocate filter memories that are dependent on tile structure and height. */
s32 Vp9AsicAllocateFilterBlockMem(struct Vp9DecContainer *dec_cont)
{
    struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
    u32 num_tile_cols = 1 << dec_cont->decoder.log2_tile_columns;

    if(num_tile_cols < 2)
    {
        return HANTRO_OK;
    }

    u32 height32 = NEXT_MULTIPLE(asic_buff->height, 64);
    u32 size = 8 * height32 * (num_tile_cols - 1);    /* Luma filter mem */
    size += (8 + 8) * height32 * (num_tile_cols - 1); /* Chroma */
    size *= 2;  /* 16 bits per pixel */
    if(asic_buff->filter_mem.logical_size >= size)
    {
        return HANTRO_OK;
    }

    /* If already allocated, release the old, too small buffers. */

    Vp9AsicReleaseFilterBlockMem(dec_cont);
    s32 dwl_ret = DWLMallocLinear(dec_cont->dwl, size, &asic_buff->filter_mem, dec_cont->memops,
                                                            (void *)dec_cont->veOpsS,
                                                            dec_cont->pVeOpsSelf);
    if(dwl_ret != DWL_OK)
    {
        Vp9AsicReleaseFilterBlockMem(dec_cont);
        return HANTRO_NOK;
    }

    size = 16 * (height32 / 4) * (num_tile_cols - 1);
    dwl_ret = DWLMallocLinear(dec_cont->dwl, size, &asic_buff->bsd_control_mem, dec_cont->memops,
                                                            (void *)dec_cont->veOpsS,
                                                            dec_cont->pVeOpsSelf);
    if(dwl_ret != DWL_OK)
    {
        Vp9AsicReleaseFilterBlockMem(dec_cont);
        return HANTRO_NOK;
    }

    return HANTRO_OK;
}

u32 Vp9AsicRun(struct Vp9DecContainer *dec_cont)
{
    s32 ret = 0;
    //s32 i = 0;
    //struct DecAsicBuffers* pAnciBuffer = NULL;
    //pAnciBuffer = (struct DecAsicBuffers*)dec_cont->asic_buff;

    #if 1
    //u32 index_ref[VP9_ACTIVE_REFS];
    //FILE* fp = NULL;
    #endif

    if(!dec_cont->asic_running)
    {
        #if 0
        ret = DWLReserveHw(dec_cont->dwl, &dec_cont->core_id);
        logd("ret=%d, %d\n", ret, __LINE__);
        if(ret != DWL_OK)
        {
            return VP9HWDEC_HW_RESERVED;
        }
        #else
        ret = DWL_OK;
        #endif
        dec_cont->asic_running = 1;
        FlushDecRegisters(dec_cont->dwl, dec_cont->core_id, dec_cont->vp9_regs);
        SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_E, 1);
        #if 0
        DWLEnableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1, dec_cont->vp9_regs[1]);
        #else
        #if 0
        for(i = 0; i < VP9_ACTIVE_REFS; i++)
            {
            index_ref[i] = Vp9BufferQueueGetRef(dec_cont->bq, dec_cont->decoder.active_ref_idx[i]);
        }
        logd("**********here1:new ref address:%x\n",
             (unsigned int)pAnciBuffer->pictures[index_ref[0]].virtual_address);
        fp = fopen("/tmp/ref_frame_y_new.dat", "wb");
        if(fp != NULL)
        {
            fwrite(pAnciBuffer->pictures[index_ref[0]].virtual_address, 1,
                pAnciBuffer->pictures[index_ref[0]].size, fp);
            fclose(fp);
        }

        fp = fopen("/tmp/ref_frame_c_new.dat", "wb");
        if(fp != NULL)
        {
            fwrite(pAnciBuffer->pictures_c[index_ref[0]].virtual_address, 1,
                pAnciBuffer->pictures_c[index_ref[0]].size, fp);
            fclose(fp);
        }
        #endif

        #if 0
            logd("**************print the VP9 register\n");
            unsigned int * nRegisterBaseAddr = NULL;
            int i = 0;
            nRegisterBaseAddr = (unsigned int*)ve_get_reglist(REG_GROUP_VETOP);
            for(i=0; i<184; i++)
            {
               logd("read reg, i = %d, value = 0x%08x",i,*(nRegisterBaseAddr+i));
            }
        #endif
        #if 1
        CdcMemFlushCache(dec_cont->memops,
              dec_cont->asic_buff->ctx_counters[dec_cont->asic_buff->ctx_conter_index].virtual_address,
              sizeof(struct Vp9EntropyCounts));
        #endif
        DWLEnableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1, 0x01);
        #endif

    }
    else
    {
        DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 13,
                dec_cont->vp9_regs[13]);
        DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 14,
                dec_cont->vp9_regs[14]);
        DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 15,
                dec_cont->vp9_regs[15]);
        #if 0
        for(i = 0; i < VP9_ACTIVE_REFS; i++)
            {
            index_ref[i] = Vp9BufferQueueGetRef(dec_cont->bq, dec_cont->decoder.active_ref_idx[i]);
        }
        logd("**********here2:new ref address:%x\n",
             (unsigned int)pAnciBuffer->pictures[index_ref[0]].virtual_address);
        fp = fopen("/tmp/ref_frame_y_new.dat", "wb");
        if(fp != NULL)
        {
            fwrite(pAnciBuffer->pictures[index_ref[0]].virtual_address, 1,
                pAnciBuffer->pictures[index_ref[0]].size, fp);
            fclose(fp);
        }

        fp = fopen("/tmp/ref_frame_c_new.dat", "wb");
        if(fp != NULL)
        {
            fwrite(pAnciBuffer->pictures_c[index_ref[0]].virtual_address, 1,
                pAnciBuffer->pictures_c[index_ref[0]].size, fp);
            fclose(fp);
        }
        #endif
        #if 1
        CdcMemFlushCache(dec_cont->memops,
                dec_cont->asic_buff->ctx_counters[dec_cont->asic_buff->ctx_conter_index].virtual_address,
                sizeof(struct Vp9EntropyCounts));
        #endif
        DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 1, 0x11);
    }
    #if 1
    Vp9SetupPicToOutput(dec_cont);
    #endif
    return ret;
}

#define DEC_MODE_VP9 13

void Vp9AsicInit(struct Vp9DecContainer *dec_cont)
{
    DWLmemset(dec_cont->vp9_regs, 0, sizeof(dec_cont->vp9_regs));
    SetDecRegister(dec_cont->vp9_regs, HWIF_DEC_MODE, DEC_MODE_VP9);
    SetCommonConfigRegs(dec_cont->vp9_regs);
}

void Vp9AsicReleaseMem(struct Vp9DecContainer *dec_cont)
{
    const void *dwl = dec_cont->dwl;
    struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
    int i = 0;

    for(i=0; i<5; i++)
    {
        if(asic_buff->ctx_counters[i].virtual_address != NULL)
        {
            DWLFreeLinear(dwl, &asic_buff->ctx_counters[i], dec_cont->memops,
                                                            dec_cont->veOpsS,
                                                            dec_cont->pVeOpsSelf);
            //DWLmemset(&asic_buff->ctx_counters[i], 0, sizeof(struct Vp9EntropyCounts));
            asic_buff->ctx_counters[i].bus_address = 0;
            asic_buff->ctx_counters[i].logical_size = 0;
            asic_buff->ctx_counters[i].size = 0;
            asic_buff->ctx_counters[i].virtual_address = NULL;
        }
    }

    if(asic_buff->prob_tbl.virtual_address != NULL)
    {
        DWLFreeLinear(dwl, &asic_buff->prob_tbl, dec_cont->memops,
                                                            dec_cont->veOpsS,
                                                            dec_cont->pVeOpsSelf);
        //DWLmemset(&asic_buff->prob_tbl, 0, sizeof(asic_buff->prob_tbl));
        asic_buff->prob_tbl.bus_address = 0;
        asic_buff->prob_tbl.virtual_address = NULL;
        asic_buff->prob_tbl.size = 0;
        asic_buff->prob_tbl.logical_size = 0;
    }



    if(asic_buff->tile_info.virtual_address != NULL)
    {
        DWLFreeLinear(dwl, &asic_buff->tile_info, dec_cont->memops,
                                                            dec_cont->veOpsS,
                                                            dec_cont->pVeOpsSelf);
        asic_buff->tile_info.virtual_address = NULL;
    }
}

s32 Vp9AsicAllocateMem(struct Vp9DecContainer *dec_cont)
{
    const void *dwl = dec_cont->dwl;
    s32 dwl_ret;
    u32 size;
    int i = 0;
    struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

    size = sizeof(struct Vp9EntropyProbs);
    dwl_ret = DWLMallocLinear(dwl, size, &asic_buff->prob_tbl, dec_cont->memops,
                                                            (void *)dec_cont->veOpsS,
                                                            dec_cont->pVeOpsSelf);
    if(dwl_ret != DWL_OK)
    {
        goto err;
    }

    size = sizeof(struct Vp9EntropyCounts);

    for(i=0; i<5; i++)
    {
        dwl_ret = DWLMallocLinear(dwl, size, &asic_buff->ctx_counters[i],dec_cont->memops,
                                                            (void *)dec_cont->veOpsS,
                                                            dec_cont->pVeOpsSelf);
        if(dwl_ret != DWL_OK)
        {
            goto err;
        }
    }

    /* max number of tiles times width and height (2 bytes each),
    * rounding up to next 16 bytes boundary + one extra 16 byte
    * chunk (HW guys wanted to have this) */

    size = (MAX_TILE_COLS * MAX_TILE_ROWS * 2 * sizeof(u16) + 15 + 16) & ~0xF;
    dwl_ret = DWLMallocLinear(dwl, size, &asic_buff->tile_info, dec_cont->memops,
                                                            (void *)dec_cont->veOpsS,
                                                            dec_cont->pVeOpsSelf);
    if(dwl_ret != DWL_OK)
    {
        goto err;
    }
    DWLmemset(asic_buff->tile_info.virtual_address, 0, asic_buff->tile_info.size);
    return 0;
err:
    Vp9AsicReleaseMem(dec_cont);
    return -1;
}

