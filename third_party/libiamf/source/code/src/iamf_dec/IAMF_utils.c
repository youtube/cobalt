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
 * @file IAMF_utils.c
 * @brief Utils.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include "IAMF_utils.h"

void iamf_freep(void **p) {
  if (p && *p) {
    free(*p);
    *p = 0;
  }
}

#define TAG(a, b, c, d) ((a) | (b) << 8 | (c) << 16 | (d) << 24)
IAMF_CodecID iamf_codec_4cc_get_codecID(uint32_t id) {
  switch (id) {
    case TAG('m', 'p', '4', 'a'):
      return IAMF_CODEC_AAC;

    case TAG('O', 'p', 'u', 's'):
      return IAMF_CODEC_OPUS;

    case TAG('f', 'L', 'a', 'C'):
      return IAMF_CODEC_FLAC;

    case TAG('i', 'p', 'c', 'm'):
      return IAMF_CODEC_PCM;

    default:
      return IAMF_CODEC_UNKNOWN;
  }
}

int iamf_codec_check(IAMF_CodecID cid) {
  return cid >= IAMF_CODEC_OPUS && cid < IAMF_CODEC_COUNT;
}

static const char *gIAMFCodecName[IAMF_CODEC_COUNT] = {"None", "OPUS", "AAC-LC",
                                                       "FLAC", "PCM"};

const char *iamf_codec_name(IAMF_CodecID cid) {
  if (iamf_codec_check(cid)) {
    return gIAMFCodecName[cid];
  }
  return "UNKNOWN";
}

static const char *gIAECString[] = {"Ok",
                                    "Bad argments",
                                    "Unknown",
                                    "Internal error",
                                    "Invalid packet",
                                    "Invalid state",
                                    "Unimplemented",
                                    "Memory allocation failure"};

const char *ia_error_code_string(int ec) {
  int cnt = sizeof(gIAECString) / sizeof(char *);
  int idx = -ec;
  if (idx >= 0 && idx < cnt) {
    return gIAECString[idx];
  }
  return "Unknown";
}

int ia_channel_layout_type_check(IAChannelLayoutType type) {
  return type > IA_CHANNEL_LAYOUT_INVALID && type < IA_CHANNEL_LAYOUT_COUNT;
}

static const char *gIACLName[] = {"1.0.0", "2.0.0",   "5.1.0", "5.1.2",
                                  "5.1.4", "7.1.0",   "7.1.2", "7.1.4",
                                  "3.1.2", "binaural"};

const char *ia_channel_layout_name(IAChannelLayoutType type) {
  if (ia_channel_layout_type_check(type)) {
    return gIACLName[type];
  }
  return "Unknown";
}

static const int gIACLChCount[] = {1, 2, 6, 8, 10, 8, 10, 12, 6, 2};

int ia_channel_layout_get_channels_count(IAChannelLayoutType type) {
  return ia_channel_layout_type_check(type) ? gIACLChCount[type] : 0;
}

static const IAChannel gIACLChannels[][IA_CH_LAYOUT_MAX_CHANNELS] = {
    {IA_CH_MONO},
    {IA_CH_L2, IA_CH_R2},
    {IA_CH_L5, IA_CH_R5, IA_CH_C, IA_CH_LFE, IA_CH_SL5, IA_CH_SR5},
    {IA_CH_L5, IA_CH_R5, IA_CH_C, IA_CH_LFE, IA_CH_SL5, IA_CH_SR5, IA_CH_HL,
     IA_CH_HR},
    {IA_CH_L5, IA_CH_R5, IA_CH_C, IA_CH_LFE, IA_CH_SL5, IA_CH_SR5, IA_CH_HFL,
     IA_CH_HFR, IA_CH_HBL, IA_CH_HBR},
    {IA_CH_L7, IA_CH_R7, IA_CH_C, IA_CH_LFE, IA_CH_SL7, IA_CH_SR7, IA_CH_BL7,
     IA_CH_BR7},
    {IA_CH_L7, IA_CH_R7, IA_CH_C, IA_CH_LFE, IA_CH_SL7, IA_CH_SR7, IA_CH_BL7,
     IA_CH_BR7, IA_CH_HL, IA_CH_HR},
    {IA_CH_L7, IA_CH_R7, IA_CH_C, IA_CH_LFE, IA_CH_SL7, IA_CH_SR7, IA_CH_BL7,
     IA_CH_BR7, IA_CH_HFL, IA_CH_HFR, IA_CH_HBL, IA_CH_HBR},
    {IA_CH_L3, IA_CH_R3, IA_CH_C, IA_CH_LFE, IA_CH_TL, IA_CH_TR},
    {IA_CH_L2, IA_CH_R2},
};

