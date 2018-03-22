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

#include "vp9hwd_decoder.h"
#include "vp9hwd_default_coef_probs.h"
#include "vp9hwd_bool.h"
#include "vp9hwd_stream.h"
#include "vp9hwd_treecoder.h"
#include "vp9hwd_entropymode.h"

extern void Vp9InitModeContexts(struct Vp9Decoder *pc);
extern void Vp9InitMbmodeProbs(struct Vp9Decoder *x);
extern void Vp9InitMvProbs(struct Vp9Decoder *x);

void Vp9StoreProbs(struct Vp9Decoder *dec)
{
    /* After adaptation the probs refresh either ARF or IPF probs. */
    if(dec->refresh_entropy_probs)
    {
        dec->entropy_last[dec->frame_context_idx] = dec->entropy;
    }
}

void Vp9ResetProbs(struct Vp9Decoder *dec)
{
    s32 i, j, k, l, m;

    Vp9InitModeContexts(dec);
    Vp9InitMbmodeProbs(dec);
    Vp9InitMvProbs(dec);

    /* Copy the default probs into two separate prob tables: part1 and part2. */
    for(i = 0; i < BLOCK_TYPES; i++)
    {
        for(j = 0; j < REF_TYPES; j++)
        {
            for(k = 0; k < COEF_BANDS; k++)
            {
                for(l = 0; l < PREV_COEF_CONTEXTS; l++)
                {
                    if(l >= 3 && k == 0)
                        continue;
                    for(m = 0; m < UNCONSTRAINED_NODES; m++)
                    {
                        dec->entropy.a.prob_coeffs[i][j][k][l][m] =
                            default_coef_probs_4x4[i][j][k][l][m];
                        dec->entropy.a.prob_coeffs8x8[i][j][k][l][m] =
                            default_coef_probs_8x8[i][j][k][l][m];
                        dec->entropy.a.prob_coeffs16x16[i][j][k][l][m] =
                            default_coef_probs_16x16[i][j][k][l][m];
                        dec->entropy.a.prob_coeffs32x32[i][j][k][l][m] =
                            default_coef_probs_32x32[i][j][k][l][m];
                    }
                }
            }
        }
    }

    if(dec->key_frame || dec->error_resilient || dec->reset_frame_context == 3)
    {
        /* Store the default probs for all saved contexts */
        for (i = 0; i < NUM_FRAME_CONTEXTS; i++)
            memcpy(&dec->entropy_last[i], &dec->entropy,
                sizeof(struct Vp9EntropyProbs));
    }
    else if (dec->reset_frame_context == 2)
    {
        memcpy(&dec->entropy_last[dec->frame_context_idx], &dec->entropy,
              sizeof(struct Vp9EntropyProbs));
    }
}

void Vp9GetProbs(struct Vp9Decoder *dec)
{
    /* Frame context tells which frame is used as reference, make
    * a copy of the context to use as base for this frame probs. */
    dec->entropy = dec->entropy_last[dec->frame_context_idx];
}

void Vp9StoreAdaptProbs(struct Vp9Decoder *dec)
{
    /* Adaptation is based on previous ctx before update. */
    dec->prev_ctx = dec->entropy.a;
}

static int MergeIndex(int v, int n, int modulus)
{
    int max1 = (n - 1 - modulus / 2) / modulus + 1;

    if(v < max1)
        v = v * modulus + modulus / 2;
    else
    {
        int w;
        v -= max1;
        w = v;
        v += (v + modulus - modulus / 2) / modulus;
        while(v % modulus == modulus / 2 ||
            w != v - (v + modulus - modulus / 2) / modulus)
            v++;
    }
    return v;
}

static int Vp9InvRecenterNonneg(int v, int m)
{
    if(v > (m << 1))
        return v;
    else if((v & 1) == 0)
        return (v >> 1) + m;
    else
        return m - ((v + 1) >> 1);
}

