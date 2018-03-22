/* Copyright 2013 Google Inc. All Rights Reserved. */

/* asic interface */

#ifndef VP9HWD_CONTAINER_H
#define VP9HWD_CONTAINER_H

#include "vp9hwd_dwl.h"
#include "vp9hwd_decoder.h"
#include "vp9hwd_bool.h"
#include "vp9hwd_buffer_queue.h"
#include "vp9hwd_fifo.h"
#include "vp9hwd_decapicommon.h"

#define VP9DEC_UNINITIALIZED 0U
#define VP9DEC_INITIALIZED 1U
#define VP9DEC_NEW_HEADERS 3U
#define VP9DEC_DECODING 4U
#define VP9DEC_END_OF_STREAM 5U

//#define VP9DEC_MAX_PIC_BUFFERS 16
#define VP9DEC_MAX_PIC_BUFFERS 16
#define VP9DEC_DYNAMIC_PIC_LIMIT 10

#define VP9_UNDEFINED_BUFFER (s32)(-1)

/* Output picture format types */
enum DecPictureFormat {
  DEC_OUT_FRM_TILED_4X4 = 0,
  DEC_OUT_FRM_RASTER_SCAN = 1, /* a.k.a. SEMIPLANAR_420 */
  DEC_OUT_FRM_PLANAR_420 = 2
};

/* Output structure for Vp9DecNextPicture */
struct Vp9DecPicture {
  u32 coded_width;  /* coded width of the picture */
  u32 coded_height; /* coded height of the picture */
  u32 frame_width;  /* pixels width of the frame as stored in memory */
  u32 frame_height; /* pixel height of the frame as stored in memory */
  const u32 *output_luma_base;   /* Pointer to the picture */
  u32 output_luma_bus_address;   /* Bus address of the luminance component */
  const u32 *output_chroma_base; /* Pointer to the picture */
  u32 output_chroma_bus_address; /* Bus address of the chrominance component */
  u32 pic_id;                    /* Identifier of the Frame to be displayed */
  u32 is_intra_frame;            /* Indicates if Frame is an Intra Frame */
  u32 is_golden_frame; /* Indicates if Frame is a Golden reference Frame */
  u32 nbr_of_err_mbs;  /* Number of concealed MB's in the frame  */
  u32 num_slice_rows;
  u32 cycles_per_mb;   /* Avarage cycle count per macroblock */
  enum DecPictureFormat output_format;
  u32 bits_per_sample;
  VideoPicture* dispFrame;
};

struct PicCallbackArg
{
    u32 display_number;
    s32 index; /* Buffer index of the output buffer. */
    s32 index_a;
    s32 index_g;
    s32 index_p;
    u32 show_frame;
    FifoInst fifo_out;        /* Output FIFO instance. */
    struct Vp9DecPicture pic; /* Information needed for outputting the frame. */
};

struct DecAsicBuffers
{
    u32 width, height;
    struct DWLLinearMem ctx_counters[5];
    struct DWLLinearMem prob_tbl;
    struct DWLLinearMem segment_map[2];
    struct DWLLinearMem tile_info;
    struct DWLLinearMem filter_mem;
    struct DWLLinearMem bsd_control_mem;
    u32 display_index[VP9DEC_MAX_PIC_BUFFERS];
    /* Concurrent access to following picture arrays is controlled indirectly
    * through buffer queue. */
    struct DWLLinearMem pictures[VP9DEC_MAX_PIC_BUFFERS];
    struct DWLLinearMem pictures_c[VP9DEC_MAX_PIC_BUFFERS];
    struct DWLLinearMem dir_mvs[VP9DEC_MAX_PIC_BUFFERS]; /* colocated MVs */
    struct DWLLinearMem raster_luma[VP9DEC_MAX_PIC_BUFFERS];
    struct DWLLinearMem raster_chroma[VP9DEC_MAX_PIC_BUFFERS];
    struct Vp9DecPicture picture_info[VP9DEC_MAX_PIC_BUFFERS];
    s32 reference_list[VP9_REF_LIST_SIZE]; /* Contains indexes to full list of
                                            picture */
                                            /* Indexes for picture buffers in pictures[] array */
    s32 out_buffer_i;
    s32 prev_out_buffer_i;
    u32 whole_pic_concealed;
    u32 disable_out_writing;
    u32 segment_map_size;
    u32 partition1_base;
    u32 partition1_bit_offset;
    u32 partition2_base;
    u32 ctx_conter_index;
};

/* Post-processor features. */
struct Vp9PpConfig{
  enum DecBitDepth {
    DEC_BITDEPTH_INITIAL = 0, /* Match the bit depth of the first frame over
                                 the whole sequence. */
    DEC_BITDEPTH_NATIVE = 1, /* Match the bit depth of the input stream. */
    DEC_BITDEPTH_8 = 8,      /* If over 8 bit deep input, downsample to 8. */
    DEC_BITDEPTH_10 = 10 /* If over 10 bit deep input, downsample to 10. */
  } bit_depth;           /* Bit depth for post-processed picture. */
  enum DecBitPacking {
    DEC_BITPACKING_LSB = 0, /* Pack the pixel into the LSBs of each sample. */
    DEC_BITPACKING_MSB
  } bit_packing;
};

struct Vp9DecContainer
{
  const void *checksum;
  u32 dec_mode;
  u32 dec_stat;
  u32 pic_number;
  u32 asic_running;
  u32 width;
  u32 height;
  u32 vp9_regs[DEC_X170_REGISTERS];
  struct DecAsicBuffers asic_buff[1];
  const void *dwl; /* struct DWL instance */
  s32 core_id;

  struct Vp9Decoder decoder;
  struct VpBoolCoder bc;

  u32 picture_broken;
  u32 intra_freeze;
  u32 out_count;
  u32 num_buffers;
  u32 active_segment_map;

  BufferQueue bq;

  u32 intra_only;
  u32 conceal;
  u32 prev_is_key;
  u32 force_intra_freeze;
  u32 prob_refresh_detected;
  struct PicCallbackArg pic_callback_arg[MAX_ASIC_CORES];
  /* Output related variables. */
  FifoInst fifo_out;     /* Fifo for output indices. */
  FifoInst fifo_display; /* Store of output indices for display reordering. */
  u32 display_number;
  //pthread_mutex_t sync_out; /* protects access to pictures in output fifo. */
  //pthread_cond_t sync_out_cv;

  u32 t_fifo_out_disp_num[VP9DEC_MAX_PIC_BUFFERS];
  u32 t_fifo_out_disp_wr;
  u32 t_fifo_out_disp_rd;

  DWLHwConfig hw_cfg;

  enum DecPictureFormat output_format;
  u32 dynamic_buffer_limit; /* limit for dynamic frame buffer count */
  //struct Vp9PpConfig pp;
  u8 pack_pixel_to_msbs;
  u8 sbm_back;

  u32 initial_bitdepth;
  struct ScMemOpsS *memops;
  VeOpsS*           veOpsS;
  void*             pVeOpsSelf;
  s32      bNeedConvert10bitTo8bit;
};

#endif

