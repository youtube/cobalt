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