static int InvRemapProb(int v, int m)
{
    const int n = 255;

    v = MergeIndex(v, n - 1, MODULUS_PARAM);
    m--;
    if((m << 1) <= n)
    {
        return 1 + Vp9InvRecenterNonneg(v + 1, m);
    }
    else
    {
        return n - Vp9InvRecenterNonneg(v + 1, n - 1 - m);
    }
}

vp9_prob Vp9ReadProbDiffUpdate(struct VpBoolCoder *bc, int oldp)
{
    int delp = Vp9DecodeSubExp(bc, 4, 255);
    return (vp9_prob)InvRemapProb(delp, oldp);
}

u32 Vp9DecodeCoeffUpdate(struct VpBoolCoder *bc,
    u8 prob_coeffs[BLOCK_TYPES][REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS]
                  [ENTROPY_NODES_PART1])
{
    u32 i, j, k, l, m;
    u32 tmp;

    tmp = Vp9ReadBits(bc, 1);
    STREAM_TRACE("coeff_prob_update_flag", tmp);
    if(!tmp)
        return HANTRO_OK;

    for(i = 0; i < BLOCK_TYPES; i++)
    {
        for(j = 0; j < REF_TYPES; j++)
        {
            for(k = 0; k < COEF_BANDS; k++)
            {
                for(l = 0; l < PREV_COEF_CONTEXTS; l++)
                {
                    if(l >= 3 && k == 0)
                        continue;
                    for(m = 0; m < UNCONSTRAINED_NODES; m++)
                    {
                        tmp = Vp9DecodeBool(bc, 252);
                        CHECK_END_OF_STREAM(tmp);
                        if(tmp)
                        {
                            u8 old, nNew;
                            old = prob_coeffs[i][j][k][l][m];
                            nNew = Vp9ReadProbDiffUpdate(bc, old);
                            STREAM_TRACE("coeff_prob_delta_subexp", nNew);
                            CHECK_END_OF_STREAM(tmp);
                            prob_coeffs[i][j][k][l][m] = nNew;
                        }
                    }
                }
            }
        }
    }
    return HANTRO_OK;
}

static void UpdateNmv(struct VpBoolCoder *bc, vp9_prob *const p,
                      const vp9_prob upd_p)
{
    u32 tmp = Vp9DecodeBool(bc, upd_p);
    if(tmp)
    {
        *p = (Vp9ReadBits(bc, 7) << 1) | 1;
        STREAM_TRACE("mv_prob", *p);
    }
}

u32 Vp9DecodeMvUpdate(struct VpBoolCoder *bc, struct Vp9Decoder *dec)
{
    u32 i, j, k;
    struct NmvContext *mvctx = &dec->entropy.a.nmvc;

    for(j = 0; j < MV_JOINTS - 1; ++j)
    {
        UpdateNmv(bc, &mvctx->joints[j], VP9_NMV_UPDATE_PROB);
    }
    for(i = 0; i < 2; ++i)
    {
        UpdateNmv(bc, &mvctx->sign[i], VP9_NMV_UPDATE_PROB);
        for(j = 0; j < MV_CLASSES - 1; ++j)
        {
            UpdateNmv(bc, &mvctx->classes[i][j], VP9_NMV_UPDATE_PROB);
        }
        for(j = 0; j < CLASS0_SIZE - 1; ++j)
        {
            UpdateNmv(bc, &mvctx->class0[i][j], VP9_NMV_UPDATE_PROB);
        }
        for(j = 0; j < MV_OFFSET_BITS; ++j)
        {
            UpdateNmv(bc, &mvctx->bits[i][j], VP9_NMV_UPDATE_PROB);
        }
    }

    for(i = 0; i < 2; ++i)
    {
        for(j = 0; j < CLASS0_SIZE; ++j)
        {
            for(k = 0; k < 3; ++k)
                UpdateNmv(bc, &mvctx->class0_fp[i][j][k], VP9_NMV_UPDATE_PROB);
        }
        for(j = 0; j < 3; ++j)
        {
            UpdateNmv(bc, &mvctx->fp[i][j], VP9_NMV_UPDATE_PROB);
        }
    }

    if(dec->allow_high_precision_mv)
    {
        for(i = 0; i < 2; ++i)
        {
            UpdateNmv(bc, &mvctx->class0_hp[i], VP9_NMV_UPDATE_PROB);
            UpdateNmv(bc, &mvctx->hp[i], VP9_NMV_UPDATE_PROB);
        }
    }
    return HANTRO_OK;
}

