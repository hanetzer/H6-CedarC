/* Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Stream buffer handling functionality. */

#ifndef SW_STREAM_H_
#define SW_STREAM_H_

//#include "basetype.h"
#include "vp9hwd_decoder.h"

/* value to be returned by GetBits if stream buffer is empty */
#define END_OF_STREAM 0xFFFFFFFFU

#define HANTRO_OK 0
#define HANTRO_NOK 1

#define HANTRO_FALSE (0U)
#define HANTRO_TRUE (1U)

typedef struct StrmData
{
    const u8 *strm_buff_start; /* pointer to start of stream buffer */
    const u8 *strm_curr_pos;   /* current read address in stream buffer */
    u32 bit_pos_in_word;       /* bit position in stream buffer byte */
    u32 strm_buff_size;        /* size of stream buffer (bytes) */
    u32 strm_buff_read_bits;   /* number of bits read from stream buffer */
    u32 remove_emul3_byte;     /* signal the pre-removal of emulation prevention 3
                    bytes */
    u32 emul_byte_count; /* counter incremented for each removed byte */
}stream_data;

u32 SwGetBits(struct StrmData *stream, u32 num_bits);
u32 SwGetBitsUnsignedMax(struct StrmData *stream, u32 max_value);
u32 SwShowBits(const struct StrmData *stream, u32 num_bits);
u32 SwFlushBits(struct StrmData *stream, u32 num_bits);
u32 SwIsByteAligned(const struct StrmData *);

#endif /* #ifdef SW_STREAM_H_ */
