/* Copyright 2013 Google Inc. All Rights Reserved. */

/* TODO(mheikkinen) do pthread wrapper. */

#include "vp9hwd_container.h"
#include "vp9hwd_regdrv.h"
#include "vp9hwd_fifo.h"
#include "vp9hwd_dectypes.h"
#include "vp9hwd_asic.h"

extern u32 GetDecRegister(const u32* reg_base, u32 id);

#if 0
u32 CycleCount(struct Vp9DecContainer *dec_cont)
{
    u32 cycles = 0;
    #define NEXT_MULTIPLE(value, n) (((value) + (n) - 1) & ~((n) - 1))
    u32 mbs = (NEXT_MULTIPLE(dec_cont->height, 16) *
                NEXT_MULTIPLE(dec_cont->width, 16)) >> 8;

    if (mbs)
        cycles = GetDecRegister(dec_cont->vp9_regs, HWIF_PERF_CYCLE_COUNT) / mbs;
    //DEBUG_PRINT(("Pic %3d cycles/mb %4d\n", dec_cont->pic_number, cycles));
    return cycles;
}

extern void printf_ref_status(BufferQueue bq, int index);
#endif

void Vp9PicToOutput(struct Vp9DecContainer *dec_cont, Fbm* curFbm, VideoPicture* curFrame)
{
    struct PicCallbackArg info = dec_cont->pic_callback_arg[0];
    //struct BQueue* q = NULL;
     //int i = 0;
     //BufferQueue  bq = NULL;

    //info.pic.cycles_per_mb = CycleCount(dec_cont);
    dec_cont->asic_buff->picture_info[info.index] = info.pic;
    if(info.show_frame)
    {
        dec_cont->asic_buff->display_index[info.index] = dec_cont->display_number++;
        dec_cont->t_fifo_out_disp_num[dec_cont->t_fifo_out_disp_wr] =
        dec_cont->display_number - 1;

        #if 0
        dec_cont->t_fifo_out_disp_wr =
        (dec_cont->t_fifo_out_disp_wr + 1) % VP9DEC_MAX_PIC_BUFFERS;
        #else
        dec_cont->t_fifo_out_disp_wr =
        (dec_cont->t_fifo_out_disp_wr + 1) % dec_cont->num_buffers;
        #endif
        FifoPush(dec_cont->fifo_out, (void *)(uintptr_t)info.index, FIFO_EXCEPTION_DISABLE);
        FbmReturnBuffer(curFbm, curFrame, 1);
        Vp9BufferQueueRemoveRef(dec_cont->bq, info.index);
    }
    else if(curFrame != NULL)
    {
        FbmReturnBuffer(curFbm, curFrame, 0);
    }
}

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define NEXT_MULTIPLE(value, n) (((value) + (n) - 1) & ~((n) - 1))

void Vp9SetupPicToOutput(struct Vp9DecContainer *dec_cont)
{
    struct PicCallbackArg *args = &dec_cont->pic_callback_arg[dec_cont->core_id];

    args->index = dec_cont->asic_buff->out_buffer_i;
    args->fifo_out = dec_cont->fifo_out;

    if(dec_cont->decoder.show_existing_frame)
    {
        logd("*****dec_cont->decoder.show_existing_frame=1\n");
        args->pic = dec_cont->asic_buff->picture_info[args->index];
        args->pic.is_intra_frame = 0;
        args->show_frame = 1;
        return;
    }
    args->show_frame = dec_cont->decoder.show_frame;

    /* Fill in the picture information for everything we know. */
    args->pic.is_intra_frame = dec_cont->decoder.key_frame;
    args->pic.is_golden_frame = 0;
    /* Frame size and format information. */
    args->pic.frame_width = NEXT_MULTIPLE(dec_cont->width, 8);
    args->pic.frame_height = NEXT_MULTIPLE(dec_cont->height, 8);
    args->pic.coded_width = dec_cont->width;
    args->pic.coded_height = dec_cont->height;
    args->pic.output_format = dec_cont->output_format;

    args->pic.bits_per_sample = dec_cont->decoder.bit_depth;
#if 0

    if(dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN)
    {
        args->pic.frame_width = NEXT_MULTIPLE(dec_cont->width, 32);
        args->pic.output_luma_base =
            dec_cont->asic_buff->raster_luma[args->index].virtual_address;
        args->pic.output_luma_bus_address =
            dec_cont->asic_buff->raster_luma[args->index].bus_address;
        args->pic.output_chroma_base =
            dec_cont->asic_buff->raster_chroma[args->index].virtual_address;
        args->pic.output_chroma_bus_address =
            dec_cont->asic_buff->raster_chroma[args->index].bus_address;
    }
    else
    {
        args->pic.output_luma_base =
            dec_cont->asic_buff->pictures[args->index].virtual_address;
        args->pic.output_luma_bus_address =
            dec_cont->asic_buff->pictures[args->index].bus_address;
        args->pic.output_chroma_base =
            dec_cont->asic_buff->pictures_c[args->index].virtual_address;
        args->pic.output_chroma_bus_address =
            dec_cont->asic_buff->pictures_c[args->index].bus_address;
    }
    #endif

    /* Finally, set the information we don't know yet to 0. */
    args->pic.nbr_of_err_mbs = 0; /* To be set after decoding. */
    args->pic.pic_id = 0;         /* To be set after output reordering. */
}

