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

typedef enum {
  CHANNELUNKNOWN = 0,
  CHANNELMONO,
  CHANNELSTEREO,
  CHANNEL51,
  CHANNEL512,
  CHANNEL514,
  CHANNEL71,
  CHANNEL712,
  CHANNEL714,
  CHANNEL312,
  CHANNELBINAURAL
} channelLayout;

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