int ia_channel_layout_get_channels(IAChannelLayoutType type,
                                   IAChannel *channels, uint32_t count) {
  int ret = 0;
  if (!ia_channel_layout_type_check(type)) {
    return 0;
  }

  ret = ia_channel_layout_get_channels_count(type);
  if (count < ret) {
    return IAMF_ERR_BUFFER_TOO_SMALL;
  }

  for (int c = 0; c < ret; ++c) {
    channels[c] = gIACLChannels[type][c];
  }

  return ret;
}

static const struct {
  int s;
  int w;
  int t;
} gIACLC2Count[IA_CHANNEL_LAYOUT_COUNT] = {
    {1, 0, 0}, {2, 0, 0}, {5, 1, 0}, {5, 1, 2}, {5, 1, 4},
    {7, 1, 0}, {7, 1, 2}, {7, 1, 4}, {3, 1, 2}, {2, 0, 0}};

int ia_channel_layout_get_category_channels_count(IAChannelLayoutType type,
                                                  uint32_t categorys) {
  int chs = 0;
  if (!ia_channel_layout_type_check(type)) {
    return 0;
  }

  if (categorys & IA_CH_CATE_TOP) {
    chs += gIACLC2Count[type].t;
  }
  if (categorys & IA_CH_CATE_WEIGHT) {
    chs += gIACLC2Count[type].w;
  }
  if (categorys & IA_CH_CATE_SURROUND) {
    chs += gIACLC2Count[type].s;
  }
  return chs;
}

static const IAChannel gIAALChannels[][IA_CH_LAYOUT_MAX_CHANNELS] = {
    {IA_CH_MONO},
    {IA_CH_L2, IA_CH_R2},
    {IA_CH_L5, IA_CH_R5, IA_CH_SL5, IA_CH_SR5, IA_CH_C, IA_CH_LFE},
    {IA_CH_L5, IA_CH_R5, IA_CH_SL5, IA_CH_SR5, IA_CH_HL, IA_CH_HR, IA_CH_C,
     IA_CH_LFE},
    {IA_CH_L5, IA_CH_R5, IA_CH_SL5, IA_CH_SR5, IA_CH_HFL, IA_CH_HFR, IA_CH_HBL,
     IA_CH_HBR, IA_CH_C, IA_CH_LFE},
    {IA_CH_L7, IA_CH_R7, IA_CH_SL7, IA_CH_SR7, IA_CH_BL7, IA_CH_BR7, IA_CH_C,
     IA_CH_LFE},
    {IA_CH_L7, IA_CH_R7, IA_CH_SL7, IA_CH_SR7, IA_CH_BL7, IA_CH_BR7, IA_CH_HL,
     IA_CH_HR, IA_CH_C, IA_CH_LFE},
    {IA_CH_L7, IA_CH_R7, IA_CH_SL7, IA_CH_SR7, IA_CH_BL7, IA_CH_BR7, IA_CH_HFL,
     IA_CH_HFR, IA_CH_HBL, IA_CH_HBR, IA_CH_C, IA_CH_LFE},
    {IA_CH_L3, IA_CH_R3, IA_CH_TL, IA_CH_TR, IA_CH_C, IA_CH_LFE},
    {IA_CH_L2, IA_CH_R2}};

int ia_audio_layer_get_channels(IAChannelLayoutType type, IAChannel *channels,
                                uint32_t count) {
  int ret = 0;
  if (!ia_channel_layout_type_check(type)) {
    return 0;
  }

  ret = ia_channel_layout_get_channels_count(type);
  if (count < ret) {
    return IAMF_ERR_BUFFER_TOO_SMALL;
  }

  for (int c = 0; c < ret; ++c) {
    channels[c] = gIAALChannels[type][c];
  }

  return ret;
}

static const char *gIAChName[] = {
    "unknown", "l7/l5", "r7/r5", "c",   "lfe", "sl7",  "sr7", "bl7",
    "br7",     "hfl",   "hfr",   "hbl", "hbr", "mono", "l2",  "r2",
    "tl",      "tr",    "l3",    "r3",  "sl5", "sr5",  "hl",  "hr"};

const char *ia_channel_name(IAChannel ch) {
  return ch < IA_CH_COUNT ? gIAChName[ch] : "unknown";
}

int bit1_count(uint32_t value) {
  int n = 0;
  for (; value; ++n) {
    value &= (value - 1);
  }
  return n;
}

int iamf_valid_mix_mode(int mode) { return mode >= 0 && mode != 3 && mode < 7; }

const MixFactors mix_factors_mat[] = {
    {1.0, 1.0, 0.707, 0.707, -1},   {0.707, 0.707, 0.707, 0.707, -1},
    {1.0, 0.866, 0.866, 0.866, -1}, {0, 0, 0, 0, 0},
    {1.0, 1.0, 0.707, 0.707, 1},    {0.707, 0.707, 0.707, 0.707, 1},
    {1.0, 0.866, 0.866, 0.866, 1},  {0, 0, 0, 0, 0}};

const MixFactors *iamf_get_mix_factors(int mode) {
  if (iamf_valid_mix_mode(mode)) return &mix_factors_mat[mode];
  return 0;
}
