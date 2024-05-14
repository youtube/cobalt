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
 * @file IAMF_flac_decoder.c
 * @brief flac codec.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include <stdlib.h>

#include "IAMF_codec.h"
#include "IAMF_debug.h"
#include "IAMF_types.h"
#include "IAMF_utils.h"
#include "bitstream.h"
#include "flac_multistream_decoder.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IAMF_FLAC"

static int iamf_flac_close(IAMF_CodecContext *ths);

typedef struct IAMF_FLAC_Context {
  FLACMSDecoder *dec;
  int *out;
  float scale_i2f;
} IAMF_FLAC_Context;

static int iamf_flac_init(IAMF_CodecContext *ths) {
  IAMF_FLAC_Context *ctx = (IAMF_FLAC_Context *)ths->priv;
  int ret = IAMF_OK;
  int bits = 0;

  if (!ths->cspec || ths->clen <= 0) {
    return IAMF_ERR_BAD_ARG;
  }

  ctx->dec = flac_multistream_decoder_open(ths->cspec, ths->clen, ths->streams,
                                           ths->coupled_streams,
                                           AUDIO_FRAME_PLANE, &ret);
  if (!ctx->dec) {
    return ret;
  }

  bits = flac_multistream_decoder_get_sample_bits(ctx->dec);
  if (bits < 1) {
    ia_loge("metadata : Invalid sample bits.");
    iamf_flac_close(ths);
    return IAMF_ERR_INTERNAL;
  }

  ctx->scale_i2f = 1 << (bits - 1);
  ia_logd("the scale of i%d to float : %f", bits, ctx->scale_i2f);

  ctx->out = IAMF_MALLOC(
      int, MAX_FLAC_FRAME_SIZE *(ths->streams + ths->coupled_streams));
  if (!ctx->out) {
    iamf_flac_close(ths);
    return IAMF_ERR_ALLOC_FAIL;
  }

  return IAMF_OK;
}

static int iamf_flac_decode(IAMF_CodecContext *ths, uint8_t *buffer[],
                            uint32_t size[], uint32_t count, void *pcm,
                            uint32_t frame_size) {
  IAMF_FLAC_Context *ctx = (IAMF_FLAC_Context *)ths->priv;
  FLACMSDecoder *dec = (FLACMSDecoder *)ctx->dec;
  int ret = IAMF_OK;

  if (count != ths->streams) {
    return IAMF_ERR_BAD_ARG;
  }

  ret = flac_multistream_decode(dec, buffer, size, ctx->out, frame_size);
  if (ret > 0) {
    float *out = (float *)pcm;
    uint32_t samples = ret * (ths->streams + ths->coupled_streams);
    for (int i = 0; i < samples; ++i) {
      out[i] = ctx->out[i] / ctx->scale_i2f;
    }
  }

  return ret;
}

int iamf_flac_close(IAMF_CodecContext *ths) {
  IAMF_FLAC_Context *ctx = (IAMF_FLAC_Context *)ths->priv;
  FLACMSDecoder *dec = (FLACMSDecoder *)ctx->dec;

  if (dec) {
    flac_multistream_decoder_close(dec);
    ctx->dec = 0;
  }
  if (ctx->out) {
    free(ctx->out);
  }

  return IAMF_OK;
}

const IAMF_Codec iamf_flac_decoder = {
    .cid = IAMF_CODEC_FLAC,
    .priv_size = sizeof(IAMF_FLAC_Context),
    .init = iamf_flac_init,
    .decode = iamf_flac_decode,
    .close = iamf_flac_close,
};
