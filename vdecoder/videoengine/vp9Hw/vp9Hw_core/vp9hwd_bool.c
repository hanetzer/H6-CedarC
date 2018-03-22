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
#include "vp9hwd_bool.h"

u32 Vp9ReadBits(struct VpBoolCoder* br, s32 bits)
{
    u32 z = 0;
    s32 bit;

    for(bit = bits - 1; bit >= 0; bit--)
    {
        z |= (Vp9DecodeBool128(br) << bit);
    }
    return z;
}

u32 Vp9DecodeBool(struct VpBoolCoder* br, s32 probability)
{
    u32 bit = 0;
    u32 split;
    u32 bigsplit;
    u32 count = br->count;
    u32 range = br->range;
    u32 value = br->value;
    u32 value_tmp = value;
    u32 range_tmp = range;
    (void)value_tmp;
    (void)range_tmp;

    split = 1 + (((range - 1) * probability) >> 8);
    bigsplit = (split << 24);

    range = split;
    if(value >= bigsplit)
    {
        range = br->range - split;
        value = value - bigsplit;
        bit = 1;
    }

    if(range >= 0x80)
    {
        br->value = value;
        br->range = range;
        return bit;
    }
    else
    {
        do
        {
            range += range;
            value += value;
            if(!--count)
            {
                /* no more stream to read? */
                if(br->pos >= br->stream_end_pos)
                {
                    br->strm_error = 1;
                    break;
                }
                count = 8;
                value |= br->buffer[br->pos];
                br->pos++;
            }
        }while (range < 0x80);
    }

    br->count = count;
    br->value = value;
    br->range = range;
    return bit;
}

u32 Vp9DecodeBool128(struct VpBoolCoder* br)
{
    u32 bit = 0;
    u32 split;
    u32 bigsplit;
    u32 count = br->count;
    u32 range = br->range;
    u32 value = br->value;
    u32 value_tmp = value;
    u32 range_tmp = range;
    (void)value_tmp;
    (void)range_tmp;

    split = (range + 1) >> 1;
    bigsplit = (split << 24);
    range = split;

    if(value >= bigsplit)
    {
        range = (br->range - split);
        value = (value - bigsplit);
        bit = 1;
    }

    if(range >= 0x80)
    {
        br->value = value;
        br->range = range;
        return bit;
    }
    else
    {
        range <<= 1;
        value <<= 1;
        if(!--count)
        {
            /* no more stream to read? */
            if(br->pos >= br->stream_end_pos)
            {
                br->strm_error = 1;
                return 0; /* any value, not valid */
            }
            count = 8;
            value |= br->buffer[br->pos];
            br->pos++;
        }
    }

    br->count = count;
    br->value = value;
    br->range = range;
    return bit;
}

void Vp9BoolStart(struct VpBoolCoder* br, const u8* source, u32 len)
{
    int marker_bit;
    br->lowvalue = 0;
    br->range = 255;
    br->count = 8;
    br->buffer = source;
    br->pos = 0;

    br->value = (br->buffer[0] << 24) + (br->buffer[1] << 16) +
              (br->buffer[2] << 8) + (br->buffer[3]);
    br->pos += 4;
    br->stream_end_pos = len;
    br->strm_error = br->pos > br->stream_end_pos;
    marker_bit = Vp9DecodeBool128(br);
    STREAM_TRACE("marker_bit", marker_bit);

    /* Marker bit must be zero */
    if(marker_bit != 0)
        br->strm_error = 1;
}

static int GetUnsignedBits(u32 num_values)
{
    s32 cat = 0;

    if(num_values <= 1)
        return 0;
    num_values--;
    while(num_values > 0)
    {
        cat++;
        num_values >>= 1;
    }
    return cat;
}

static u32 BoolDecodeUniform(struct VpBoolCoder* bc, u32 n)
{
    u32 value, v;
    s32 l = GetUnsignedBits(n);
    s32 m = (1 << l) - n;
    if(!l)
        return 0;
    value = Vp9ReadBits(bc, l - 1);
    if((s32)value >= m)
    {
        v = Vp9ReadBits(bc, 1);
        value = (value << 1) - m + v;
    }
    return value;
}

u32 Vp9DecodeSubExp(struct VpBoolCoder* bc, u32 k, u32 num_syms)
{
    s32 i = 0, mk = 0, value = 0;
    while(1)
    {
        s32 b = (i ? k + i - 1 : k);
        s32 a = (1 << b);

        if((s32)num_syms <= mk + 3 * a)
        {
            value = BoolDecodeUniform(bc, num_syms - mk) + mk;
            break;
        }
        else
        {
            value = Vp9ReadBits(bc, 1);
            if(value)
            {
                i++;
                mk += a;
            }
            else
            {
                value = Vp9ReadBits(bc, b) + mk;
                break;
            }
        }
    }
    return value;
}

#if 0
#include "basetype.h"
#include "vp9hwd_bool.h"

/*#define BOOLDEC_TRACE*/