#define MODE_COUNT_SAT 20
#define MODE_MAX_UPDATE_FACTOR 128
#define MAX_PROBS 32

static int UpdateModeCt(vp9_prob pre_prob, vp9_prob prob,
                        unsigned int branch_ct[2])
{
    int factor, count = branch_ct[0] + branch_ct[1];
    count = count > MODE_COUNT_SAT ? MODE_COUNT_SAT : count;
    factor = (MODE_MAX_UPDATE_FACTOR * count / MODE_COUNT_SAT);
    return WeightedProb(pre_prob, prob, factor);
}

static int update_mode_ct2(vp9_prob pre_prob, unsigned int branch_ct[2])
{
    return UpdateModeCt(pre_prob, GetBinaryProb(branch_ct[0], branch_ct[1]),
                      branch_ct);
}

static void UpdateModeProbs(int n_modes, const vp9_tree_index *tree,
                            unsigned int *cnt, vp9_prob *pre_probs,
                            vp9_prob *pre_probs_b, vp9_prob *dst_probs,
                            vp9_prob *dst_probs_b, u32 tok0_offset)
{
    vp9_prob probs[MAX_PROBS];
    unsigned int branch_ct[MAX_PROBS][2];
    int t/*, count, factor*/;

    //ASSERT(n_modes - 1 < MAX_PROBS);
    Vp9TreeProbsFromDistribution(tree, probs, branch_ct, cnt, tok0_offset);
    for(t = 0; t < n_modes - 1; ++t)
    {
        int count = branch_ct[t][0] + branch_ct[t][1];
        count = count > MODE_COUNT_SAT ? MODE_COUNT_SAT : count;
        int factor = (MODE_MAX_UPDATE_FACTOR * count / MODE_COUNT_SAT);

        if(t < 8 || dst_probs_b == NULL)
            dst_probs[t] = WeightedProb(pre_probs[t], probs[t], factor);
        else
            dst_probs_b[t - 8] = WeightedProb(pre_probs_b[t - 8], probs[t], factor);
    }
}

void vp9_tx_counts_to_branch_counts_8x8(unsigned int *tx_count_8x8p,
                                    unsigned int (*ct_8x8p)[2])
{
    ct_8x8p[0][0] = tx_count_8x8p[TX_4X4];
    ct_8x8p[0][1] = tx_count_8x8p[TX_8X8];
}

void vp9_tx_counts_to_branch_counts_16x16(unsigned int *tx_count_16x16p,
                                      unsigned int (*ct_16x16p)[2])
{
    ct_16x16p[0][0] = tx_count_16x16p[TX_4X4];
    ct_16x16p[0][1] = tx_count_16x16p[TX_8X8] + tx_count_16x16p[TX_16X16];
    ct_16x16p[1][0] = tx_count_16x16p[TX_8X8];
    ct_16x16p[1][1] = tx_count_16x16p[TX_16X16];
}

void vp9_tx_counts_to_branch_counts_32x32(unsigned int *tx_count_32x32p,
                                      unsigned int (*ct_32x32p)[2])
{
    ct_32x32p[0][0] = tx_count_32x32p[TX_4X4];
    ct_32x32p[0][1] = tx_count_32x32p[TX_8X8] + tx_count_32x32p[TX_16X16] +
                    tx_count_32x32p[TX_32X32];
    ct_32x32p[1][0] = tx_count_32x32p[TX_8X8];
    ct_32x32p[1][1] = tx_count_32x32p[TX_16X16] + tx_count_32x32p[TX_32X32];
    ct_32x32p[2][0] = tx_count_32x32p[TX_16X16];
    ct_32x32p[2][1] = tx_count_32x32p[TX_32X32];
}