s32 Vp9ProcessAsicStatus(struct Vp9DecContainer *dec_cont, u32 asic_status,
                         u32 *error_concealment)
{
    /* Handle system error situations */
    if(asic_status == VP9HWDEC_SYSTEM_TIMEOUT)
    {
        /* This timeout is DWL(software/os) generated */
        return DEC_HW_TIMEOUT;
    }
    else if(asic_status == VP9HWDEC_SYSTEM_ERROR)
    {
        return DEC_SYSTEM_ERROR;
    }
    else if(asic_status == VP9HWDEC_HW_RESERVED)
    {
        return DEC_HW_RESERVED;
    }

    /* Handle possible common HW error situations */
    if(asic_status & DEC_HW_IRQ_BUS)
    {
        return DEC_HW_BUS_ERROR;
    }

    /* for all the rest we will output a picture (concealed or not) */
    if((asic_status & DEC_HW_IRQ_TIMEOUT) || (asic_status & DEC_HW_IRQ_ERROR) ||
      (asic_status & DEC_HW_IRQ_ASO) /* to signal lost residual */)
    {
        /* This timeout is HW generated */
        if(asic_status & DEC_HW_IRQ_TIMEOUT)
        {
#ifdef VP9HWTIMEOUT_ASSERT
            ASSERT(0);
#endif
            loge("IRQ: HW TIMEOUT\n");
        }
        else
        {
            loge("IRQ: STREAM ERROR\n");
        }

        /* normal picture freeze */
        *error_concealment = 1;
    }
    else if(asic_status & DEC_HW_IRQ_RDY)
    {
        //loge("IRQ: PICTURE RDY\n");
        if(dec_cont->decoder.key_frame)
        {
            dec_cont->picture_broken = 0;
            dec_cont->force_intra_freeze = 0;
        }
    }
    else
    {
        //ASSERT(0);
    }
    return DEC_OK;
}

#if 0
void Vp9ConstantConcealment(struct Vp9DecContainer *dec_cont, u8 pixel_value)
{
    struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
    s32 index = asic_buff->out_buffer_i;

    dec_cont->picture_broken = 1;

    CdcMemSet(dec_cont->memops,asic_buff->pictures[index].virtual_address, pixel_value,
            asic_buff->pictures[index].size);
    CdcMemSet(dec_cont->memops, asic_buff->pictures_c[index].virtual_address, pixel_value,
            asic_buff->pictures_c[index].size);
    CdcMemFlushCache(dec_cont->memops,asic_buff->pictures[index].virtual_address,
            asic_buff->pictures[index].size);
    CdcMemFlushCache(dec_cont->memops, asic_buff->pictures_c[index].virtual_address,
           asic_buff->pictures_c[index].size);

    if(dec_cont->output_format == DEC_OUT_FRM_RASTER_SCAN)
    {
        CdcMemSet(dec_cont->memops, asic_buff->raster_luma[index].virtual_address, pixel_value,
                asic_buff->raster_luma[index].size);
        CdcMemSet(dec_cont->memops, asic_buff->raster_chroma[index].virtual_address, pixel_value,
                asic_buff->raster_chroma[index].size);
        CdcMemFlushCache(dec_cont->memops, asic_buff->raster_luma[index].virtual_address,
                asic_buff->raster_luma[index].size);
        CdcMemFlushCache(dec_cont->memops, asic_buff->raster_chroma[index].virtual_address,
                asic_buff->raster_chroma[index].size);
    }
}
#endif

s32 VP9SyncAndOutput(struct Vp9DecContainer *dec_cont, Fbm* curFbm, VideoPicture* curFrame)
{
    s32 ret = 0;
    u32 asic_status;
    u32 error_concealment = 0;

    /* aliases */
    struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

    /* If hw was running, sync with hw and output picture */
    if(dec_cont->asic_running)
    {
        asic_status = Vp9AsicSync(dec_cont);
        /* Handle asic return status */
        ret = Vp9ProcessAsicStatus(dec_cont, asic_status, &error_concealment);
        if(ret)
        {
            return ret;
        }

        /* Adapt probabilities */
        /* TODO should this be done after error handling? */
        Vp9UpdateProbabilities(dec_cont);
        /* Update reference frame flags */

        Vp9UpdateRefs(dec_cont, error_concealment);
        /* Store prev out info */
        if(!error_concealment || dec_cont->intra_only || dec_cont->pic_number==1)
        {
            if(error_concealment)
            {
                //Vp9ConstantConcealment(dec_cont, 128);
            }
            asic_buff->prev_out_buffer_i = asic_buff->out_buffer_i;
            Vp9PicToOutput(dec_cont, curFbm, curFrame);
        }
        else
        {
            dec_cont->picture_broken = 1;
        }
        asic_buff->out_buffer_i = VP9_UNDEFINED_BUFFER;

    }
    return ret;
}

