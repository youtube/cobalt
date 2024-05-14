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
 * @file iamf_header.h
 * @brief IAMF information APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef IAMF_HEADER_H
#define IAMF_HEADER_H

#include <stdint.h>

#define OPUS_DEMIXING_MATRIX_SIZE_MAX (18 * 18 * 2)

typedef struct {
  int magic_id;
  int version;
  int channels; /* Number of channels: 1..255 */
  int preskip;
  uint32_t input_sample_rate;
  int gain; /* in dB S7.8 should be zero whenever possible */
  int channel_mapping;
  /* The rest is only used if channel_mapping != 0 */
  int nb_streams;
  int nb_coupled;
  int cmf;
  int nb_output_channels;
  unsigned char stream_map[255];
  unsigned char
      dmatrix[OPUS_DEMIXING_MATRIX_SIZE_MAX];  // 2 channel demixing matrix

} OpusHeader;

typedef struct {
  // MP4DecoderConfigDescriptor
  int object_type_indication;
  int stream_type;
  int upstream;

  // MP4DecSpecificInfoDescriptor
  int audio_object_type;
  int sample_rate;
  int channels;

  // GASpecificConfig
  int frame_length_flag;
  int depends_on_core_coder;
  int extension_flag;
} AACHeader;

typedef struct {
  uint32_t description_length;
  uint8_t *description;
  int skip;
  int timescale;
} IAMFHeader;

#ifdef __cplusplus
extern "C" {
#endif
int iamf_header_read_description_OBUs(IAMFHeader *h, uint8_t **dst,
                                      uint32_t *size);
void iamf_header_free(IAMFHeader *h, int n);
#ifdef __cplusplus
}
#endif
extern const int wav_permute_matrix[8][8];

#endif
