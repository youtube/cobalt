/*
BSD 3-Clause Clear License The Clear BSD License

Copyright (c) 2023, Alliance for Open Media.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
  int len = ths->clen;
  int ret = 0;

  if (!ths->cspec || ths->clen <= 0) {
    return IAMF_ERR_BAD_ARG;
  }

  int idx = 1;
  if (config[idx] != 0x40                     /* Audio ISO/IEC 14496-3 */
      || (config[idx + 1] >> 2 & 0x3f) != 5   /* AudioStream */
      || (config[idx + 1] >> 1 & 0x1) != 0) { /* upstream */
    return IAMF_ERR_BAD_ARG;
  }
  idx += 13;
  if (config[idx] != 0x05) {
    return IAMF_ERR_BAD_ARG;  // MP4DecSpecificDescrTag
  }
  ++idx;

  ths->cspec = &config[idx];
  ths->clen = len - idx;
  ia_logd("aac codec spec info size %d", ths->clen);

  ctx->dec = aac_multistream_decoder_open(ths->cspec, ths->clen, ths->streams,
                                          ths->coupled_streams,
                                          AUDIO_FRAME_PLANE, &ret);
  if (!ctx->dec) {
    return IAMF_ERR_INVALID_STATE;
  }

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