u32 Vp9DecodeBool(struct VpBoolCoder* br, i32 probability) {
  u32 bit = 0;
  u32 split;
  u32 bigsplit;
  u32 count = br->count;
  u32 range = br->range;
  u32 value = br->value;
  u32 value_tmp = value;
  u32 range_tmp = range;
  (void)value_tmp;
  (void)range_tmp;

  split = 1 + (((range - 1) * probability) >> 8);
  bigsplit = (split << 24);

#ifdef BOOLDEC_TRACE
  printf("    p=%3d %9d/%4d s=%3d", probability, value >> 24, range, split);
#endif /* BOOLDEC_TRACE */

  range = split;

  if (value >= bigsplit) {
    range = br->range - split;
    value = value - bigsplit;
    bit = 1;
  }

#ifdef BOOLDEC_TRACE
  printf(" --> %d\n", bit);
#endif /* BOOLDEC_TRACE */

  if (range >= 0x80) {
    br->value = value;
    br->range = range;
#ifdef BOOLPROBS_TRACE
    printf("%-4d%-4d%-4d%-4d%-4d%-4d\n", value_tmp >> 24, range_tmp,
           probability, value >> 24, range, bit);
#endif
    return bit;
  } else {
    do {
      range += range;
      value += value;

      if (!--count) {
        /* no more stream to read? */
        if (br->pos >= br->stream_end_pos) {
          br->strm_error = 1;
          break;
        }
        count = 8;
        value |= br->buffer[br->pos];
        br->pos++;
      }
    } while (range < 0x80);
  }

  br->count = count;
  br->value = value;
  br->range = range;

#ifdef BOOLPROBS_TRACE
  printf("%-4d%-4d%-4d%-4d%-4d%-4d\n", value_tmp >> 24, range_tmp, probability,
         value >> 24, range, bit);
#endif

  return bit;
}

u32 Vp9DecodeBool128(struct VpBoolCoder* br) {

  u32 bit = 0;
  u32 split;
  u32 bigsplit;
  u32 count = br->count;
  u32 range = br->range;
  u32 value = br->value;
  u32 value_tmp = value;
  u32 range_tmp = range;
  (void)value_tmp;
  (void)range_tmp;

  split = (range + 1) >> 1;
  bigsplit = (split << 24);

#ifdef BOOLDEC_TRACE
  printf("    p=%3d %9d/%4d s=%3d", 128, value >> 24, range, split);
#endif /* BOOLDEC_TRACE */

  range = split;

  if (value >= bigsplit) {
    range = (br->range - split);
    value = (value - bigsplit);
    bit = 1;
  }

#ifdef BOOLDEC_TRACE
  printf(" --> %d\n", bit);
#endif /* BOOLDEC_TRACE */

  if (range >= 0x80) {
    br->value = value;
    br->range = range;
#ifdef BOOLPROBS_TRACE
    printf("%-4d%-4d%-4d%-4d%-4d%-4d\n", value_tmp >> 24, range_tmp, 128,
           value >> 24, range, bit);
#endif
    return bit;
  } else {
    range <<= 1;
    value <<= 1;

    if (!--count) {
      /* no more stream to read? */
      if (br->pos >= br->stream_end_pos) {
        br->strm_error = 1;
        return 0; /* any value, not valid */
      }
      count = 8;
      value |= br->buffer[br->pos];
      br->pos++;
    }
  }

  br->count = count;
  br->value = value;
  br->range = range;

#ifdef BOOLPROBS_TRACE
  printf("%-4d%-4d%-4d%-4d%-4d%-4d\n", value_tmp >> 24, range_tmp, 128,
         value >> 24, range, bit);
#endif

  return bit;
}

static int GetUnsignedBits(u32 num_values) {
  i32 cat = 0;
  if (num_values <= 1) return 0;
  num_values--;
  while (num_values > 0) {
    cat++;
    num_values >>= 1;
  }
  return cat;
}

static u32 BoolDecodeUniform(struct VpBoolCoder* bc, u32 n) {
  u32 value, v;
  i32 l = GetUnsignedBits(n);
  i32 m = (1 << l) - n;
  if (!l) return 0;
  value = Vp9ReadBits(bc, l - 1);
  if (value >= m) {
    v = Vp9ReadBits(bc, 1);
    value = (value << 1) - m + v;
  }
  return value;
}

u32 Vp9DecodeSubExp(struct VpBoolCoder* bc, u32 k, u32 num_syms) {
  i32 i = 0, mk = 0, value = 0;

  while (1) {
    i32 b = (i ? k + i - 1 : k);
    i32 a = (1 << b);
    if (num_syms <= mk + 3 * a) {
      value = BoolDecodeUniform(bc, num_syms - mk) + mk;
      break;
    } else {
      value = Vp9ReadBits(bc, 1);
      if (value) {
        i++;
        mk += a;
      } else {
        value = Vp9ReadBits(bc, b) + mk;
        break;
      }
    }
  }
  return value;
}

void Vp9BoolStart(struct VpBoolCoder* br, const u8* source, u32 len) {
  int marker_bit;

  br->lowvalue = 0;
  br->range = 255;
  br->count = 8;
  br->buffer = source;
  br->pos = 0;

  br->value = (br->buffer[0] << 24) + (br->buffer[1] << 16) +
              (br->buffer[2] << 8) + (br->buffer[3]);

  br->pos += 4;

  br->stream_end_pos = len;
  br->strm_error = br->pos > br->stream_end_pos;

  marker_bit = Vp9DecodeBool128(br);
  STREAM_TRACE("marker_bit", marker_bit);

  /* Marker bit must be zero */
  if (marker_bit != 0) br->strm_error = 1;
}

void Vp9BoolStop(struct VpBoolCoder* bc) { (void)bc; }

u32 Vp9ReadBits(struct VpBoolCoder* br, i32 bits) {
  u32 z = 0;
  i32 bit;

  for (bit = bits - 1; bit >= 0; bit--) {
    z |= (Vp9DecodeBool128(br) << bit);
  }

  return z;
}
#endif
