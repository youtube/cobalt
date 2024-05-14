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
 * @file IAMF_core_decoder.h
 * @brief Core decoder APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef IAMF_CORE_DECODER_H_
#define IAMF_CORE_DECODER_H_

#include <stdint.h>

#include "IAMF_codec.h"
#include "IAMF_defines.h"

typedef struct IAMF_CoreDecoder IAMF_CoreDecoder;

IAMF_CoreDecoder *iamf_core_decoder_open(IAMF_CodecID cid);
void iamf_core_decoder_close(IAMF_CoreDecoder *ths);
int iamf_core_decoder_init(IAMF_CoreDecoder *ths);
int iamf_core_decoder_set_codec_conf(IAMF_CoreDecoder *ths, uint8_t *spec,
                                     uint32_t len);
int iamf_core_decoder_set_streams_info(IAMF_CoreDecoder *ths, uint32_t mode,
                                       uint8_t channels, uint8_t streams,
                                       uint8_t coupled_streams,
                                       uint8_t mapping[],
                                       uint32_t mapping_size);
int iamf_core_decoder_decode(IAMF_CoreDecoder *ths, uint8_t *buffers[],
                             uint32_t *sizes, uint32_t count, float *out,
                             uint32_t frame_size);
int iamf_core_decoder_get_delay(IAMF_CoreDecoder *ths);

#endif /* IAMF_CORE_DECODER_H_ */
