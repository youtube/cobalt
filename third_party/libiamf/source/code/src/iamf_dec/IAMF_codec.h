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
