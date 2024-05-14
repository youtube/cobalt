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
 * @file IAMF_pcm_decoder.c
 * @brief pcm codec.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include "IAMF_codec.h"
#include "IAMF_debug.h"
#include "IAMF_types.h"
#include "bitstream.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IAMF_PCM"

typedef int (*readf)(uint8_t *, int);
typedef struct IAMF_PCM_Context {
  float scale_i2f;
  readf func;
} IAMF_PCM_Context;

static int iamf_pcm_init(IAMF_CodecContext *ths) {
  IAMF_PCM_Context *ctx = (IAMF_PCM_Context *)ths->priv;
  uint8_t *config = ths->cspec;

  if (!ths->cspec || ths->clen <= 0) {
    return IAMF_ERR_BAD_ARG;
  }

  ths->flags = config[0];
  ths->sample_size = config[1];
  ths->sample_rate = readu32be(config, 2);

  ia_logd("sample format flags 0x%x, size %u, rate %u", ths->flags,
          ths->sample_size, ths->sample_rate);

  ctx->func = reads16le;
  ctx->scale_i2f = 1 << 15;
  if (ths->sample_size == 16) {
    if (!ths->flags) ctx->func = reads16be;
  } else if (ths->sample_size == 24) {
    ctx->scale_i2f = 1 << 23;
    if (!ths->flags)
      ctx->func = reads24be;
    else
      ctx->func = reads24le;
  } else if (ths->sample_size == 32) {
    ctx->scale_i2f = 1U << 31;
    if (!ths->flags)
      ctx->func = reads32be;
    else
      ctx->func = reads32le;
  }

  ia_logd("the scale of int to float: %f", ctx->scale_i2f);
  return IAMF_OK;
}

static int iamf_pcm_decode(IAMF_CodecContext *ths, uint8_t *buf[],
                           uint32_t len[], uint32_t count, void *pcm,
                           const uint32_t frame_size) {
  IAMF_PCM_Context *ctx = (IAMF_PCM_Context *)ths->priv;
  float *fpcm = (float *)pcm;
  int c = 0, cc;
  int sample_size_bytes = ths->sample_size / 8;
  int samples = 0;

  if (!count || count != ths->streams) return IAMF_ERR_BAD_ARG;

  if (ths->coupled_streams)
    samples = len[0] / 2 / sample_size_bytes;
  else
    samples = len[0] / sample_size_bytes;

  for (; c < ths->coupled_streams; ++c)
    if (len[c] / 2 / sample_size_bytes != samples) {
      ia_loge("the length of stream %d and 0 is different %d vs %d", c,
              len[c] / 2 / sample_size_bytes, samples);
      samples = -1;
      break;
    }

  if (samples > 0) {
    for (; c < ths->streams; ++c)
      if (len[c] / sample_size_bytes != samples) {
        ia_loge("the length of stream %d and 0 is different %d vs %d", c,
                len[c] / sample_size_bytes, samples);
        samples = -1;
        break;
      }
  }

  if (samples < 0) return IAMF_ERR_INTERNAL;

  ia_logd("cs %d, s %d, frame size %d, samples %d", ths->coupled_streams,
          ths->streams, frame_size, samples);

  if (samples != frame_size)
    ia_logw("real samples and frame size are different: %d vs %d", samples,
            frame_size);

  c = 0;
  for (; c < ths->coupled_streams; ++c) {
    for (int s = 0; s < samples; ++s) {
      for (int lf = 0; lf < 2; ++lf) {
        fpcm[samples * (c * 2 + lf) + s] =
            ctx->func(buf[c], (s * 2 + lf) * sample_size_bytes) /
            ctx->scale_i2f;
      }
    }
  }

  cc = ths->coupled_streams;
  for (; c < ths->streams; ++c) {
    for (int s = 0; s < samples; ++s) {
      fpcm[samples * (cc + c) + s] =
          ctx->func(buf[c], s * sample_size_bytes) / ctx->scale_i2f;
    }
  }
  return samples;
}

static int iamf_pcm_close(IAMF_CodecContext *ths) { return IAMF_OK; }

const IAMF_Codec iamf_pcm_decoder = {
    .cid = IAMF_CODEC_PCM,
    .priv_size = sizeof(IAMF_PCM_Context),
    .init = iamf_pcm_init,
    .decode = iamf_pcm_decode,
    .close = iamf_pcm_close,
};
