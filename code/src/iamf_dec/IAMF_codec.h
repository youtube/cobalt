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
 * @file IAMF_codec.h
 * @brief Common codec APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef IAMF_CODEC_H_
#define IAMF_CODEC_H_

#include <stdint.h>

#include "IAMF_defines.h"

typedef struct IAMF_CodecContext {
  void *priv;

  uint8_t *cspec;
  uint32_t clen;

  uint32_t delay;
  uint32_t flags;
  uint32_t sample_rate;
  uint32_t sample_size;
  uint32_t channel_mapping_family;

  uint8_t streams;
  uint8_t coupled_streams;
  uint8_t channels;
} IAMF_CodecContext;

typedef struct IAMF_Codec {
  IAMF_CodecID cid;
  uint32_t flags;
  uint32_t priv_size;
  int (*init)(IAMF_CodecContext *ths);
  int (*decode)(IAMF_CodecContext *ths, uint8_t *buf[], uint32_t len[],
                uint32_t count, void *pcm, const uint32_t frame_size);
  int (*info)(IAMF_CodecContext *ths);
  int (*close)(IAMF_CodecContext *ths);
} IAMF_Codec;

#endif /* IAMF_CODEC_H_ */
