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
 * @file IAMF_defines.h
 * @brief AMF Common defines
 * @version 0.1
 * @date Created 3/3/2023
 **/

#ifndef IAMF_DEFINES_H
#define IAMF_DEFINES_H

#include <stdint.h>

/**
 * Audio Element Type
 * */

typedef enum {
  AUDIO_ELEMENT_INVALID = -1,
  AUDIO_ELEMENT_CHANNEL_BASED,
  AUDIO_ELEMENT_SCENE_BASED,
  AUDIO_ELEMENT_COUNT
} AudioElementType;

typedef enum AmbisonicsMode {
  AMBISONICS_MONO,
  AMBISONICS_PROJECTION
} AmbisonicsMode;

typedef enum IAMF_LayoutType {
  IAMF_LAYOUT_TYPE_NOT_DEFINED = 0,
  IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION = 2,
  IAMF_LAYOUT_TYPE_BINAURAL,
} IAMF_LayoutType;

typedef enum IAMF_SoundSystem {
  SOUND_SYSTEM_INVALID = -1,
  SOUND_SYSTEM_A,        // 0+2+0, 0
  SOUND_SYSTEM_B,        // 0+5+0, 1
  SOUND_SYSTEM_C,        // 2+5+0, 1
  SOUND_SYSTEM_D,        // 4+5+0, 1
  SOUND_SYSTEM_E,        // 4+5+1, 1
  SOUND_SYSTEM_F,        // 3+7+0, 2
  SOUND_SYSTEM_G,        // 4+9+0, 1
  SOUND_SYSTEM_H,        // 9+10+3, 2
  SOUND_SYSTEM_I,        // 0+7+0, 1
  SOUND_SYSTEM_J,        // 4+7+0, 1
  SOUND_SYSTEM_EXT_712,  // 2+7+0, 1
  SOUND_SYSTEM_EXT_312,  // 2+3+0, 1
  SOUND_SYSTEM_MONO,     // 0+1+0, 0
  SOUND_SYSTEM_END
} IAMF_SoundSystem;

typedef enum IAMF_ParameterType {
  IAMF_PARAMETER_TYPE_MIX_GAIN = 0,
  IAMF_PARAMETER_TYPE_DEMIXING,
  IAMF_PARAMETER_TYPE_RECON_GAIN,
} IAMF_ParameterType;

typedef enum IAMF_AnimationType {
  ANIMATION_TYPE_INVALID = -1,
  ANIMATION_TYPE_STEP,
  ANIMATION_TYPE_LINEAR,
  ANIMATION_TYPE_BEZIER
} IAMF_AnimationType;
/**
 *  Layout Syntax:
 *
 *  class layout() {
 *    unsigned int (2) layout_type;
 *
 *    if (layout_type == LOUDSPEAKERS_SS_CONVENTION) {
 *      unsigned int (4) sound_system;
 *      unsigned int (2) reserved;
 *    } else if (layout_type == BINAURAL or NOT_DEFINED) {
 *      unsigned int (6) reserved;
 *    }
 *  }
 *
 * */
typedef struct IAMF_Layout {
  union {
    struct {
      uint8_t reserved : 2;
      uint8_t sound_system : 4;
      uint8_t type : 2;
    } sound_system;

    struct {
      uint8_t reserved : 6;
      uint8_t type : 2;
    } binaural;

    struct {
      uint8_t reserved : 6;
      uint8_t type : 2;
    };
  };
} IAMF_Layout;

/**
 *
 *  Loudness Info Syntax:
 *
 *  class loudness_info() {
 *    unsigned int (8) info_type;
 *    signed int (16) integrated_loudness;
 *    signed int (16) digital_peak;
 *
 *    if (info_type & 1) {
 *      signed int (16) true_peak;
 *    }
 *
 *    if (info_type & 2) {
 *      unsigned int (8) num_anchored_loudness;
 *      for (i = 0; i < num_anchored_loudness; i++) {
 *        unsigned int (8) anchor_element;
 *        signed int (16) anchored_loudness;
 *      }
 *    }
 *  }
 *
 * */

typedef struct _anchor_loudness_t {
  uint8_t anchor_element;
  int16_t anchored_loudness;
} anchor_loudness_t;

typedef struct IAMF_LoudnessInfo {
  uint8_t info_type;
  int16_t integrated_loudness;
  int16_t digital_peak;
  int16_t true_peak;
  uint8_t num_anchor_loudness;
  anchor_loudness_t *anchor_loudness;
} IAMF_LoudnessInfo;

/**
 * Codec ID
 * */
typedef enum {
  IAMF_CODEC_UNKNOWN = 0,
  IAMF_CODEC_OPUS,
  IAMF_CODEC_AAC,
  IAMF_CODEC_FLAC,
  IAMF_CODEC_PCM,
  IAMF_CODEC_COUNT
} IAMF_CodecID;

/**
 * Error codes.
 * */

enum {
  IAMF_OK = 0,
  IAMF_ERR_BAD_ARG = -1,
  IAMF_ERR_BUFFER_TOO_SMALL = -2,
  IAMF_ERR_INTERNAL = -3,
  IAMF_ERR_INVALID_PACKET = -4,
  IAMF_ERR_INVALID_STATE = -5,
  IAMF_ERR_UNIMPLEMENTED = -6,
  IAMF_ERR_ALLOC_FAIL = -7,
};

/**
 * IA channel layout type.
 * */

typedef enum {
  IA_CHANNEL_LAYOUT_INVALID = -1,
  IA_CHANNEL_LAYOUT_MONO = 0,  // 1.0.0
  IA_CHANNEL_LAYOUT_STEREO,    // 2.0.0
  IA_CHANNEL_LAYOUT_510,       // 5.1.0
  IA_CHANNEL_LAYOUT_512,       // 5.1.2
  IA_CHANNEL_LAYOUT_514,       // 5.1.4
  IA_CHANNEL_LAYOUT_710,       // 7.1.0
  IA_CHANNEL_LAYOUT_712,       // 7.1.2
  IA_CHANNEL_LAYOUT_714,       // 7.1.4
  IA_CHANNEL_LAYOUT_312,       // 3.1.2
  IA_CHANNEL_LAYOUT_BINAURAL,  // binaural
  IA_CHANNEL_LAYOUT_COUNT
} IAChannelLayoutType;
#endif /* IAMF_DEFINES_H */
