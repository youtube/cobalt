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
 * @file IAMF_types.h
 * @brief Internal definition.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef IAMF_TYPES_H_
#define IAMF_TYPES_H_

typedef enum {
  IA_CH_RE_L,
  IA_CH_RE_C,
  IA_CH_RE_R,
  IA_CH_RE_LS,
  IA_CH_RE_RS,
  IA_CH_RE_LTF,
  IA_CH_RE_RTF,
  IA_CH_RE_LB,
  IA_CH_RE_RB,
  IA_CH_RE_LTB,
  IA_CH_RE_RTB,
  IA_CH_RE_LFE,
  IA_CH_RE_COUNT,

  IA_CH_RE_RSS = IA_CH_RE_RS,
  IA_CH_RE_LSS = IA_CH_RE_LS,
  IA_CH_RE_RTR = IA_CH_RE_RTB,
  IA_CH_RE_LTR = IA_CH_RE_LTB,
  IA_CH_RE_RSR = IA_CH_RE_RB,
  IA_CH_RE_LRS = IA_CH_RE_LB,
} IAReconChannel;

typedef enum {
  IA_CH_INVALID,
  IA_CH_L7,
  IA_CH_R7,
  IA_CH_C,
  IA_CH_LFE,
  IA_CH_SL7,
  IA_CH_SR7,
  IA_CH_BL7,
  IA_CH_BR7,
  IA_CH_HFL,
  IA_CH_HFR,
  IA_CH_HBL,
  IA_CH_HBR,
  IA_CH_MONO,
  IA_CH_L2,
  IA_CH_R2,
  IA_CH_TL,
  IA_CH_TR,
  IA_CH_L3,
  IA_CH_R3,
  IA_CH_SL5,
  IA_CH_SR5,
  IA_CH_HL,
  IA_CH_HR,
  IA_CH_COUNT,

  IA_CH_L5 = IA_CH_L7,
  IA_CH_R5 = IA_CH_R7,
} IAChannel;

typedef enum {
  AUDIO_FRAME_PLANE = 0x1,
} AFlag;

typedef enum {
  STREAM_MODE_AMBISONICS_NONE,
  STREAM_MODE_AMBISONICS_MONO,
  STREAM_MODE_AMBISONICS_PROJECTION
} IAMF_Stream_Mode;

typedef struct MixFactors {
  float alpha;
  float beta;
  float gamma;
  float delta;
  int w_idx_offset;
} MixFactors;

typedef enum { IAMF_PROFILE_SIMPLE, IAMF_PROFILE_BASE } IAMF_Profile;

#define U8_MASK 0xFF
#define U16_MASK 0xFFFF

#define IA_CH_LAYOUT_MAX_CHANNELS 12

#define OPUS_FRAME_SIZE 960
#define MAX_OPUS_FRAME_SIZE OPUS_FRAME_SIZE * 6
#define AAC_FRAME_SIZE 1024
#define MAX_AAC_FRAME_SIZE 2048
#define MAX_FRAME_SIZE AAC_FRAME_SIZE * 6
#define MAX_FLAC_FRAME_SIZE 32768

#define MAX_STREAMS 255

#define IA_CH_CATE_SURROUND 0x100
#define IA_CH_CATE_WEIGHT 0X200
#define IA_CH_CATE_TOP 0X400

#endif /* IAMF_TYPES_H_ */
