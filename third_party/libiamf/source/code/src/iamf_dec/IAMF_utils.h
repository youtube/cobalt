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
