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
 * @file audio_defines.h
 * @brief Audio common definitions
 * @version 0.1
 * @date Created 3/3/2023
**/

#ifndef _AUDIO_DEFINES_H_
#define _AUDIO_DEFINES_H_

#define LIMITER_MaximumTruePeak -1.0f
#define LIMITER_AttackSec 0.001f
#define LIMITER_ReleaseSec 0.200f
#define LIMITER_LookAhead 240

#define MAX_CHANNELS 12
#define MAX_OUTPUT_CHANNELS 24
#define MAX_DELAYSIZE 4096
#define CHUNK_SIZE 960
#define FRAME_SIZE 960

#define IA_FRAME_MAXSIZE 2048
#define MAX_PACKET_SIZE             \
  (MAX_CHANNELS * sizeof(int16_t) * \
   IA_FRAME_MAXSIZE)  // IA_FRAME_MAXSIZE*2/channel

#endif
