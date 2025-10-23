/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

/**
 * @file IAMF_aac_decoder.c
 * @brief aac codec.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include <stdlib.h>

#include "IAMF_codec.h"
#include "IAMF_debug.h"
#include "IAMF_types.h"
#include "aac_multistream_decoder.h"
#include "bitstream.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IAMF_AAC"

static int iamf_aac_close(IAMF_CodecContext *ths);

typedef struct IAMF_AAC_Context {
  AACMSDecoder *dec;
  short *out;
} IAMF_AAC_Context;

/**
 *  IAC-AAC-LC Specific
 *
 *  Constraints :
 *  1, objectTypeIndication = 0x40
 *  2, streamType = 0x05 (Audio Stream)
 *  3, upstream = 0
 *  4, decSpecificInfo(): The syntax and values shall conform to
 *     AudioSpecificConfig() of [MP4-Audio] with following constraints:
 *     1> audioObjectType = 2
 *     2> channelConfiguration should be set to 2
 *     3> GASpecificConfig(): The syntax and values shall conform to
 *        GASpecificConfig() of [MP4-Audio] with following constraints:
 *        1) frameLengthFlag = 0 (1024 lines IMDCT)
 *        2) dependsOnCoreCoder = 0
 *        3) extensionFlag = 0
 * */

static int iamf_aac_init(IAMF_CodecContext *ths) {
  IAMF_AAC_Context *ctx = (IAMF_AAC_Context *)ths->priv;
  uint8_t *config = ths->cspec;
  BitStream b;
  int ret = 0;

  bs(&b, ths->cspec, ths->clen);
  if (bs_get32b(&b, 8) != 0x04) return IAMF_ERR_BAD_ARG;
  bs_getExpandableSize(&b);

  if (bs_get32b(&b, 8) != 0x40 || bs_get32b(&b, 6) != 5 || bs_get32b(&b, 1))
    return IAMF_ERR_BAD_ARG;

  bs_skipABytes(&b, 11);

  if (bs_get32b(&b, 8) != 0x05) return IAMF_ERR_BAD_ARG;

  ths->clen = bs_getExpandableSize(&b);
  ths->cspec = config + bs_tell(&b);
  ia_logd("aac codec spec info size %d", ths->clen);

  ctx->dec = aac_multistream_decoder_open(ths->cspec, ths->clen, ths->streams,
                                          ths->coupled_streams,
                                          AUDIO_FRAME_PLANE, &ret);
  if (!ctx->dec) return IAMF_ERR_INVALID_STATE;

  ctx->out = (short *)malloc(sizeof(short) * MAX_AAC_FRAME_SIZE *
                             (ths->streams + ths->coupled_streams));
  if (!ctx->out) {
    iamf_aac_close(ths);
    return IAMF_ERR_ALLOC_FAIL;
  }

  return IAMF_OK;
}

static int iamf_aac_decode(IAMF_CodecContext *ths, uint8_t *buffer[],
                           uint32_t size[], uint32_t count, void *pcm,
                           uint32_t frame_size) {
  IAMF_AAC_Context *ctx = (IAMF_AAC_Context *)ths->priv;
  AACMSDecoder *dec = (AACMSDecoder *)ctx->dec;
  int ret = IAMF_OK;

  if (count != ths->streams) {
    return IAMF_ERR_BAD_ARG;
  }

  ret = aac_multistream_decode(dec, buffer, size, ctx->out, frame_size);
  if (ret > 0) {
    float *out = (float *)pcm;
    uint32_t samples = ret * (ths->streams + ths->coupled_streams);
    for (int i = 0; i < samples; ++i) {
      out[i] = ctx->out[i] / 32768.f;
    }
  }

  return ret;
}

int iamf_aac_info(IAMF_CodecContext *ths) {
  IAMF_AAC_Context *ctx = (IAMF_AAC_Context *)ths->priv;
  AACMSDecoder *dec = (AACMSDecoder *)ctx->dec;
  ths->delay = aac_multistream_decoder_get_delay(dec);
  return IAMF_OK;
}

int iamf_aac_close(IAMF_CodecContext *ths) {
  IAMF_AAC_Context *ctx = (IAMF_AAC_Context *)ths->priv;
  AACMSDecoder *dec = (AACMSDecoder *)ctx->dec;

  if (dec) {
    aac_multistream_decoder_close(dec);
    ctx->dec = 0;
  }
  if (ctx->out) {
    free(ctx->out);
  }

  return IAMF_OK;
}

const IAMF_Codec iamf_aac_decoder = {
    .cid = IAMF_CODEC_AAC,
    .priv_size = sizeof(IAMF_AAC_Context),
    .init = iamf_aac_init,
    .decode = iamf_aac_decode,
    .info = iamf_aac_info,
    .close = iamf_aac_close,
};
