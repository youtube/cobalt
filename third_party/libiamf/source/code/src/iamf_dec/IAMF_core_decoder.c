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
 * @file IAMF_core_decoder.c
 * @brief Core decoder.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "IAMF_core_decoder.h"
#include "IAMF_debug.h"
#include "IAMF_types.h"
#include "IAMF_utils.h"
#include "bitstream.h"
#include "fixedp11_5.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IAMF_CORE"

#ifdef CONFIG_OPUS_CODEC
extern const IAMF_Codec iamf_opus_decoder;
#endif

#ifdef CONFIG_AAC_CODEC
extern const IAMF_Codec iamf_aac_decoder;
#endif

#ifdef CONFIG_FLAC_CODEC
extern const IAMF_Codec iamf_flac_decoder;
#endif

extern const IAMF_Codec iamf_pcm_decoder;

struct IAMF_CoreDecoder {
  IAMF_CodecID cid;
  const IAMF_Codec *cdec;
  IAMF_CodecContext *ctx;

  int ambisonics;
  void *matrix;
  void *buffer;
};

typedef struct FloatMatrix {
  float *matrix;
  int row;
  int column;
} FloatMatrix;

static const IAMF_Codec *gCodecs[] = {0,
#ifdef CONFIG_OPUS_CODEC
                                      &iamf_opus_decoder,
#else
                                      0,
#endif
#ifdef CONFIG_AAC_CODEC
                                      &iamf_aac_decoder,
#else
                                      0,
#endif
#ifdef CONFIG_FLAC_CODEC
                                      &iamf_flac_decoder,
#else
                                      0,
#endif
                                      &iamf_pcm_decoder

};

static int iamf_core_decoder_convert_mono(IAMF_CoreDecoder *ths, float *out,
                                          float *in, uint32_t frame_size) {
  uint8_t *map = ths->matrix;

  memset(out, 0, sizeof(float) * frame_size * ths->ctx->channels);
  for (int i = 0; i < ths->ctx->channels; ++i) {
    if (map[i] != 255)
      memcpy(&out[frame_size * i], &in[frame_size * map[i]],
             frame_size * sizeof(float));
    else
      memset(&out[frame_size * i], 0, frame_size * sizeof(float));
  }
  return IAMF_OK;
}
static int iamf_core_decoder_convert_projection(IAMF_CoreDecoder *ths,
                                                float *out, float *in,
                                                uint32_t frame_size) {
  FloatMatrix *matrix = ths->matrix;
  for (int s = 0; s < frame_size; ++s) {
    for (int r = 0; r < matrix->row; ++r) {
      out[r * frame_size + s] = .0f;
      for (int l = 0; l < matrix->column; ++l) {
        out[r * frame_size + s] +=
            in[l * frame_size + s] * matrix->matrix[l * matrix->row + r];
      }
    }
  }
  return IAMF_OK;
}

IAMF_CoreDecoder *iamf_core_decoder_open(IAMF_CodecID cid) {
  IAMF_CoreDecoder *ths = 0;
  IAMF_CodecContext *ctx = 0;
  int ec = IAMF_OK;

  if (!iamf_codec_check(cid)) {
    ia_loge("Invlid codec id %u", cid);
    return 0;
  }

  if (!gCodecs[cid]) {
    ia_loge("Unimplment %s decoder.", iamf_codec_name(cid));
    return 0;
  }

  ths = IAMF_MALLOCZ(IAMF_CoreDecoder, 1);
  if (!ths) {
    ia_loge("%s : IAMF_CoreDecoder handle.",
            ia_error_code_string(IAMF_ERR_ALLOC_FAIL));
    return 0;
  }

  ths->cid = cid;
  ths->cdec = gCodecs[cid];

  ctx = IAMF_MALLOCZ(IAMF_CodecContext, 1);
  if (!ctx) {
    ec = IAMF_ERR_ALLOC_FAIL;
    ia_loge("%s : IAMF_CodecContext handle.", ia_error_code_string(ec));
    goto termination;
  }
  ths->ctx = ctx;

  ctx->priv = IAMF_MALLOCZ(char, ths->cdec->priv_size);
  if (!ctx->priv) {
    ec = IAMF_ERR_ALLOC_FAIL;
    ia_loge("%s : private data.", ia_error_code_string(ec));
    goto termination;
  }

termination:
  if (ec != IAMF_OK) {
    iamf_core_decoder_close(ths);
    ths = 0;
  }
  return ths;
}

