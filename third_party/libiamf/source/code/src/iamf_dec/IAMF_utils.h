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
 * @file IAMF_utils.h
 * @brief Utils APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef IAMF_UITLS_H
#define IAMF_UITLS_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "IAMF_defines.h"
#include "IAMF_types.h"

#define IAMF_MALLOC(type, n) ((type *)malloc(sizeof(type) * (n)))
#define IAMF_REALLOC(type, p, n) ((type *)realloc(p, sizeof(type) * (n)))
#define IAMF_MALLOCZ(type, n) ((type *)calloc(n, sizeof(type)))
#define IAMF_FREE(p) \
  {                  \
    if (p) free(p);  \
  }
#define IAMF_FREEP(p) iamf_freep((void **)p)

#define RSHIFT(a) (1 << (a))

void iamf_freep(void **p);

IAMF_CodecID iamf_codec_4cc_get_codecID(uint32_t id);
int iamf_codec_check(IAMF_CodecID cid);
const char *iamf_codec_name(IAMF_CodecID cid);

const char *ia_error_code_string(int ec);

int ia_channel_layout_type_check(IAChannelLayoutType type);
const char *ia_channel_layout_name(IAChannelLayoutType type);
int ia_channel_layout_get_channels_count(IAChannelLayoutType type);
int ia_channel_layout_get_channels(IAChannelLayoutType type,
                                   IAChannel *channels, uint32_t count);
int ia_channel_layout_get_category_channels_count(IAChannelLayoutType type,
                                                  uint32_t categorys);

int ia_audio_layer_get_channels(IAChannelLayoutType type, IAChannel *channels,
                                uint32_t count);
const char *ia_channel_name(IAChannel ch);

int bit1_count(uint32_t value);

int iamf_valid_mix_mode(int mode);
const MixFactors *iamf_get_mix_factors(int mode);

#endif /* IAMF_UITLS_H */