void Vp9AdaptModeProbs(struct Vp9Decoder *cm)
{
    s32 i, j;
    struct Vp9AdaptiveEntropyProbs *fc = &cm->entropy.a;

    for(i = 0; i < INTRA_INTER_CONTEXTS; i++)
        fc->intra_inter_prob[i] = update_mode_ct2(cm->prev_ctx.intra_inter_prob[i],
                                              cm->ctx_ctr.intra_inter_count[i]);
    for(i = 0; i < COMP_INTER_CONTEXTS; i++)
        fc->comp_inter_prob[i] = update_mode_ct2(cm->prev_ctx.comp_inter_prob[i],
                                              cm->ctx_ctr.comp_inter_count[i]);
    for (i = 0; i < REF_CONTEXTS; i++)
        fc->comp_ref_prob[i] = update_mode_ct2(cm->prev_ctx.comp_ref_prob[i],
                                           cm->ctx_ctr.comp_ref_count[i]);
    for(i = 0; i < REF_CONTEXTS; i++)
    {
        for (j = 0; j < 2; j++)
        {
            fc->single_ref_prob[i][j] =
                update_mode_ct2(cm->prev_ctx.single_ref_prob[i][j],
                          cm->ctx_ctr.single_ref_count[i][j]);
        }
    }

    for(i = 0; i < BLOCK_SIZE_GROUPS; ++i)
    {
        UpdateModeProbs(
            VP9_INTRA_MODES, vp9hwd_intra_mode_tree, cm->ctx_ctr.sb_ymode_counts[i],
            cm->prev_ctx.sb_ymode_prob[i], cm->prev_ctx.sb_ymode_prob_b[i],
            cm->entropy.a.sb_ymode_prob[i], cm->entropy.a.sb_ymode_prob_b[i], 0);
    }
    for(i = 0; i < VP9_INTRA_MODES; ++i)
    {
        UpdateModeProbs(
            VP9_INTRA_MODES, vp9hwd_intra_mode_tree, cm->ctx_ctr.uv_mode_counts[i],
            cm->prev_ctx.uv_mode_prob[i], cm->prev_ctx.uv_mode_prob_b[i],
            cm->entropy.a.uv_mode_prob[i], cm->entropy.a.uv_mode_prob_b[i], 0);
    }

    for(i = 0; i < NUM_PARTITION_CONTEXTS; i++)
        UpdateModeProbs(PARTITION_TYPES, vp9hwd_partition_tree,
            cm->ctx_ctr.partition_counts[i],
            cm->prev_ctx.partition_prob[INTER_FRAME][i], NULL,
            cm->entropy.a.partition_prob[INTER_FRAME][i], NULL, 0);

    if(cm->mcomp_filter_type == SWITCHABLE)
    {
        for(i = 0; i <= VP9_SWITCHABLE_FILTERS; ++i)
        {
            UpdateModeProbs(VP9_SWITCHABLE_FILTERS, vp9hwd_switchable_interp_tree,
                      cm->ctx_ctr.switchable_interp_counts[i],
                      cm->prev_ctx.switchable_interp_prob[i], NULL,
                      cm->entropy.a.switchable_interp_prob[i], NULL, 0);
        }
    }

    if(cm->transform_mode == TX_MODE_SELECT)
    {
        int j;
        unsigned int branch_ct_8x8p[TX_SIZE_MAX_SB - 3][2];
        unsigned int branch_ct_16x16p[TX_SIZE_MAX_SB - 2][2];
        unsigned int branch_ct_32x32p[TX_SIZE_MAX_SB - 1][2];
        for(i = 0; i < TX_SIZE_CONTEXTS; ++i)
        {
            vp9_tx_counts_to_branch_counts_8x8(cm->ctx_ctr.tx8x8_count[i],
                                     branch_ct_8x8p);
            for(j = 0; j < TX_SIZE_MAX_SB - 3; ++j)
            {
                int factor;
                int count = branch_ct_8x8p[j][0] + branch_ct_8x8p[j][1];
                vp9_prob prob =
                    GetBinaryProb(branch_ct_8x8p[j][0], branch_ct_8x8p[j][1]);
                count = count > MODE_COUNT_SAT ? MODE_COUNT_SAT : count;
                factor = (MODE_MAX_UPDATE_FACTOR * count / MODE_COUNT_SAT);
                fc->tx8x8_prob[i][j] =
                    WeightedProb(cm->prev_ctx.tx8x8_prob[i][j], prob, factor);
            }
        }

        for(i = 0; i < TX_SIZE_CONTEXTS; ++i)
        {
            vp9_tx_counts_to_branch_counts_16x16(cm->ctx_ctr.tx16x16_count[i],
                                       branch_ct_16x16p);
            for(j = 0; j < TX_SIZE_MAX_SB - 2; ++j)
            {
                int factor;
                int count = branch_ct_16x16p[j][0] + branch_ct_16x16p[j][1];
                vp9_prob prob =
                    GetBinaryProb(branch_ct_16x16p[j][0], branch_ct_16x16p[j][1]);
                count = count > MODE_COUNT_SAT ? MODE_COUNT_SAT : count;
                factor = (MODE_MAX_UPDATE_FACTOR * count / MODE_COUNT_SAT);
                fc->tx16x16_prob[i][j] =
                    WeightedProb(cm->prev_ctx.tx16x16_prob[i][j], prob, factor);
            }
        }

        for(i = 0; i < TX_SIZE_CONTEXTS; ++i)
        {
            vp9_tx_counts_to_branch_counts_32x32(cm->ctx_ctr.tx32x32_count[i],
                                       branch_ct_32x32p);
            for(j = 0; j < TX_SIZE_MAX_SB - 1; ++j)
            {
                int factor;
                int count = branch_ct_32x32p[j][0] + branch_ct_32x32p[j][1];
                vp9_prob prob =
                    GetBinaryProb(branch_ct_32x32p[j][0], branch_ct_32x32p[j][1]);
                count = count > MODE_COUNT_SAT ? MODE_COUNT_SAT : count;
                factor = (MODE_MAX_UPDATE_FACTOR * count / MODE_COUNT_SAT);
                fc->tx32x32_prob[i][j] =
                    WeightedProb(cm->prev_ctx.tx32x32_prob[i][j], prob, factor);
            }
        }
    }

    for(i = 0; i < MBSKIP_CONTEXTS; ++i)
        fc->mbskip_probs[i] = update_mode_ct2(cm->prev_ctx.mbskip_probs[i],
                                              cm->ctx_ctr.mbskip_count[i]);
}

#define MVREF_COUNT_SAT 20
#define MVREF_MAX_UPDATE_FACTOR 128
void Vp9AdaptModeContext(struct Vp9Decoder *cm)
{
    int i, j;
    u32(*mode_ct)[VP9_INTER_MODES - 1][2] = cm->ctx_ctr.inter_mode_counts;

    for(j = 0; j < INTER_MODE_CONTEXTS; j++)
    {
        for(i = 0; i < VP9_INTER_MODES - 1; i++)
        {
            int count = mode_ct[j][i][0] + mode_ct[j][i][1], factor;
            count = count > MVREF_COUNT_SAT ? MVREF_COUNT_SAT : count;
            factor = (MVREF_MAX_UPDATE_FACTOR * count / MVREF_COUNT_SAT);
            cm->entropy.a.inter_mode_prob[j][i] = WeightedProb(
                cm->prev_ctx.inter_mode_prob[j][i],
                GetBinaryProb(mode_ct[j][i][0], mode_ct[j][i][1]), factor);
        }
    }
}