void iamf_core_decoder_close(IAMF_CoreDecoder *ths) {
  if (ths) {
    if (ths->ctx) {
      if (ths->cdec) ths->cdec->close(ths->ctx);
      if (ths->ctx->priv) {
        free(ths->ctx->priv);
      }
      free(ths->ctx);
    }

    if (ths->matrix) {
      if (ths->ambisonics == STREAM_MODE_AMBISONICS_PROJECTION) {
        FloatMatrix *fm = ths->matrix;
        if (fm->matrix) free(fm->matrix);
      }
      free(ths->matrix);
    }

    if (ths->buffer) free(ths->buffer);

    free(ths);
  }
}

int iamf_core_decoder_init(IAMF_CoreDecoder *ths) {
  IAMF_CodecContext *ctx = ths->ctx;
  return ths->cdec->init(ctx);
}

int iamf_core_decoder_set_codec_conf(IAMF_CoreDecoder *ths, uint8_t *spec,
                                     uint32_t len) {
  IAMF_CodecContext *ctx = ths->ctx;
  ctx->cspec = spec;
  ctx->clen = len;
  return IAMF_OK;
}

int iamf_core_decoder_set_streams_info(IAMF_CoreDecoder *ths, uint32_t mode,
                                       uint8_t channels, uint8_t streams,
                                       uint8_t coupled_streams,
                                       uint8_t mapping[],
                                       uint32_t mapping_size) {
  IAMF_CodecContext *ctx = ths->ctx;
  ths->ambisonics = mode;
  ctx->channels = channels;
  ctx->streams = streams;
  ctx->coupled_streams = coupled_streams;
  if (mapping && mapping_size > 0) {
    if (ths->ambisonics == STREAM_MODE_AMBISONICS_PROJECTION) {
      FloatMatrix *matrix = IAMF_MALLOCZ(FloatMatrix, 1);
      int count;
      BitStream b;
      float *factors;

      if (!matrix) return IAMF_ERR_ALLOC_FAIL;

      bs(&b, mapping, mapping_size);

      matrix->row = ctx->channels;
      matrix->column = ctx->streams + ctx->coupled_streams;
      count = matrix->row * matrix->column;
      factors = IAMF_MALLOCZ(float, count);
      if (!factors) {
        free(matrix);
        return IAMF_ERR_ALLOC_FAIL;
      }
      matrix->matrix = factors;

      for (int i = 0; i < count; ++i) {
        factors[i] = q_to_float(bs_getA16b(&b), 15);
        ia_logi("factor %d : %f", i, factors[i]);
      }
      ths->matrix = matrix;
    } else if (ths->ambisonics == STREAM_MODE_AMBISONICS_MONO) {
      uint8_t *matrix = IAMF_MALLOCZ(uint8_t, mapping_size);
      if (!matrix) return IAMF_ERR_ALLOC_FAIL;

      if (channels != mapping_size) {
        free(matrix);
        ia_loge("Invalid ambisonics mono info.");
        return IAMF_ERR_BAD_ARG;
      }
      memcpy(matrix, mapping, mapping_size);
      ths->matrix = matrix;
    }
  }
  return IAMF_OK;
}

int iamf_core_decoder_decode(IAMF_CoreDecoder *ths, uint8_t *buffer[],
                             uint32_t size[], uint32_t count, float *out,
                             uint32_t frame_size) {
  int ret = IAMF_OK;
  IAMF_CodecContext *ctx = ths->ctx;
  if (ctx->streams != count) return IAMF_ERR_BUFFER_TOO_SMALL;
  if (ths->ambisonics == STREAM_MODE_AMBISONICS_NONE)
    return ths->cdec->decode(ctx, buffer, size, count, out, frame_size);

  if (!ths->buffer) {
    float *block = 0;

    block = IAMF_MALLOC(float, ctx->channels *frame_size);
    if (!block) return IAMF_ERR_ALLOC_FAIL;
    ths->buffer = block;
  }
  ret = ths->cdec->decode(ctx, buffer, size, count, ths->buffer, frame_size);
  if (ret > 0) {
    if (ths->ambisonics == STREAM_MODE_AMBISONICS_PROJECTION)
      iamf_core_decoder_convert_projection(ths, out, ths->buffer, ret);
    else
      iamf_core_decoder_convert_mono(ths, out, ths->buffer, ret);
  }
  return ret;
}

int iamf_core_decoder_get_delay(IAMF_CoreDecoder *ths) {
  if (ths->cdec->info) ths->cdec->info(ths->ctx);
  if (ths && ths->ctx) {
    return ths->ctx->delay;
  }
  return 0;
}
