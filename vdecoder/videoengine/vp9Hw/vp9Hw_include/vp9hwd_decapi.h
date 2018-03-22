/* Copyright 2012 Google Inc. All Rights Reserved. */

#ifndef VP9DECAPI_H
#define VP9DECAPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "vp9hwd_decapicommon.h"
#include "vp9hwd_dectypes.h"

/*------------------------------------------------------------------------------
    API type definitions
------------------------------------------------------------------------------*/

/* Decoder instance */
typedef const void *Vp9DecInst;

/* Input structure */
struct Vp9DecInput {
  u8 *stream;             /* Pointer to the input */
  u32 stream_bus_address; /* DMA bus address of the input stream */
  u32 data_len;           /* Number of bytes to be decoded         */
  u32 pic_id;             /* Identifier for the picture to be decoded */
};

/* Output structure */
struct Vp9DecOutput {
  u8 *strm_curr_pos; /* Pointer to stream position where decoding ended */
  u32 strm_curr_bus_address; /* DMA bus address location where the decoding
                                ended */
  u32 data_left; /* how many bytes left undecoded */
};

/* stream info filled by Vp9DecGetInfo */
struct Vp9DecInfo {
  u32 vp_version;
  u32 vp_profile;
  u32 bit_depth;
  u32 coded_width;   /* coded width */
  u32 coded_height;  /* coded height */
  u32 frame_width;   /* pixels width of the frame as stored in memory */
  u32 frame_height;  /* pixel height of the frame as stored in memory */
  u32 scaled_width;  /* scaled width of the displayed video */
  u32 scaled_height; /* scaled height of the displayed video */
  u32 dpb_mode;      /* DPB mode; frame, or field interlaced */
  enum DecPictureFormat output_format; /* format of the output picture */
  u32 pic_buff_size; /* number of picture buffers allocated&used by decoder */
  u32 multi_buff_pp_size; /* number of picture buffers needed in
                             decoder+postprocessor multibuffer mode */
};

typedef struct DecSwHwBuild Vp9DecBuild;

#if 0

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
};
#endif

/*------------------------------------------------------------------------------
    Prototypes of Decoder API functions
------------------------------------------------------------------------------*/

Vp9DecBuild Vp9DecGetBuild(void);

enum DecRet Vp9DecInit(Vp9DecInst *dec_inst, u32 use_video_freeze_concealment,
                       u32 num_frame_buffers, enum DecDpbFlags dpb_flags,
                       enum DecPictureFormat output_format,
                       struct Vp9PpConfig pp_cfg);

void Vp9DecRelease(Vp9DecInst dec_inst);

enum DecRet Vp9DecDecode(Vp9DecInst dec_inst, const struct Vp9DecInput *input,
                         struct Vp9DecOutput *output);

enum DecRet Vp9DecNextPicture(Vp9DecInst dec_inst,
                              struct Vp9DecPicture *output);

enum DecRet Vp9DecPictureConsumed(Vp9DecInst dec_inst,
                                  const struct Vp9DecPicture *picture);

enum DecRet Vp9DecEndOfStream(Vp9DecInst dec_inst);

enum DecRet Vp9DecGetInfo(Vp9DecInst dec_inst, struct Vp9DecInfo *dec_info);

#ifdef __cplusplus
}
#endif

#endif /* VP9DECAPI_H */
