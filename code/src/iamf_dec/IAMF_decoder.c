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
 * @file IAMF_decoder.c
 * @brief IAMF decoder.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>
#include <math.h>

#include "IAMF_OBU.h"
#include "IAMF_debug.h"
#include "IAMF_decoder.h"
#include "IAMF_decoder_private.h"
#include "IAMF_utils.h"
#include "ae_rdr.h"
#include "bitstream.h"
#include "demixer.h"
#include "fixedp11_5.h"
#include "speex_resampler.h"

#define INVALID_VALUE -1
#define INVALID_ID (uint64_t)(-1)
#define INVALID_TIMESTAMP 0xFFFFFFFF
#define OUTPUT_SAMPLERATE 48000
#define SPEEX_RESAMPLER_QUALITY 4

#define IAMF_DECODER_CONFIG_MIX_PRESENTATION 0x1
#define IAMF_DECODER_CONFIG_OUTPUT_LAYOUT 0x2
#define IAMF_DECODER_CONFIG_PRESENTATION 0x4

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IAMF_DEC"
#define STR(str) _STR(str)
#define _STR(str) #str

#define SR 0
#if SR
extern void iamf_rec_stream_log(int eid, int chs, float *in, int size);
extern void iamf_ren_stream_log(int eid, int chs, float *out, int size);
extern void iamf_mix_stream_log(int chs, float *out, int size);
extern void iamf_stream_log_free();
#endif

/* ----------------------------- Utils ----------------------------- */

static void swap(void **p1, void **p2) {
  void *p = *p2;
  *p2 = *p1;
  *p1 = p;
}

static int64_t time_transform(int64_t t1, int s1, int s2) {
  if (s1 == s2) return t1;
  double r = t1 * s2;
  return r / s1 + 0.5f;
}

/* ----------------------------- Internal methods ------------------ */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
static int16_t FLOAT2INT16(float x) {
  x = x * 32768.f;
  x = MAX(x, -32768.f);
  x = MIN(x, 32767.f);
  return (int16_t)lrintf(x);
}

static int32_t FLOAT2INT24(float x) {
  x = x * 8388608.f;
  x = MAX(x, -8388608.f);
  x = MIN(x, 8388607.f);
  return (int32_t)lrintf(x);
}

static int32_t FLOAT2INT32(float x) {
  x = x * 2147483648.f;
  x = MAX(x, -2147483648.f);
  x = MIN(x, 2147483647.f);
  return (int32_t)lrintf(x);
}

static void iamf_decoder_plane2stride_out(void *dst, const float *src,
                                          int frame_size, int channels,
                                          uint32_t bit_depth) {
  if (bit_depth == 16) {
    int16_t *int16_dst = (int16_t *)dst;
    for (int c = 0; c < channels; ++c) {
      for (int i = 0; i < frame_size; i++) {
        if (src) {
          int16_dst[i * channels + c] = FLOAT2INT16(src[frame_size * c + i]);
        } else {
          int16_dst[i * channels + c] = 0;
        }
      }
    }
  } else if (bit_depth == 24) {
    uint8_t *int24_dst = (uint8_t *)dst;
    for (int c = 0; c < channels; ++c) {
      for (int i = 0; i < frame_size; i++) {
        if (src) {
          int32_t tmp = FLOAT2INT24(src[frame_size * c + i]);
          int24_dst[(i * channels + c) * 3] = tmp & 0xff;
          int24_dst[(i * channels + c) * 3 + 1] = (tmp >> 8) & 0xff;
          int24_dst[(i * channels + c) * 3 + 2] =
              ((tmp >> 16) & 0x7f) | ((tmp >> 24) & 0x80);
        } else {
          int24_dst[(i * channels + c) * 3] = 0;
          int24_dst[(i * channels + c) * 3 + 1] = 0;
          int24_dst[(i * channels + c) * 3 + 2] = 0;
        }
      }
    }
  } else if (bit_depth == 32) {
    int32_t *int32_dst = (int32_t *)dst;
    for (int c = 0; c < channels; ++c) {
      for (int i = 0; i < frame_size; i++) {
        if (src) {
          int32_dst[i * channels + c] = FLOAT2INT32(src[frame_size * c + i]);
        } else {
          int32_dst[i * channels + c] = 0;
        }
      }
    }
  }
}
static void ia_decoder_stride2plane_out_float(void *dst, const float *src,
                                              int frame_size, int channels) {
  float *float_dst = (float *)dst;

  ia_logd("channels %d", channels);
  for (int i = 0; i < frame_size; i++) {
    if (src) {
      for (int c = 0; c < channels; ++c) {
        float_dst[c * frame_size + i] = src[channels * i + c];
      }
    } else {
      for (int c = 0; c < channels; ++c) {
        float_dst[c * frame_size + i] = 0;
      }
    }
  }
}

static void ia_decoder_plane2stride_out_float(void *dst, const float *src,
                                              int frame_size, int channels) {
  float *float_dst = (float *)dst;

  ia_logd("channels %d", channels);
  for (int c = 0; c < channels; ++c) {
    if (src) {
      for (int i = 0; i < frame_size; i++) {
        float_dst[i * channels + c] = src[frame_size * c + i];
      }
    } else {
      for (int i = 0; i < frame_size; i++) {
        float_dst[i * channels + c] = 0;
      }
    }
  }
}

static int iamf_sound_system_valid(IAMF_SoundSystem ss) {
  return ss > SOUND_SYSTEM_INVALID && ss < SOUND_SYSTEM_END;
}

static int iamf_sound_system_channels_count_without_lfe(IAMF_SoundSystem ss) {
  static int ss_channels[] = {2, 5, 7, 9, 10, 10, 13, 22, 7, 11, 9, 5, 1};
  return ss_channels[ss];
}

static int iamf_sound_system_lfe1(IAMF_SoundSystem ss) {
  return ss != SOUND_SYSTEM_A && ss != SOUND_SYSTEM_MONO;
}

static int iamf_sound_system_lfe2(IAMF_SoundSystem ss) {
  return ss == SOUND_SYSTEM_F || ss == SOUND_SYSTEM_H;
}

static uint32_t iamf_sound_system_get_rendering_id(IAMF_SoundSystem ss) {
  static IAMF_SOUND_SYSTEM ss_rids[] = {
      BS2051_A, BS2051_B, BS2051_C, BS2051_D, BS2051_E, BS2051_F, BS2051_G,
      BS2051_H, BS2051_I, BS2051_J, IAMF_712, IAMF_312, IAMF_MONO};
  return ss_rids[ss];
}

static IAChannelLayoutType iamf_sound_system_get_channel_layout(
    IAMF_SoundSystem ss) {
  static IAChannelLayoutType ss_layout[] = {
      IA_CHANNEL_LAYOUT_STEREO,  IA_CHANNEL_LAYOUT_510,
      IA_CHANNEL_LAYOUT_512,     IA_CHANNEL_LAYOUT_514,
      IA_CHANNEL_LAYOUT_INVALID, IA_CHANNEL_LAYOUT_INVALID,
      IA_CHANNEL_LAYOUT_INVALID, IA_CHANNEL_LAYOUT_INVALID,
      IA_CHANNEL_LAYOUT_710,     IA_CHANNEL_LAYOUT_714,
      IA_CHANNEL_LAYOUT_712,     IA_CHANNEL_LAYOUT_312,
      IA_CHANNEL_LAYOUT_MONO};
  return ss_layout[ss];
}

static IAMF_SoundMode iamf_sound_mode_combine(IAMF_SoundMode a,
                                              IAMF_SoundMode b) {
  int out = IAMF_SOUND_MODE_MULTICHANNEL;
  if (a == IAMF_SOUND_MODE_NONE)
    out = b;
  else if (b == IAMF_SOUND_MODE_NONE)
    out = a;
  else if (a == b)
    out = a;
  else if (a == IAMF_SOUND_MODE_BINAURAL || b == IAMF_SOUND_MODE_BINAURAL)
    out = IAMF_SOUND_MODE_NA;

  return out;
}

static uint32_t iamf_layer_layout_get_rendering_id(int layer_layout) {
  static IAMF_SOUND_SYSTEM l_rids[] = {
      IAMF_MONO, IAMF_STEREO, IAMF_51,  IAMF_512, IAMF_514,
      IAMF_71,   IAMF_712,    IAMF_714, IAMF_312, IAMF_BINAURAL};
  return l_rids[layer_layout];
}

static int iamf_layer_layout_lfe1(int layer_layout) {
  return layer_layout > IA_CHANNEL_LAYOUT_STEREO &&
         layer_layout < IA_CHANNEL_LAYOUT_BINAURAL;
}

static IAMF_SoundSystem iamf_layer_layout_convert_sound_system(int layout) {
  static IAMF_SoundSystem layout2ss[] = {
      SOUND_SYSTEM_MONO,    SOUND_SYSTEM_A, SOUND_SYSTEM_B,
      SOUND_SYSTEM_C,       SOUND_SYSTEM_D, SOUND_SYSTEM_I,
      SOUND_SYSTEM_EXT_712, SOUND_SYSTEM_J, SOUND_SYSTEM_EXT_312};
  if (ia_channel_layout_type_check(layout)) return layout2ss[layout];
  return SOUND_SYSTEM_INVALID;
}

static int iamf_layout_lfe1(IAMF_Layout *layout) {
  if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
    return iamf_sound_system_lfe1(layout->sound_system.sound_system);
  }
  return 0;
}

static int iamf_layout_lfe2(IAMF_Layout *layout) {
  if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
    return iamf_sound_system_lfe2(layout->sound_system.sound_system);
  }
  return 0;
}

static int iamf_layout_channels_count(IAMF_Layout *layout) {
  int ret = 0;
  if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
    ret = iamf_sound_system_channels_count_without_lfe(
        layout->sound_system.sound_system);
    ret += iamf_sound_system_lfe1(layout->sound_system.sound_system);
    ret += iamf_sound_system_lfe2(layout->sound_system.sound_system);
    ia_logd("sound system %x, channels %d", layout->sound_system.sound_system,
            ret);
  } else if (layout->type == IAMF_LAYOUT_TYPE_BINAURAL) {
    ret = 2;
    ia_logd("binaural channels %d", ret);
  }

  return ret;
}

static int iamf_layout_lfe_check(IAMF_Layout *layout) {
  int ret = 0;
  ret += iamf_layout_lfe1(layout);
  ret += iamf_layout_lfe2(layout);
  return !!ret;
}

static void iamf_layout_reset(IAMF_Layout *layout) {
  if (layout) memset(layout, 0, sizeof(IAMF_Layout));
}

static int iamf_layout_copy(IAMF_Layout *dst, IAMF_Layout *src) {
  memcpy(dst, src, sizeof(IAMF_Layout));
  return IAMF_OK;
}

static int iamf_layout_copy2(IAMF_Layout *dst, TargetLayout *src) {
  dst->type = src->type;
  if (src->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
    SoundSystemLayout *layout = SOUND_SYSTEM_LAYOUT(src);
    dst->sound_system.sound_system = layout->sound_system;
  }
  return IAMF_OK;
}

static void iamf_layout_dump(IAMF_Layout *layout) {
  if (layout) {
    ia_logt("layout type %d", layout->type);
    if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
      ia_logt("sound system %d", layout->sound_system.sound_system);
    }
  }
}

static void iamf_layout_info_free(LayoutInfo *layout) {
  if (layout) {
    if (layout->sp.sp_layout.predefined_sp)
      free(layout->sp.sp_layout.predefined_sp);
    iamf_layout_reset(&layout->layout);
    free(layout);
  }
}

static IAMF_SoundSystem iamf_layout_get_sound_system(IAMF_Layout *layout) {
  IAMF_SoundSystem ss = SOUND_SYSTEM_INVALID;
  if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION)
    ss = layout->sound_system.sound_system;
  return ss;
}

static IAMF_SoundMode iamf_layout_get_sound_mode(IAMF_Layout *layout) {
  IAMF_SoundMode mode = IAMF_SOUND_MODE_NONE;
  if (layout->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
    if (layout->sound_system.sound_system == SOUND_SYSTEM_A)
      mode = IAMF_SOUND_MODE_STEREO;
    else
      mode = IAMF_SOUND_MODE_MULTICHANNEL;
  } else if (layout->type == IAMF_LAYOUT_TYPE_BINAURAL)
    mode = IAMF_SOUND_MODE_BINAURAL;

  return mode;
}

static uint32_t iamf_recon_channels_get_flags(IAChannelLayoutType l1,
                                              IAChannelLayoutType l2) {
  uint32_t s1, s2, t1, t2, flags;

  if (l1 == l2) return 0;

  s1 = ia_channel_layout_get_category_channels_count(l1, IA_CH_CATE_SURROUND);
  s2 = ia_channel_layout_get_category_channels_count(l2, IA_CH_CATE_SURROUND);
  t1 = ia_channel_layout_get_category_channels_count(l1, IA_CH_CATE_TOP);
  t2 = ia_channel_layout_get_category_channels_count(l2, IA_CH_CATE_TOP);
  flags = 0;

  if (s1 != s2) {
    if (s2 <= 3) {
      flags |= RSHIFT(IA_CH_RE_L);
      flags |= RSHIFT(IA_CH_RE_R);
    } else if (s2 == 5) {
      flags |= RSHIFT(IA_CH_RE_LS);
      flags |= RSHIFT(IA_CH_RE_RS);
    } else if (s2 == 7) {
      flags |= RSHIFT(IA_CH_RE_LB);
      flags |= RSHIFT(IA_CH_RE_RB);
    }
  }

  if (t2 != t1 && t2 == 4) {
    flags |= RSHIFT(IA_CH_RE_LTB);
    flags |= RSHIFT(IA_CH_RE_RTB);
  }

  if (s2 == 5 && t1 && t2 == t1) {
    flags |= RSHIFT(IA_CH_RE_LTF);
    flags |= RSHIFT(IA_CH_RE_RTF);
  }

  return flags;
}

static void iamf_recon_channels_order_update(IAChannelLayoutType layout,
                                             IAMF_ReconGain *re) {
  int chs = 0;
  static IAReconChannel recon_channel_order[] = {
      IA_CH_RE_L,  IA_CH_RE_C,   IA_CH_RE_R,   IA_CH_RE_LS,
      IA_CH_RE_RS, IA_CH_RE_LTF, IA_CH_RE_RTF, IA_CH_RE_LB,
      IA_CH_RE_RB, IA_CH_RE_LTB, IA_CH_RE_RTB, IA_CH_RE_LFE};

  static IAChannel channel_layout_map[IA_CHANNEL_LAYOUT_COUNT][IA_CH_RE_COUNT] =
      {{IA_CH_MONO, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID,
        IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID,
        IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID},
       {IA_CH_L2, IA_CH_INVALID, IA_CH_R2, IA_CH_INVALID, IA_CH_INVALID,
        IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID,
        IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID},
       {IA_CH_L5, IA_CH_C, IA_CH_R5, IA_CH_SL5, IA_CH_SR5, IA_CH_INVALID,
        IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID,
        IA_CH_INVALID, IA_CH_LFE},
       {IA_CH_L5, IA_CH_C, IA_CH_R5, IA_CH_SL5, IA_CH_SR5, IA_CH_HL, IA_CH_HR,
        IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_LFE},
       {IA_CH_L5, IA_CH_C, IA_CH_R5, IA_CH_SL5, IA_CH_SR5, IA_CH_HFL, IA_CH_HFR,
        IA_CH_INVALID, IA_CH_INVALID, IA_CH_HBL, IA_CH_HBR, IA_CH_LFE},
       {IA_CH_L7, IA_CH_C, IA_CH_R7, IA_CH_SL7, IA_CH_SR7, IA_CH_INVALID,
        IA_CH_INVALID, IA_CH_BL7, IA_CH_BR7, IA_CH_INVALID, IA_CH_INVALID,
        IA_CH_LFE},
       {IA_CH_L7, IA_CH_C, IA_CH_R7, IA_CH_SL7, IA_CH_SR7, IA_CH_HL, IA_CH_HR,
        IA_CH_BL7, IA_CH_BR7, IA_CH_INVALID, IA_CH_INVALID, IA_CH_LFE},
       {IA_CH_L7, IA_CH_C, IA_CH_R7, IA_CH_SL7, IA_CH_SR7, IA_CH_HFL, IA_CH_HFR,
        IA_CH_BL7, IA_CH_BR7, IA_CH_HBL, IA_CH_HBR, IA_CH_LFE},
       {IA_CH_L3, IA_CH_C, IA_CH_R3, IA_CH_INVALID, IA_CH_INVALID, IA_CH_TL,
        IA_CH_TR, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID, IA_CH_INVALID,
        IA_CH_LFE}};

#define RECON_CHANNEL_FLAG(c) RSHIFT(c)

  for (int c = 0; c < IA_CH_RE_COUNT; ++c) {
    if (re->flags & RECON_CHANNEL_FLAG(recon_channel_order[c]))
      re->order[chs++] = channel_layout_map[layout][recon_channel_order[c]];
  }
}

static int iamf_channel_layout_get_new_channels(IAChannelLayoutType last,
                                                IAChannelLayoutType cur,
                                                IAChannel *new_chs,
                                                uint32_t count) {
  uint32_t chs = 0;

  /**
   * In ChannelGroup for Channel audio: The order conforms to following rules:
   *
   * @ Coupled Substream(s) comes first and followed by non-coupled
   * Substream(s).
   * @ Coupled Substream(s) for surround channels comes first and followed by
   * one(s) for top channels.
   * @ Coupled Substream(s) for front channels comes first and followed by
   * one(s) for side, rear and back channels.
   * @ Coupled Substream(s) for side channels comes first and followed by one(s)
   * for rear channels.
   * @ Center channel comes first and followed by LFE and followed by the other
   * one.
   * */

  if (last == IA_CHANNEL_LAYOUT_INVALID) {
    chs = ia_audio_layer_get_channels(cur, new_chs, count);
  } else {
    uint32_t s1 = ia_channel_layout_get_category_channels_count(
        last, IA_CH_CATE_SURROUND);
    uint32_t s2 =
        ia_channel_layout_get_category_channels_count(cur, IA_CH_CATE_SURROUND);
    uint32_t t1 =
        ia_channel_layout_get_category_channels_count(last, IA_CH_CATE_TOP);
    uint32_t t2 =
        ia_channel_layout_get_category_channels_count(cur, IA_CH_CATE_TOP);

    if (s1 < 5 && 5 <= s2) {
      new_chs[chs++] = IA_CH_L5;
      new_chs[chs++] = IA_CH_R5;
      ia_logd("new channels : l5/r5(l7/r7)");
    }
    if (s1 < 7 && 7 <= s2) {
      new_chs[chs++] = IA_CH_SL7;
      new_chs[chs++] = IA_CH_SR7;
      ia_logd("new channels : sl7/sr7");
    }
    if (t2 != t1 && t2 == 4) {
      new_chs[chs++] = IA_CH_HFL;
      new_chs[chs++] = IA_CH_HFR;
      ia_logd("new channels : hfl/hfr");
    }
    if (t2 - t1 == 4) {
      new_chs[chs++] = IA_CH_HBL;
      new_chs[chs++] = IA_CH_HBR;
      ia_logd("new channels : hbl/hbr");
    } else if (!t1 && t2 - t1 == 2) {
      if (s2 < 5) {
        new_chs[chs++] = IA_CH_TL;
        new_chs[chs++] = IA_CH_TR;
        ia_logd("new channels : tl/tr");
      } else {
        new_chs[chs++] = IA_CH_HL;
        new_chs[chs++] = IA_CH_HR;
        ia_logd("new channels : hl/hr");
      }
    }

    if (s1 < 3 && 3 <= s2) {
      new_chs[chs++] = IA_CH_C;
      new_chs[chs++] = IA_CH_LFE;
      ia_logd("new channels : c/lfe");
    }
    if (s1 < 2 && 2 <= s2) {
      new_chs[chs++] = IA_CH_L2;
      ia_logd("new channel : l2");
    }
  }

  if (chs > count) {
    ia_loge("too much new channels %d, we only need less than %d channels", chs,
            count);
    chs = IAMF_ERR_BUFFER_TOO_SMALL;
  }
  return chs;
}

static IAChannel iamf_output_gain_channel_map(IAChannelLayoutType type,
                                              IAOutputGainChannel gch) {
  IAChannel ch = IA_CH_INVALID;
  switch (gch) {
    case IA_CH_GAIN_L: {
      switch (type) {
        case IA_CHANNEL_LAYOUT_MONO:
          ch = IA_CH_MONO;
          break;
        case IA_CHANNEL_LAYOUT_STEREO:
          ch = IA_CH_L2;
          break;
        case IA_CHANNEL_LAYOUT_312:
          ch = IA_CH_L3;
          break;
        default:
          break;
      }
    } break;

    case IA_CH_GAIN_R: {
      switch (type) {
        case IA_CHANNEL_LAYOUT_STEREO:
          ch = IA_CH_R2;
          break;
        case IA_CHANNEL_LAYOUT_312:
          ch = IA_CH_R3;
          break;
        default:
          break;
      }
    } break;

    case IA_CH_GAIN_LS: {
      if (ia_channel_layout_get_category_channels_count(
              type, IA_CH_CATE_SURROUND) == 5) {
        ch = IA_CH_SL5;
      }
    } break;

    case IA_CH_GAIN_RS: {
      if (ia_channel_layout_get_category_channels_count(
              type, IA_CH_CATE_SURROUND) == 5) {
        ch = IA_CH_SR5;
      }
    } break;

    case IA_CH_GAIN_LTF: {
      if (ia_channel_layout_get_category_channels_count(
              type, IA_CH_CATE_SURROUND) < 5) {
        ch = IA_CH_TL;
      } else {
        ch = IA_CH_HL;
      }
    } break;

    case IA_CH_GAIN_RTF: {
      if (ia_channel_layout_get_category_channels_count(
              type, IA_CH_CATE_SURROUND) < 5) {
        ch = IA_CH_TR;
      } else {
        ch = IA_CH_HR;
      }
    } break;
    default:
      break;
  }

  return ch;
}

/* ----------------------------- Internal Interfaces--------------- */

static uint32_t iamf_decoder_internal_read_descriptors_OBUs(
    IAMF_DecoderHandle handle, const uint8_t *data, uint32_t size);
static int32_t iamf_decoder_internal_add_descrptor_OBU(
    IAMF_DecoderHandle handle, IAMF_OBU *obu);
static IAMF_StreamDecoder *iamf_stream_decoder_open(IAMF_Stream *stream,
                                                    IAMF_CodecConf *conf);
static uint32_t iamf_set_stream_info(IAMF_DecoderHandle handle);
static int iamf_decoder_internal_deliver(IAMF_DecoderHandle handle,
                                         IAMF_Frame *obj);
static int iamf_stream_scale_decoder_decode(IAMF_StreamDecoder *decoder,
                                            float *pcm);
static int iamf_stream_scale_demixer_configure(IAMF_StreamDecoder *decoder);
static int32_t iamf_stream_scale_decoder_demix(IAMF_StreamDecoder *decoder,
                                               float *src, float *dst,
                                               uint32_t frame_size);
static int iamf_stream_ambisonics_decoder_decode(IAMF_StreamDecoder *decoder,
                                                 float *pcm);

/* >>>>>>>>>>>>>>>>>> DATABASE >>>>>>>>>>>>>>>>>> */

static void iamf_database_reset(IAMF_DataBase *db);
static IAMF_CodecConf *iamf_database_get_codec_conf(IAMF_DataBase *db,
                                                    uint64_t cid);
static ElementItem *iamf_database_element_get_item(IAMF_DataBase *db,
                                                   uint64_t eid);

static void mix_gain_unit_free(MixGainUnit *u) {
  if (u) {
    IAMF_FREE(u->gains);
    free(u);
  }
}

static int mix_gain_bezier_linear(float s, float e, int d, int o, uint32_t l,
                                  float *g) {
  int oe = o + l;
  for (int i = o, k = 0; i < oe; ++i, ++k) g[k] = s + (e - s) * i / d;

  return IAMF_OK;
}

static int mix_gain_bezier_quad(float s, float e, int d, float c, int ct, int o,
                                uint32_t l, float *g) {
  int oe = o + l;
  int64_t alpha = d - 2 * ct;
  float a = 1.0f;

  for (int i = o, k = 0; i < oe; ++i, ++k) {
    if (alpha) {
      a = (sqrt(pow(ct, 2) + alpha * i) - ct) / alpha;
    } else {
      a = i;
      a /= (2 * ct);
    }
    g[k] = (s + e - 2 * c) * pow(a, 2) + 2 * a * (c - s) + s;
  }

  return IAMF_OK;
}

static void iamf_object_free(void *obj) { IAMF_object_free(IAMF_OBJ(obj)); }

static ObjectSet *iamf_object_set_new(IAMF_Free func) {
  ObjectSet *s = IAMF_MALLOCZ(ObjectSet, 1);
  if (s) {
    s->objFree = func;
  }

  return s;
}

static void iamf_object_set_free(ObjectSet *s) {
  if (s) {
    if (s->objFree) {
      for (int i = 0; i < s->count; ++i) s->objFree(s->items[i]);
      if (s->items) free(s->items);
    }
    free(s);
  }
}

#define CAP_DEFAULT 6
static int iamf_object_set_add(ObjectSet *s, void *item) {
  if (!item) return IAMF_ERR_BAD_ARG;

  if (s->count == s->capacity) {
    void **cap = 0;
    if (!s->count) {
      cap = IAMF_MALLOCZ(void *, CAP_DEFAULT);
    } else {
      cap = IAMF_REALLOC(void *, s->items, s->capacity + CAP_DEFAULT);
    }
    if (!cap) return IAMF_ERR_ALLOC_FAIL;
    s->items = cap;
    s->capacity += CAP_DEFAULT;
  }

  s->items[s->count++] = item;
  return IAMF_OK;
}

static int iamf_codec_conf_get_sampling_rate(IAMF_CodecConf *c) {
  uint32_t cid = iamf_codec_4cc_get_codecID(c->codec_id);
  if (cid == IAMF_CODEC_PCM) {
    if (c->decoder_conf_size < 6) return IAMF_ERR_BAD_ARG;
    return reads32be(c->decoder_conf, 2);
  } else if (cid == IAMF_CODEC_OPUS) {
    if (c->decoder_conf_size < 8) return IAMF_ERR_BAD_ARG;
    return reads32be(c->decoder_conf, 4);
  } else if (cid == IAMF_CODEC_AAC) {
    BitStream b;
    int ret, type;
    static int sf[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                       16000, 12000, 11025, 8000,  7350,  0,     0,     0};

    /* DecoderConfigDescriptor (14 bytes) + DecSpecificInfoTag (1 byte) */
    if (c->decoder_conf_size < 16) return IAMF_ERR_BAD_ARG;
    bs(&b, c->decoder_conf + 15, c->decoder_conf_size - 15);

    type = bs_get32b(&b, 5);
    if (type == 31) bs_get32b(&b, 6);

    ret = bs_get32b(&b, 4);
    if (ret == 0xf)
      return bs_get32b(&b, 24);
    else
      return sf[ret];
  } else if (cid == IAMF_CODEC_FLAC) {
    BitStream b;
    int last, type, size;
    bs(&b, c->decoder_conf, c->decoder_conf_size);
    while (1) {
      last = bs_get32b(&b, 1);
      type = bs_get32b(&b, 7);
      size = bs_get32b(&b, 24);
      if (!type) {
        bs_skip(&b, 80);
        return bs_get32b(&b, 20);
      } else
        bs_skip(&b, size * 8);

      if (last) break;
    }
  }
  return IAMF_ERR_BAD_ARG;
}

static void iamf_parameter_item_clear_segments(ParameterItem *pi) {
  if (pi && pi->value.params) {
    queue_t *q = pi->value.params;
    while (queue_length(q) > 0) {
      ParameterSegment *seg = queue_pop(q);
      IAMF_parameter_segment_free(seg);
    }
  }
}

static void iamf_parameter_item_free(void *e) {
  ParameterItem *pi = (ParameterItem *)e;
  iamf_parameter_item_clear_segments(pi);
  if (pi && pi->value.params) queue_free(pi->value.params);
  IAMF_FREE(pi);
}

static void iamf_database_viewer_reset(Viewer *v) {
  if (v->items) {
    free_tp ff = v->freeF;
    if (!v->freeF) ff = free;
    for (int i = 0; i < v->count; ++i) ff(v->items[i]);
    free(v->items);
  }
  v->count = 0;
  v->items = 0;
}

static ParameterItem *iamf_database_parameter_viewer_get_item(Viewer *viewer,
                                                              uint64_t pid) {
  ParameterItem *pi = 0, *vpi;
  for (int i = 0; i < viewer->count; ++i) {
    vpi = (ParameterItem *)viewer->items[i];
    if (vpi->id == pid) {
      pi = vpi;
      break;
    }
  }
  return pi;
}

static ParameterItem *iamf_database_parameter_get_item(IAMF_DataBase *db,
                                                       uint64_t pid) {
  return iamf_database_parameter_viewer_get_item(&db->pViewer, pid);
}

static int iamf_database_parameter_check_timestamp(IAMF_DataBase *db,
                                                   uint64_t pid, uint64_t pts) {
  ParameterItem *pi =
      iamf_database_parameter_viewer_get_item(&db->pViewer, pid);
  if (!pi) return 0;
  if (pts > pi->timestamp && pts <= pi->timestamp + pi->duration) return 1;
  ia_logw("pid %" PRIu64 ": request pts %" PRIu64 " vs parameter pts %" PRIu64
          ", duration %" PRIu64,
          pid, pts, pi->timestamp, pi->duration);
  return 0;
}

static ParameterSegment *iamf_database_parameter_get_segment(IAMF_DataBase *db,
                                                             uint64_t pid,
                                                             uint64_t pts) {
  ParameterItem *pi =
      iamf_database_parameter_viewer_get_item(&db->pViewer, pid);
  ParameterSegment *seg = 0, *s = 0;
  uint64_t start = 0;
  int count = 0;

  if (!pi) return 0;
  if (!iamf_database_parameter_check_timestamp(db, pi->id, pts)) {
    ia_logw("Don't receive parameter %" PRIu64 " at %" PRIu64
            ", current parameter at %" PRIu64,
            pi->id, pts, pi->timestamp);
    return 0;
  } else
    start = pts - pi->timestamp;

  count = pi->value.params ? queue_length(pi->value.params) : 0;
  for (int i = 0; i < count; ++i) {
    s = queue_take(pi->value.params, i);
    if (!s) break;
    if (start < s->segment_interval) {
      seg = s;
      break;
    } else {
      start -= s->segment_interval;
    }
  }
  return seg;
}

static int iamf_database_parameter_get_demix_mode(IAMF_DataBase *db,
                                                  uint64_t pid, uint64_t pts) {
  DemixingSegment *dseg =
      (DemixingSegment *)iamf_database_parameter_get_segment(db, pid, pts);
  return dseg ? dseg->demixing_mode : IAMF_ERR_INTERNAL;
}

static ReconGainList *iamf_database_parameter_get_recon_gain_list(
    IAMF_DataBase *db, uint64_t pid, uint64_t pts) {
  ReconGainSegment *rg =
      (ReconGainSegment *)iamf_database_parameter_get_segment(db, pid, pts);
  return rg ? &rg->list : 0;
}

static MixGainUnit *iamf_database_parameter_get_mix_gain_unit(
    IAMF_DataBase *db, uint64_t pid, uint64_t pt, int duration, int rate) {
  ParameterItem *pi = 0;
  MixGainUnit *mgu = 0;
  uint64_t start = 0;
  float ratio = 1.f;
  int use_default = 0;

  pi = iamf_database_parameter_viewer_get_item(&db->pViewer, pid);
  if (!pi) return 0;

  ia_logd("pts %" PRIu64 ", parameter id %" PRIu64 ", pts %" PRIu64
          ", duration %" PRId64,
          pt, pid, pi->timestamp, pi->duration);
  if (pt < pi->timestamp) {
    ia_logw("Don't receive mix gain parameter %" PRIu64 " at %" PRIu64
            ", current parameter at "
            "%" PRIu64,
            pi->id, pt, pi->timestamp);
    use_default = 1;
  } else
    start = pt - pi->timestamp;

  mgu = IAMF_MALLOCZ(MixGainUnit, 1);
  if (!mgu) return 0;

  if (pi->value.mix_gain.use_default || use_default) {
    ia_logd("use default mix gain %f", pi->value.mix_gain.default_mix_gain);
    mgu->constant_gain = pi->value.mix_gain.default_mix_gain;
    mgu->count = duration;
  } else {
    int64_t sgd = 0;
    int64_t minterval = 0;
    int left = duration;
    MixGainSegment *seg = 0;

    if (!pi->param_base || !pi->value.params) {
      mix_gain_unit_free(mgu);
      return 0;
    }

    if (rate != pi->param_base->rate)
      ratio = (rate + 0.1f) / pi->param_base->rate;

    for (int i = 0; i < queue_length(pi->value.params); ++i) {
      seg = (MixGainSegment *)queue_take(pi->value.params, i);
      if (!seg) {
        ia_loge("Can't find %d-th segment", i);
        break;
      }
      minterval = seg->seg.segment_interval * ratio;
      sgd += minterval;
      if (start < sgd) {
        ia_logd("find segment %d, s(%" PRIu64 ") ~ e(%" PRIu64 ")", i,
                sgd - minterval, sgd);
        if (seg->mix_gain.animated_type == PARAMETER_ANIMATED_TYPE_STEP) {
          if (!mgu->count && start + duration <= sgd) {
            mgu->constant_gain = seg->mix_gain_f.start;
            mgu->count = duration;
            ia_logd("use constant mix gain %f", seg->mix_gain_f.start);
          } else if (!mgu->count) {
            mgu->gains = IAMF_MALLOC(float, duration);
            if (!mgu->gains) {
              ia_loge("Fail to allocate gains of mix gain unit.");
              mix_gain_unit_free(mgu);
              return 0;
            }
            mgu->count = sgd - start;
            for (int i = 0; i < mgu->count; ++i)
              mgu->gains[i] = seg->mix_gain_f.start;
            start = sgd;
            ia_logd("step-1: get %d gains", mgu->count);
          } else {
            int e = mgu->count + minterval;
            if (e >= duration)
              e = duration;
            else {
              start = sgd;
              e = mgu->count + minterval;
            }
            for (int i = mgu->count; i < e; ++i)
              mgu->gains[i] = seg->mix_gain_f.start;
            ia_logd("step-2: get %d gains", e - mgu->count);
            mgu->count = e;
          }
        } else {
          int off = 0;
          int ss = sgd - minterval;
          int d = 0;
          off = start - ss;
          if (!mgu->gains) {
            mgu->gains = IAMF_MALLOC(float, duration);
            if (!mgu->gains) {
              ia_loge("Fail to allocate gains of mix gain unit.");
              mix_gain_unit_free(mgu);
              return 0;
            }
          }

          if (start + left <= sgd) {
            d = left;
          } else {
            d = sgd - start;
            start = sgd;
            left -= d;
          }
          if (seg->mix_gain.animated_type == PARAMETER_ANIMATED_TYPE_LINEAR)
            mix_gain_bezier_linear(seg->mix_gain_f.start, seg->mix_gain_f.end,
                                   minterval, off, d, mgu->gains + mgu->count);
          else
            mix_gain_bezier_quad(
                seg->mix_gain_f.start, seg->mix_gain_f.end, minterval,
                seg->mix_gain_f.control,
                seg->mix_gain_f.control_relative_time * (minterval + .1f), off,
                d, mgu->gains + mgu->count);
          mgu->count += d;
          ia_logd("not step: get %d gains", d);
        }
      }

      if (mgu->count == duration) break;
    }
  }

  return mgu;
}

static int iamf_database_parameter_add_item(IAMF_DataBase *db,
                                            ParameterBase *base,
                                            uint64_t parent_id, int rate) {
  Viewer *pv = &db->pViewer;
  ParameterItem *pi = 0;
  ParameterItem **pis = 0;
  ElementItem *ei = 0;
  uint64_t pid, type;

  if (!base) return IAMF_ERR_BAD_ARG;

  pid = base->id;
  type = base->type;
  pi = iamf_database_parameter_viewer_get_item(pv, pid);

  if (pi) {
    ia_logd("parameter %" PRIu64 " is already in database.", pid);
    return IAMF_OK;
  }

  pis = IAMF_REALLOC(ParameterItem *, pv->items, pv->count + 1);
  if (!pis) return IAMF_ERR_ALLOC_FAIL;
  pv->items = (void **)pis;
  pis[pv->count] = 0;

  pi = IAMF_MALLOCZ(ParameterItem, 1);
  if (!pi) return IAMF_ERR_ALLOC_FAIL;

  pi->value.params = queue_new();
  if (!pi->value.params) {
    free(pi);
    return IAMF_ERR_ALLOC_FAIL;
  }

  // use default mix gain.
  if (type == IAMF_PARAMETER_TYPE_MIX_GAIN) pi->value.mix_gain.use_default = 1;

  pis[pv->count++] = pi;
  ia_logd("add parameter item %p, its id %" PRIu64 ", and count is %d", pi, pid,
          pv->count);

  pi->id = pid;
  pi->type = base->type;
  pi->parent_id = parent_id;
  pi->param_base = base;
  pi->rate = rate;

  ei = iamf_database_element_get_item(db, parent_id);
  if (ei) {
    if (type == IAMF_PARAMETER_TYPE_DEMIXING) {
      ei->demixing = pi;
    } else if (type == IAMF_PARAMETER_TYPE_RECON_GAIN) {
      ei->reconGain = pi;
    }
  }
  return IAMF_OK;
}

static int iamf_database_parameter_add(IAMF_DataBase *db, IAMF_Object *obj) {
  Viewer *pv = &db->pViewer;
  IAMF_Parameter *p = (IAMF_Parameter *)obj;
  ParameterItem *pi = 0;

  pi = iamf_database_parameter_viewer_get_item(pv, p->id);
  if (pi) {
    if (obj->flags & IAMF_OBU_FLAG_REDUNDANT && pi->duration > 0) {
      ia_logd("Ignore redundant parameter obu with id %" PRIu64 ".", p->id);
      return IAMF_OK;
    }

    if (pi->type == IAMF_PARAMETER_TYPE_MIX_GAIN &&
        pi->value.mix_gain.use_default) {
      pi->value.mix_gain.use_default = 0;
    }

    for (int i = 0; i < p->nb_segments; ++i) {
      queue_push(pi->value.params, p->segments[i]);
      pi->duration += p->segments[i]->segment_interval;
      p->segments[i] = 0;
    }
    ia_logd("New p(%" PRIu64 ")ts(s:%" PRIu64 ", e: %" PRIu64 ").", pi->id,
            pi->timestamp, pi->timestamp + pi->duration);
  } else {
    ia_logw("Can not find parameter item for paramter (%" PRIu64 ")", p->id);
  }

  return IAMF_OK;
}

static ParameterBase *iamf_database_parameter_viewer_get_parmeter_base(
    IAMF_DataBase *db, uint64_t pid) {
  ParameterItem *pi =
      iamf_database_parameter_viewer_get_item(&db->pViewer, pid);
  return pi ? pi->param_base : 0;
}

static int iamf_database_parameters_clear_segments(IAMF_DataBase *db) {
  ParameterItem *pi = 0;
  for (int i = 0; i < db->pViewer.count; ++i) {
    pi = (ParameterItem *)db->pViewer.items[i];
    iamf_parameter_item_clear_segments(pi);
  }
  return IAMF_OK;
}

static int iamf_database_parameters_time_elapse(IAMF_DataBase *db,
                                                uint64_t duration,
                                                uint32_t rate) {
  Viewer *pv = &db->pViewer;
  ParameterItem *pi;
  ParameterSegment *seg;

  for (int i = 0; i < pv->count; ++i) {
    pi = (ParameterItem *)pv->items[i];
    ia_logd("S: pid %" PRIu64 " pts %" PRIu64 ", duration %" PRIu64
            ", elapsed %" PRIu64 ", rate %" PRIu64 ", elapse %" PRIu64
            ", rate %u",
            pi->id, pi->timestamp, pi->duration, pi->elapse,
            pi->param_base->rate, duration, rate);

    if (pi->value.params) {
      pi->elapse += time_transform(duration, rate, pi->param_base->rate);
      while (1) {
        // ia_logd("length %d", queue_length(pi->value.params));
        seg = (ParameterSegment *)queue_take(pi->value.params, 0);
        if (seg && seg->segment_interval <= pi->elapse) {
          pi->timestamp += seg->segment_interval;
          pi->duration -= seg->segment_interval;
          pi->elapse -= seg->segment_interval;
          // ia_logd("pi %p, pid %" PRIu64", pop segment %p", pi, pi->id, seg);
          queue_pop(pi->value.params);
          IAMF_parameter_segment_free(seg);
        } else {
          ia_logd("E: pid %" PRIu64 " pts %" PRIu64 ", duration %" PRIu64
                  ", elapsed %" PRIu64,
                  pi->id, pi->timestamp, pi->duration, pi->elapse);
          break;
        }
      }
    }
  }
  return IAMF_OK;
}

static int iamf_database_element_add(IAMF_DataBase *db, IAMF_Object *obj) {
  int ret = IAMF_OK;
  int rate = 0;
  ElementItem *ei = 0;
  ElementItem **eItem = 0;
  Viewer *v = &db->eViewer;
  IAMF_Element *e = (IAMF_Element *)obj;

  ei = iamf_database_element_get_item(db, e->element_id);
  if (ei) {
    ia_logd("element %" PRIu64 " is already in database.", e->element_id);
    return IAMF_OK;
  }

  eItem = IAMF_REALLOC(ElementItem *, v->items, v->count + 1);
  if (!eItem) return IAMF_ERR_ALLOC_FAIL;
  eItem[v->count] = 0;

  v->items = (void **)eItem;
  ret = iamf_object_set_add(db->element, (void *)obj);
  if (ret != IAMF_OK) return ret;

  ei = IAMF_MALLOCZ(ElementItem, 1);
  if (!ei) return IAMF_ERR_ALLOC_FAIL;

  v->items[v->count++] = ei;
  ei->id = e->element_id;
  ei->element = IAMF_ELEMENT(obj);
  ei->codecConf = iamf_database_get_codec_conf(db, e->codec_config_id);
  rate = iamf_codec_conf_get_sampling_rate(ei->codecConf);

  for (int i = 0; i < e->nb_parameters; ++i) {
    iamf_database_parameter_add_item(db, e->parameters[i], ei->id, rate);
  }

  return ret;
}

int iamf_database_init(IAMF_DataBase *db) {
  memset(db, 0, sizeof(IAMF_DataBase));

  db->codecConf = iamf_object_set_new(iamf_object_free);
  db->element = iamf_object_set_new(iamf_object_free);
  db->mixPresentation = iamf_object_set_new(iamf_object_free);
  db->eViewer.freeF = free;
  db->pViewer.freeF = iamf_parameter_item_free;

  if (!db->codecConf || !db->element || !db->mixPresentation) {
    iamf_database_reset(db);
    return IAMF_ERR_ALLOC_FAIL;
  }
  return 0;
}

void iamf_database_reset(IAMF_DataBase *db) {
  if (db) {
    if (db->version) iamf_object_free(db->version);
    if (db->codecConf) iamf_object_set_free(db->codecConf);
    if (db->element) iamf_object_set_free(db->element);
    if (db->mixPresentation) iamf_object_set_free(db->mixPresentation);

    iamf_database_viewer_reset(&db->eViewer);
    iamf_database_viewer_reset(&db->pViewer);

    memset(db, 0, sizeof(IAMF_DataBase));
  }
}

static int iamf_database_add_object(IAMF_DataBase *db, IAMF_Object *obj) {
  int ret = IAMF_OK;
  if (!obj) return IAMF_ERR_BAD_ARG;

  switch (obj->type) {
    case IAMF_OBU_SEQUENCE_HEADER:
      if (db->version) {
        ia_logw("WARNING : Receive Multiple START CODE OBUs !!!");
        free(db->version);
      }
      db->version = obj;
      break;
    case IAMF_OBU_CODEC_CONFIG:
      ret = iamf_object_set_add(db->codecConf, (void *)obj);
      break;
    case IAMF_OBU_AUDIO_ELEMENT:
      ret = iamf_database_element_add(db, obj);
      break;
    case IAMF_OBU_MIX_PRESENTATION:
      ret = iamf_object_set_add(db->mixPresentation, (void *)obj);
      break;
    case IAMF_OBU_PARAMETER_BLOCK:
      ret = iamf_database_parameter_add(db, obj);
      IAMF_object_free(obj);
      break;
    default:
      ia_logd("IAMF Object %s (%d) is not needed in database.",
              IAMF_OBU_type_string(obj->type), obj->type);
      IAMF_object_free(obj);
  }
  return ret;
}

IAMF_CodecConf *iamf_database_get_codec_conf(IAMF_DataBase *db, uint64_t cid) {
  IAMF_CodecConf *ret = 0;

  if (db->codecConf) {
    IAMF_CodecConf *c = 0;
    for (uint32_t i = 0; i < db->codecConf->count; ++i) {
      c = (IAMF_CodecConf *)db->codecConf->items[i];
      if (c->codec_conf_id == cid) {
        ret = c;
        break;
      }
    }
  }
  return ret;
}

static IAMF_Element *iamf_database_get_element(IAMF_DataBase *db,
                                               uint64_t eid) {
  ElementItem *ei = iamf_database_element_get_item(db, eid);
  return ei ? ei->element : 0;
}

static IAMF_Element *iamf_database_get_element_by_parameterID(IAMF_DataBase *db,
                                                              uint64_t pid) {
  IAMF_Element *elem = 0;
  IAMF_Element *e = 0;
  ElementItem *ei = 0, *vei;
  for (int i = 0; i < db->eViewer.count; ++i) {
    vei = (ElementItem *)db->eViewer.items[i];
    e = IAMF_ELEMENT(vei->element);
    for (int p = 0; p < e->nb_parameters; ++p) {
      if (pid == e->parameters[p]->id) {
        elem = e;
        break;
      }
    }
    if (!elem) {
      ei = iamf_database_element_get_item(db, e->element_id);
      if (ei && ei->mixGain && ei->mixGain->id == pid) {
        elem = e;
        break;
      }
    }
  }
  return elem;
}

static IAMF_MixPresentation *iamf_database_get_mix_presentation(
    IAMF_DataBase *db, uint64_t id) {
  IAMF_MixPresentation *ret = 0, *obj;
  if (db && db->mixPresentation && db->mixPresentation->count > 0) {
    for (int i = 0; i < db->mixPresentation->count; ++i) {
      obj = IAMF_MIX_PRESENTATION(db->mixPresentation->items[i]);
      if (obj && obj->mix_presentation_id == id) {
        ret = obj;
        break;
      }
    }
  }
  return ret;
}

ElementItem *iamf_database_element_get_item(IAMF_DataBase *db, uint64_t eid) {
  ElementItem *ei = 0, *vei;
  for (int i = 0; i < db->eViewer.count; ++i) {
    vei = (ElementItem *)db->eViewer.items[i];
    if (vei->id == eid) {
      ei = vei;
      break;
    }
  }
  return ei;
}

static int iamf_database_element_get_substream_index(IAMF_DataBase *db,
                                                     uint64_t element_id,
                                                     uint64_t substream_id) {
  IAMF_Element *obj = iamf_database_get_element(db, element_id);
  int ret = -1;

  if (obj) {
    for (int i = 0; i < obj->nb_substreams; ++i) {
      if (obj->substream_ids[i] == substream_id) {
        ret = i;
        break;
      }
    }
  }
  return ret;
}

static IAMF_CodecConf *iamf_database_element_get_codec_conf(IAMF_DataBase *db,
                                                            uint64_t eid) {
  ElementItem *ei = iamf_database_element_get_item(db, eid);
  return ei ? ei->codecConf : 0;
}

static int iamf_database_element_set_mix_gain_parameter(IAMF_DataBase *db,
                                                        uint64_t eid,
                                                        uint64_t pid) {
  ElementItem *ei = iamf_database_element_get_item(db, eid);
  ParameterItem *pi =
      iamf_database_parameter_viewer_get_item(&db->pViewer, pid);
  if (ei) ei->mixGain = pi;
  return IAMF_OK;
}

/* <<<<<<<<<<<<<<<<<< DATABASE <<<<<<<<<<<<<<<<<< */

/* <<<<<<<<<<<<<<<<<< STREAM DECODER MIXER RESAMPLER <<<<<<<<<<<<<<<<<< */

static int iamf_stream_set_output_layout(IAMF_Stream *s, LayoutInfo *layout);
static uint32_t iamf_stream_mode_ambisonics(uint32_t ambisonics_mode);
static void iamf_stream_free(IAMF_Stream *s);
static void iamf_stream_decoder_close(IAMF_StreamDecoder *d);
static IAMF_StreamRenderer *iamf_stream_renderer_open(IAMF_Stream *s,
                                                      IAMF_MixPresentation *mp,
                                                      int frame_size);
static int iamf_stream_renderer_enable_downmix(IAMF_StreamRenderer *sr);
static void iamf_stream_renderer_close(IAMF_StreamRenderer *sr);
static void iamf_mixer_reset(IAMF_Mixer *m);
static int iamf_stream_scale_decoder_update_recon_gain(
    IAMF_StreamDecoder *decoder, ReconGainList *list);
static IAMF_Resampler *iamf_stream_resampler_open(IAMF_Stream *stream,
                                                  uint32_t out_rate,
                                                  int quality);
static void iamf_stream_resampler_close(IAMF_Resampler *r);

static int iamf_packet_check_count(Packet *pkt) {
  return pkt->count == pkt->nb_sub_packets;
}

/**
 * @brief     Trimming the start of end samples after decoding.
 * @param     [in] f : the input audio frame.
 * @param     [in] start : the trimming start position in audio frame obu header
 * @param     [in] end : the trimming end position in audio frame obu header
 * @param     [in] start_extension : the offset after start
 * @return    the number of samples after trimming
 */
static int iamf_frame_trim(Frame *f, int start, int end, int start_extension) {
  int s, ret;
  s = start + start_extension;
  ret = f->samples - s - end;
  if (start < 0 || end < 0 || start_extension < 0 || ret < 0) {
    ia_logw(
        "clip start %d, end %d, start extension %d, and the left samples %d",
        start, end, start_extension, ret);
    return IAMF_ERR_BAD_ARG;
  }

  if (ret > 0 && ret != f->samples) {
    for (int c = 0; c < f->channels; ++c) {
      memmove(&f->data[c * ret], &f->data[c * f->samples + s],
              ret * sizeof(float));
    }
  }
  f->samples = ret;
  f->pts += start;
  return ret;
}

/**
 * @brief     Apply animated gain to audio frame.
 * @param     [in] f : the input audio frame.
 * @param     [in] gain : the gain value
 * @return    @ref IAErrCode.
 */
static int iamf_frame_gain(Frame *f, MixGainUnit *gain) {
  int soff;
  if (!gain) return IAMF_ERR_BAD_ARG;
  if (f->samples > gain->count) {
    ia_logd("frame samples should be not greater than gain count %d vs %d",
            f->samples, gain->count);
    return IAMF_ERR_INTERNAL;
  }

  if (!gain->gains) {
    ia_logd("use constant gain %f.", gain->constant_gain);
    if (gain->constant_gain != 1.f && gain->constant_gain > 0.f) {
      int count = f->samples * f->channels;
      for (int i = 0; i < count; ++i) f->data[i] *= gain->constant_gain;
    }
    return IAMF_OK;
  }

  ia_logd("use gains %f -> %f", gain->gains[0], gain->gains[gain->count - 1]);
  for (int c = 0; c < f->channels; ++c) {
    soff = c * f->samples;
    for (int i = 0; i < f->samples; ++i) f->data[soff + i] *= gain->gains[i];
  }

  return IAMF_OK;
}

static void iamf_presentation_free(IAMF_Presentation *pst) {
  if (pst) {
    for (int i = 0; i < pst->nb_streams; ++i) {
      if (pst->decoders[i]) iamf_stream_decoder_close(pst->decoders[i]);
      if (pst->renderers[i]) iamf_stream_renderer_close(pst->renderers[i]);
      if (pst->streams[i]) iamf_stream_free(pst->streams[i]);
    }
    if (pst->resampler) iamf_stream_resampler_close(pst->resampler);

    free(pst->renderers);
    free(pst->decoders);
    free(pst->streams);
    iamf_mixer_reset(&pst->mixer);
    free(pst);
  }
}

static IAMF_Stream *iamf_presentation_take_stream(IAMF_Presentation *pst,
                                                  uint64_t eid) {
  IAMF_Stream *stream = 0;

  if (!pst) return 0;

  for (int i = 0; i < pst->nb_streams; ++i) {
    if (pst->streams[i]->element_id == eid) {
      stream = pst->streams[i];
      pst->streams[i] = 0;
      break;
    }
  }

  return stream;
}

static IAMF_StreamDecoder *iamf_presentation_take_decoder(
    IAMF_Presentation *pst, IAMF_Stream *stream) {
  IAMF_StreamDecoder *decoder = 0;
  for (int i = 0; i < pst->nb_streams; ++i) {
    if (pst->decoders[i]->stream == stream) {
      decoder = pst->decoders[i];
      pst->decoders[i] = 0;
      break;
    }
  }

  return decoder;
}

static IAMF_StreamRenderer *iamf_presentation_take_renderer(
    IAMF_Presentation *pst, IAMF_Stream *stream) {
  IAMF_StreamRenderer *renderer = 0;
  for (int i = 0; i < pst->nb_streams; ++i) {
    if (pst->renderers[i]->stream == stream) {
      renderer = pst->renderers[i];
      pst->renderers[i] = 0;
      break;
    }
  }

  return renderer;
}

static IAMF_Resampler *iamf_presentation_take_resampler(
    IAMF_Presentation *pst) {
  IAMF_Resampler *resampler = 0;
  resampler = pst->resampler;
  pst->resampler = 0;

  return resampler;
}

static int iamf_presentation_reuse_stream(IAMF_Presentation *dst,
                                          IAMF_Presentation *src,
                                          uint64_t eid) {
  IAMF_Stream *stream = 0;
  IAMF_StreamDecoder *decoder = 0;
  IAMF_StreamRenderer *renderer = 0;
  IAMF_Stream **streams;
  IAMF_StreamDecoder **decoders;
  IAMF_StreamRenderer **renderers;

  if (!dst || !src) return IAMF_ERR_BAD_ARG;

  stream = iamf_presentation_take_stream(src, eid);
  if (!stream) return IAMF_ERR_INTERNAL;

  decoder = iamf_presentation_take_decoder(src, stream);
  if (!decoder) return IAMF_ERR_INTERNAL;

  renderer = iamf_presentation_take_renderer(src, stream);
  if (!renderer) return IAMF_ERR_INTERNAL;

  iamf_stream_renderer_enable_downmix(renderer);

  streams = IAMF_REALLOC(IAMF_Stream *, dst->streams, dst->nb_streams + 1);
  if (!streams) return IAMF_ERR_INTERNAL;
  dst->streams = streams;

  decoders =
      IAMF_REALLOC(IAMF_StreamDecoder *, dst->decoders, dst->nb_streams + 1);
  if (!decoders) return IAMF_ERR_INTERNAL;
  dst->decoders = decoders;

  renderers =
      IAMF_REALLOC(IAMF_StreamRenderer *, dst->renderers, dst->nb_streams + 1);
  if (!renderers) return IAMF_ERR_INTERNAL;
  dst->renderers = renderers;

  dst->streams[dst->nb_streams] = stream;
  dst->decoders[dst->nb_streams] = decoder;
  dst->renderers[dst->nb_streams] = renderer;
  ++dst->nb_streams;
  ia_logd("reuse stream with element id %" PRIu64, eid);

  return 0;
}

/**
 * Output sound mode:
 *
 * |---------------------------------------------------------------------|
 * | Elem B\Elem A     | Mono   | Stereo | Binaural | > 2-ch | Ambisonic |
 * |---------------------------------------------------------------------|
 * | Mono              | Stereo | Stereo | N/A      | M-ch   | SPL       |
 * |---------------------------------------------------------------------|
 * | Stereo            | Stereo | Stereo | N/A      | M-ch   | SPL       |
 * |---------------------------------------------------------------------|
 * | Binaural          | N/A    | N/A    | Binaural | N/A    | N/A       |
 * |---------------------------------------------------------------------|
 * | > 2-ch            | M-ch   | M-ch   | N/A      | M-ch   | SPL       |
 * |---------------------------------------------------------------------|
 * | Ambisonic         | SPL    | SPL    | N/A      | SPL    | SPL       |
 * |---------------------------------------------------------------------|
 *
 * 2-ch : 2 channels
 * M-ch : Multichannel
 * SPL : Same to playback layout, it means:
 *
 * If (Output_sound_mode == ?Same to playback layout?)
 * {
 *    If (Output_sound_system == ?Sound system A (0+2+0)?) { Output_sound_mode =
 * Stereo; } Else { Output_sound_mode = Multichannel; }
 * }
 *
 * */
static IAMF_SoundMode iamf_presentation_get_output_sound_mode(
    IAMF_Presentation *pst) {
  IAMF_Stream *st;
  IAMF_SoundMode mode = IAMF_SOUND_MODE_NONE, sm;

  for (int s = 0; s < pst->nb_streams; ++s) {
    st = pst->streams[s];
    if (st->scheme == AUDIO_ELEMENT_TYPE_SCENE_BASED)
      sm = iamf_layout_get_sound_mode(&st->final_layout->layout);
    else {
      ChannelLayerContext *cctx = (ChannelLayerContext *)st->priv;
      int type = cctx->layout;
      if (type == IA_CHANNEL_LAYOUT_MONO || type == IA_CHANNEL_LAYOUT_STEREO)
        sm = IAMF_SOUND_MODE_STEREO;
      else if (type == IA_CHANNEL_LAYOUT_BINAURAL)
        sm = IAMF_SOUND_MODE_BINAURAL;
      else
        sm = IAMF_SOUND_MODE_MULTICHANNEL;
    }
    mode = iamf_sound_mode_combine(mode, sm);
  }

  return mode;
}

static ElementConf *iamf_mix_presentation_get_element_conf(
    IAMF_MixPresentation *mp, uint64_t eid) {
  ElementConf *ec = 0;
  if (!mp || !mp->sub_mixes || !mp->sub_mixes->conf_s) return 0;

  for (int i = 0; i < mp->sub_mixes->nb_elements; ++i) {
    if (mp->sub_mixes->conf_s[i].element_id == eid) {
      ec = &mp->sub_mixes->conf_s[i];
      break;
    }
  }

  return ec;
}

void iamf_stream_free(IAMF_Stream *s) {
  if (s) {
    if (s->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
      ChannelLayerContext *ctx = s->priv;
      if (ctx) {
        if (ctx->conf_s) {
          for (int i = 0; i < ctx->nb_layers; ++i) {
            IAMF_FREE(ctx->conf_s[i].output_gain);
            IAMF_FREE(ctx->conf_s[i].recon_gain);
          }
          free(ctx->conf_s);
        }
        free(ctx);
      }
    } else if (s->scheme == AUDIO_ELEMENT_TYPE_SCENE_BASED) {
      AmbisonicsContext *ctx = s->priv;
      free(ctx);
    }
    free(s);
  }
}

static IAMF_Stream *iamf_stream_new(IAMF_Element *elem, IAMF_CodecConf *conf,
                                    LayoutInfo *layout) {
  IAMF_Stream *stream = IAMF_MALLOCZ(IAMF_Stream, 1);
  if (!stream) goto stream_fail;

  stream->element_id = elem->element_id;
  stream->scheme = elem->element_type;
  stream->codecConf_id = conf->codec_conf_id;
  stream->codec_id = iamf_codec_4cc_get_codecID(conf->codec_id);
  stream->sampling_rate = iamf_codec_conf_get_sampling_rate(conf);
  stream->nb_substreams = elem->nb_substreams;
  stream->max_frame_size = AAC_FRAME_SIZE < conf->nb_samples_per_frame
                               ? conf->nb_samples_per_frame * 6
                               : MAX_FRAME_SIZE;

  ia_logd("codec conf id %" PRIu64 ", codec id 0x%x (%s), sampling rate is %u",
          conf->codec_conf_id, conf->codec_id,
          iamf_codec_name(stream->codec_id), stream->sampling_rate);
  if (stream->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
    ChannelLayerContext *ctx = IAMF_MALLOCZ(ChannelLayerContext, 1);
    SubLayerConf *sub_conf;
    ChannelLayerConf *layer_conf;
    ScalableChannelLayoutConf *layers_conf = elem->channels_conf;
    float gain_db;
    int chs = 0;
    IAChannelLayoutType last = IA_CHANNEL_LAYOUT_INVALID;
    ParameterBase *pb;

    if (!ctx) {
      goto stream_fail;
    }

    stream->priv = (void *)ctx;
    ctx->nb_layers = layers_conf->nb_layers;
    if (ctx->nb_layers) {
      sub_conf = IAMF_MALLOCZ(SubLayerConf, ctx->nb_layers);
      if (!sub_conf) {
        goto stream_fail;
      }

      ctx->conf_s = sub_conf;
      for (int i = 0; i < ctx->nb_layers; ++i) {
        sub_conf = &ctx->conf_s[i];
        layer_conf = &layers_conf->layer_conf_s[i];
        sub_conf->layout = layer_conf->loudspeaker_layout;
        sub_conf->nb_substreams = layer_conf->nb_substreams;
        sub_conf->nb_coupled_substreams = layer_conf->nb_coupled_substreams;
        sub_conf->nb_channels =
            sub_conf->nb_substreams + sub_conf->nb_coupled_substreams;

        ia_logi("audio layer %d :", i);
        ia_logi(" > loudspeaker layout %s(%d) .",
                ia_channel_layout_name(sub_conf->layout), sub_conf->layout);
        ia_logi(" > sub-stream count %d .", sub_conf->nb_substreams);
        ia_logi(" > coupled sub-stream count %d .",
                sub_conf->nb_coupled_substreams);

        if (layer_conf->output_gain_flag) {
          sub_conf->output_gain = IAMF_MALLOCZ(IAMF_OutputGain, 1);
          if (!sub_conf->output_gain) {
            ia_loge("Fail to allocate memory for output gain of sub config.");
            goto stream_fail;
          }
          sub_conf->output_gain->flags =
              layer_conf->output_gain_info->output_gain_flag;
          gain_db = q_to_float(layer_conf->output_gain_info->output_gain, 8);
          sub_conf->output_gain->gain = db2lin(gain_db);
          ia_logi(" > output gain flags 0x%02x",
                  sub_conf->output_gain->flags & U8_MASK);
          ia_logi(" > output gain %f (0x%04x), linear gain %f", gain_db,
                  layer_conf->output_gain_info->output_gain & U16_MASK,
                  sub_conf->output_gain->gain);
        } else {
          ia_logi(" > no output gain info.");
        }

        if (layer_conf->recon_gain_flag) {
          sub_conf->recon_gain = IAMF_MALLOCZ(IAMF_ReconGain, 1);
          if (!sub_conf->recon_gain) {
            ia_loge("Fail to allocate memory for recon gain of sub config.");
            goto stream_fail;
          }
          ia_logi(" > wait recon gain info.");
        } else {
          ia_logi(" > no recon gain info.");
        }

        chs += iamf_channel_layout_get_new_channels(
            last, sub_conf->layout, &ctx->channels_order[chs],
            IA_CH_LAYOUT_MAX_CHANNELS - chs);

        stream->nb_coupled_substreams += sub_conf->nb_coupled_substreams;

        ia_logi(" > the total of %d channels", chs);
        last = sub_conf->layout;
      }
    }
    stream->nb_channels = stream->nb_substreams + stream->nb_coupled_substreams;

    ia_logi("channels %d, streams %d, coupled streams %d.", stream->nb_channels,
            stream->nb_substreams, stream->nb_coupled_substreams);

    ia_logi("all channels order:");
    for (int c = 0; c < stream->nb_channels; ++c)
      ia_logi("channel %s(%d)", ia_channel_name(ctx->channels_order[c]),
              ctx->channels_order[c]);

    ctx->layer = ctx->nb_layers - 1;
    iamf_stream_set_output_layout(stream, layout);
    ctx->layout = ctx->conf_s[ctx->layer].layout;
    ctx->channels = ia_channel_layout_get_channels_count(ctx->layout);
    ctx->dmx_mode = ctx->dmx_default_w_idx = ctx->dmx_default_mode =
        INVALID_VALUE;
    for (int i = 0; i < elem->nb_parameters; ++i) {
      pb = elem->parameters[i];
      if (pb->type == IAMF_PARAMETER_TYPE_DEMIXING) {
        DemixingParameter *dp = (DemixingParameter *)pb;
        ctx->dmx_default_mode = dp->mode;
        ctx->dmx_default_w_idx = dp->w;
        break;
      }
    }

    ia_logi("initialized layer %d, layout %s (%d), layout channel count %d.",
            ctx->layer, ia_channel_layout_name(ctx->layout), ctx->layout,
            ctx->channels);
  } else {
    AmbisonicsConf *aconf = elem->ambisonics_conf;
    AmbisonicsContext *ctx;
    stream->nb_channels = aconf->output_channel_count;
    stream->nb_substreams = aconf->substream_count;
    stream->nb_coupled_substreams = aconf->coupled_substream_count;

    ctx = IAMF_MALLOCZ(AmbisonicsContext, 1);
    if (!ctx) {
      goto stream_fail;
    }

    stream->priv = (void *)ctx;
    ctx->mode = iamf_stream_mode_ambisonics(aconf->ambisonics_mode);
    ctx->mapping = aconf->mapping;
    ctx->mapping_size = aconf->mapping_size;

    iamf_stream_set_output_layout(stream, layout);
    ia_logi("stream mode %d", ctx->mode);
  }
  return stream;

stream_fail:

  if (stream) iamf_stream_free(stream);
  return 0;
}

uint32_t iamf_stream_mode_ambisonics(uint32_t ambisonics_mode) {
  if (ambisonics_mode == AMBISONICS_MODE_MONO)
    return STREAM_MODE_AMBISONICS_MONO;
  else if (ambisonics_mode == AMBISONICS_MODE_PROJECTION)
    return STREAM_MODE_AMBISONICS_PROJECTION;
  return STREAM_MODE_AMBISONICS_NONE;
}

int iamf_stream_set_output_layout(IAMF_Stream *s, LayoutInfo *layout) {
  s->final_layout = layout;

  if (s->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
    ChannelLayerContext *ctx = (ChannelLayerContext *)s->priv;
    if (ctx) {
      if (ctx->nb_layers == 1) {
        return IAMF_OK;
      }

      if (layout->layout.type == IAMF_LAYOUT_TYPE_BINAURAL) {
        ctx->layer = ctx->nb_layers - 1;
        ia_logd("use the highest layout downmix to binaural.");
        return IAMF_OK;
      }

      // use the layout that matches the playback layout
      for (int i = 0; i < ctx->nb_layers; ++i) {
        if (iamf_layer_layout_convert_sound_system(ctx->conf_s[i].layout) ==
            layout->layout.sound_system.sound_system) {
          ctx->layer = i;
          ia_logi("scalabel channels layer is %d", i);
          return IAMF_OK;
        }
      }

      // select next highest available layout
      int playback_channels = IAMF_layout_sound_system_channels_count(
          layout->layout.sound_system.sound_system);
      for (int i = 0; i < ctx->nb_layers; ++i) {
        int channels =
            ia_channel_layout_get_channels_count(ctx->conf_s[i].layout);
        if (channels > playback_channels) {
          ctx->layer = i;
          ia_logi("scalabel channels layer is %d", i);
          return IAMF_OK;
        }
      }
    }
  }

  return IAMF_OK;
}

static int iamf_stream_enable(IAMF_DecoderHandle handle, IAMF_Element *elem) {
  IAMF_DecoderContext *ctx = &handle->ctx;
  IAMF_DataBase *db = &ctx->db;
  IAMF_Presentation *pst = ctx->presentation;
  uint64_t element_id;
  IAMF_Stream *stream = 0;
  IAMF_Stream **streams;
  IAMF_CodecConf *conf;
  IAMF_StreamDecoder *decoder = 0;
  IAMF_StreamDecoder **decoders;
  IAMF_StreamRenderer *renderer = 0;
  IAMF_StreamRenderer **renderers;
  int ret = IAMF_ERR_ALLOC_FAIL;

  ia_logd("enable element id %" PRIu64, elem->element_id);
  element_id = elem->element_id;
  conf = iamf_database_element_get_codec_conf(db, element_id);
  ia_logd("codec conf id %" PRIu64, conf->codec_conf_id);

  stream = iamf_stream_new(elem, conf, ctx->output_layout);
  if (!stream) goto stream_enable_fail;

  decoder = iamf_stream_decoder_open(stream, conf);
  if (!decoder) {
    ret = IAMF_ERR_UNIMPLEMENTED;
    goto stream_enable_fail;
  }

  renderer = iamf_stream_renderer_open(stream, pst->obj, decoder->frame_size);
  if (!renderer) {
    ret = IAMF_ERR_ALLOC_FAIL;
    goto stream_enable_fail;
  }

  streams = IAMF_REALLOC(IAMF_Stream *, pst->streams, pst->nb_streams + 1);
  if (!streams) goto stream_enable_fail;
  pst->streams = streams;

  decoders =
      IAMF_REALLOC(IAMF_StreamDecoder *, pst->decoders, pst->nb_streams + 1);
  if (!decoders) goto stream_enable_fail;
  pst->decoders = decoders;

  renderers =
      IAMF_REALLOC(IAMF_StreamRenderer *, pst->renderers, pst->nb_streams + 1);
  if (!renderers) goto stream_enable_fail;
  pst->renderers = renderers;

  pst->streams[pst->nb_streams] = stream;
  pst->decoders[pst->nb_streams] = decoder;
  pst->renderers[pst->nb_streams] = renderer;
  ++pst->nb_streams;

  return IAMF_OK;

stream_enable_fail:
  if (renderer) iamf_stream_renderer_close(renderer);

  if (decoder) iamf_stream_decoder_close(decoder);

  if (stream) iamf_stream_free(stream);

  return ret;
}

IAMF_Resampler *iamf_stream_resampler_open(IAMF_Stream *stream,
                                           uint32_t out_rate, int quality) {
  IAMF_Resampler *resampler = IAMF_MALLOCZ(IAMF_Resampler, 1);
  if (!resampler) goto open_fail;
  int err = 0;
  uint32_t channels = stream->final_layout->channels;
  SpeexResamplerState *speex_resampler = speex_resampler_init(
      channels, stream->sampling_rate, out_rate, quality, &err);
  ia_logi("in sample rate %u, out sample rate %u", stream->sampling_rate,
          out_rate);
  resampler->speex_resampler = speex_resampler;
  if (err != RESAMPLER_ERR_SUCCESS) goto open_fail;
  speex_resampler_skip_zeros(speex_resampler);
  resampler->buffer = IAMF_MALLOCZ(float, stream->max_frame_size *channels);
  if (!resampler->buffer) goto open_fail;
  return resampler;
open_fail:
  if (resampler) iamf_stream_resampler_close(resampler);
  return 0;
}

void iamf_stream_resampler_close(IAMF_Resampler *r) {
  if (r) {
    if (r->buffer) free(r->buffer);
    speex_resampler_destroy(r->speex_resampler);
    IAMF_FREE(r);
  }
}

static IAMF_CoreDecoder *iamf_stream_sub_decoder_open(
    int mode, int channels, int nb_streams, int nb_coupled_streams,
    uint8_t *mapping, int mapping_size, IAMF_CodecConf *conf) {
  IAMF_CodecID cid;
  IAMF_CoreDecoder *cDecoder;
  int ret = 0;

  cid = iamf_codec_4cc_get_codecID(conf->codec_id);
  cDecoder = iamf_core_decoder_open(cid);

  if (cDecoder) {
    iamf_core_decoder_set_codec_conf(cDecoder, conf->decoder_conf,
                                     conf->decoder_conf_size);
    ia_logd(
        "codec %s, mode %d, channels %d, streams %d, coupled streams %d, "
        "mapping size  %d",
        iamf_codec_name(cid), mode, channels, nb_streams, nb_coupled_streams,
        mapping_size);
    iamf_core_decoder_set_streams_info(cDecoder, mode, channels, nb_streams,
                                       nb_coupled_streams, mapping,
                                       mapping_size);
    ret = iamf_core_decoder_init(cDecoder);
    if (ret != IAMF_OK) {
      ia_loge("Fail to initalize core decoder.");
      iamf_core_decoder_close(cDecoder);
      cDecoder = 0;
    }
  }

  return cDecoder;
}

static int iamf_stream_decoder_decode_finish(IAMF_StreamDecoder *decoder);

void iamf_stream_decoder_close(IAMF_StreamDecoder *d) {
  if (d) {
    IAMF_Stream *s = d->stream;

    if (d->packet.sub_packets) {
      for (int i = 0; i < d->packet.nb_sub_packets; ++i)
        IAMF_FREE(d->packet.sub_packets[i]);
    }
    IAMF_FREE(d->packet.sub_packets);
    IAMF_FREE(d->packet.sub_packet_sizes);

    for (int i = 0; i < DEC_BUF_CNT; ++i) {
      IAMF_FREE(d->buffers[i]);
    }

    if (s->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
      if (d->scale) {
        if (d->scale->sub_decoders) {
          for (int i = 0; i < d->scale->nb_layers; ++i)
            iamf_core_decoder_close(d->scale->sub_decoders[i]);
          free(d->scale->sub_decoders);
        }
        if (d->scale->demixer) demixer_close(d->scale->demixer);
        free(d->scale);
      }
    } else if (s->scheme == AUDIO_ELEMENT_TYPE_SCENE_BASED) {
      if (d->ambisonics) {
        if (d->ambisonics->decoder)
          iamf_core_decoder_close(d->ambisonics->decoder);
        free(d->ambisonics);
      }
    }
    free(d);
  }
}

static uint32_t iamf_set_stream_info(IAMF_DecoderHandle handle) {
  IAMF_DecoderContext *ctx = &handle->ctx;
  IAMF_Presentation *pst = ctx->presentation;
  IAMF_StreamDecoder *decoder = 0;
  IAMF_Stream *stream = 0;
  uint32_t cur = 0;
  for (int i = 0; i < pst->nb_streams; i++) {
    cur = pst->streams[i]->max_frame_size;
    if (cur > ctx->info.max_frame_size) ctx->info.max_frame_size = cur;
  }
  for (int i = 0; i < pst->nb_streams; i++) {
    decoder = pst->decoders[i];
    stream = pst->streams[i];
    if (ctx->info.max_frame_size > stream->max_frame_size) {
      for (int n = 0; i < DEC_BUF_CNT; ++n) {
        float *buffer =
            IAMF_REALLOC(float, decoder->buffers[n],
                         ctx->info.max_frame_size * stream->nb_channels);
        if (!buffer) goto fail;
        decoder->buffers[n] = buffer;
      }
    }
  }
  return 0;
fail:
  if (decoder) iamf_stream_decoder_close(decoder);
  return 0;
}

IAMF_StreamDecoder *iamf_stream_decoder_open(IAMF_Stream *stream,
                                             IAMF_CodecConf *conf) {
  IAMF_StreamDecoder *decoder;
  int channels = 0;

  decoder = IAMF_MALLOCZ(IAMF_StreamDecoder, 1);

  if (!decoder) goto open_fail;

  decoder->stream = stream;
  decoder->frame_size = conf->nb_samples_per_frame;
  decoder->delay = -1;
  decoder->packet.nb_sub_packets = stream->nb_substreams;
  decoder->packet.sub_packets = IAMF_MALLOCZ(uint8_t *, stream->nb_substreams);
  decoder->packet.sub_packet_sizes =
      IAMF_MALLOCZ(uint32_t, stream->nb_substreams);

  if (!decoder->packet.sub_packets || !decoder->packet.sub_packet_sizes)
    goto open_fail;

  ia_logt("check channels.");
  channels = stream->final_layout->channels;
  ia_logd("final target channels vs stream original channels (%d vs %d).",
          channels, stream->nb_channels);
  if (channels < stream->nb_channels) {
    channels = stream->nb_channels;
  }

  for (int i = 0; i < DEC_BUF_CNT; ++i) {
    decoder->buffers[i] = IAMF_MALLOCZ(float, stream->max_frame_size *channels);
    if (!decoder->buffers[i]) goto open_fail;
  }
  decoder->frame.id = stream->element_id;
  decoder->frame.channels = stream->nb_channels;

  if (stream->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
    ScalableChannelDecoder *scale = IAMF_MALLOCZ(ScalableChannelDecoder, 1);
    ChannelLayerContext *ctx = (ChannelLayerContext *)stream->priv;
    IAMF_CoreDecoder **sub_decoders;
    IAMF_CoreDecoder *sub;

    if (!scale) goto open_fail;

    decoder->scale = scale;
    scale->nb_layers = ctx->layer + 1;
    sub_decoders = IAMF_MALLOCZ(IAMF_CoreDecoder *, scale->nb_layers);
    if (!sub_decoders) goto open_fail;
    scale->sub_decoders = sub_decoders;
    ia_logi("open sub decdoers for channel-based.");
    for (int i = 0; i < scale->nb_layers; ++i) {
      sub = iamf_stream_sub_decoder_open(
          STREAM_MODE_AMBISONICS_NONE, ctx->conf_s[i].nb_channels,
          ctx->conf_s[i].nb_substreams, ctx->conf_s[i].nb_coupled_substreams, 0,
          0, conf);
      if (!sub) goto open_fail;
      sub_decoders[i] = sub;
    }
    if (ctx->channels != decoder->frame.channels) {
      ia_logd("frame channels changes from %u to %u", decoder->frame.channels,
              ctx->channels);
      decoder->frame.channels = ctx->channels;
    }

    ia_logi("open demixer.");
    scale->demixer = demixer_open(conf->nb_samples_per_frame);
    if (!scale->demixer) goto open_fail;
    iamf_stream_scale_demixer_configure(decoder);
  } else if (stream->scheme == AUDIO_ELEMENT_TYPE_SCENE_BASED) {
    AmbisonicsDecoder *a = IAMF_MALLOCZ(AmbisonicsDecoder, 1);
    AmbisonicsContext *ctx = (AmbisonicsContext *)stream->priv;
    if (!a) goto open_fail;
    decoder->ambisonics = a;

    ia_logd("open sub decdoers for ambisonics.");
    a->decoder = iamf_stream_sub_decoder_open(
        ctx->mode, stream->nb_channels, stream->nb_substreams,
        stream->nb_coupled_substreams, ctx->mapping, ctx->mapping_size, conf);

    if (!a->decoder) goto open_fail;
  }

  return decoder;

open_fail:
  if (decoder) iamf_stream_decoder_close(decoder);
  return 0;
}

static int iamf_stream_decoder_receive_packet(IAMF_StreamDecoder *decoder,
                                              int substream_index,
                                              IAMF_Frame *packet) {
  if (substream_index > INVALID_VALUE &&
      substream_index < decoder->packet.nb_sub_packets) {
    if (!decoder->packet.sub_packets[substream_index]) {
      ++decoder->packet.count;
    }
    IAMF_FREE(decoder->packet.sub_packets[substream_index]);
    decoder->packet.sub_packets[substream_index] =
        IAMF_MALLOC(uint8_t, packet->size);
    if (!decoder->packet.sub_packets[substream_index])
      return IAMF_ERR_ALLOC_FAIL;
    memcpy(decoder->packet.sub_packets[substream_index], packet->data,
           packet->size);
    decoder->packet.sub_packet_sizes[substream_index] = packet->size;
  }

  if (!substream_index) {
    decoder->frame.strim = packet->trim_start;
    decoder->frame.etrim = packet->trim_end;
  }

  return 0;
}

static int iamf_stream_decoder_update_parameter(IAMF_StreamDecoder *dec,
                                                IAMF_DataBase *db,
                                                uint64_t pid) {
  ParameterItem *pi = iamf_database_parameter_get_item(db, pid);
  IAMF_Stream *s = dec->stream;

  if (pi) {
    if (pi->type == IAMF_PARAMETER_TYPE_DEMIXING) {
      ChannelLayerContext *ctx = (ChannelLayerContext *)s->priv;
      ctx->dmx_mode = iamf_database_parameter_get_demix_mode(
          db, pid, s->timestamp + dec->frame_size / 2);
      ia_logd("update demix mode %d", ctx->dmx_mode);
    } else if (pi->type == IAMF_PARAMETER_TYPE_RECON_GAIN) {
      ReconGainList *recon = iamf_database_parameter_get_recon_gain_list(
          db, pid, s->timestamp + dec->frame_size / 2);
      ia_logd("update recon %p", recon);
      if (recon) iamf_stream_scale_decoder_update_recon_gain(dec, recon);
    }
  }
  return IAMF_OK;
}

static int iamf_stream_decoder_update_delay(IAMF_StreamDecoder *dec) {
  IAMF_CoreDecoder *cder = 0;
  int delay = 0;
  if (dec->stream->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
    if (dec->scale->sub_decoders) cder = dec->scale->sub_decoders[0];
  } else
    cder = dec->ambisonics->decoder;

  if (cder) delay = dec->delay = iamf_core_decoder_get_delay(cder);

  return delay;
}

static int iamf_stream_decoder_decode(IAMF_StreamDecoder *decoder, float *pcm) {
  int ret = 0;
  IAMF_Stream *stream = decoder->stream;
  float *buffer = decoder->buffers[2];
  if (stream->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
    ret = iamf_stream_scale_decoder_decode(decoder, buffer);
  } else if (stream->scheme == AUDIO_ELEMENT_TYPE_SCENE_BASED)
    ret = iamf_stream_ambisonics_decoder_decode(decoder, pcm);

  if (stream->trimming_start != decoder->frame_size && decoder->delay < 0) {
    iamf_stream_decoder_update_delay(decoder);
    stream->trimming_start += decoder->delay;
    ia_logi("decoder delay is %d, trimming start is %" PRIu64, decoder->delay,
            stream->trimming_start);

    if (stream->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
      demixer_set_frame_offset(decoder->scale->demixer, decoder->delay);
    }
  }

  if (stream->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED)
    iamf_stream_scale_decoder_demix(decoder, buffer, pcm, ret);

  return ret;
}

int iamf_stream_decoder_decode_finish(IAMF_StreamDecoder *decoder) {
  for (int i = 0; i < decoder->packet.nb_sub_packets; ++i) {
    IAMF_FREEP(&decoder->packet.sub_packets[i]);
  }
  memset(decoder->packet.sub_packet_sizes, 0,
         sizeof(uint32_t) * decoder->packet.nb_sub_packets);
  decoder->packet.count = 0;
  return 0;
}

int iamf_stream_scale_decoder_set_default_recon_gain(
    IAMF_StreamDecoder *decoder) {
  IAMF_Stream *stream = decoder->stream;
  Demixer *demixer;
  ChannelLayerContext *ctx;
  IAMF_ReconGain rg;

  if (stream->scheme != AUDIO_ELEMENT_TYPE_CHANNEL_BASED || !decoder->scale)
    return IAMF_ERR_INTERNAL;

  demixer = decoder->scale->demixer;
  ctx = (ChannelLayerContext *)stream->priv;

  if (!demixer || !ctx) return IAMF_ERR_INTERNAL;

  memset(&rg, 0, sizeof(IAMF_ReconGain));
  if (ctx->layer > 0) {
    uint8_t layout = ctx->conf_s[0].layout;
    rg.flags = iamf_recon_channels_get_flags(layout, ctx->layout);
    ia_logd("first layer %d, target layout %d", layout, ctx->layout);
    ia_logd("default recon gain flags 0x%04x", rg.flags);
    rg.nb_channels = bit1_count(rg.flags);
    ia_logd("default channel count %d", rg.nb_channels);
    iamf_recon_channels_order_update(ctx->layout, &rg);
    for (int i = 0; i < rg.nb_channels; ++i) {
      rg.recon_gain[i] = 1.f;
      ia_logd("channel %s(%d) : default recon gain is 1.",
              ia_channel_name(rg.order[i]), rg.order[i]);
    }
  }
  demixer_set_recon_gain(demixer, rg.nb_channels, rg.order, rg.recon_gain,
                         rg.flags);

  return IAMF_OK;
}

static int iamf_stream_scale_decoder_update_recon_gain(
    IAMF_StreamDecoder *decoder, ReconGainList *list) {
  ReconGain *src;
  IAMF_ReconGain *dst;
  int ret = 0;
  IAMF_Stream *stream = decoder->stream;
  ChannelLayerContext *ctx = (ChannelLayerContext *)stream->priv;

  if (!list) return IAMF_ERR_BAD_ARG;

  ia_logt("recon gain info : list %p, count %d, recons %p", list, list->count,
          list->recon);
  for (int i = 0; i < ctx->nb_layers; ++i) {
    src = &list->recon[i];
    dst = ctx->conf_s[i].recon_gain;
    if (dst) {
      ia_logd("audio layer %d :", i);
      if (dst->flags ^ src->flags) {
        dst->flags = src->flags;
        dst->nb_channels = bit1_count(src->flags);
        iamf_recon_channels_order_update(ctx->conf_s[i].layout, dst);
      }
      for (int c = 0; c < dst->nb_channels; ++c) {
        dst->recon_gain[c] = src->recon_gain_f[c];
      }
      ia_logd(" > recon gain flags 0x%04x", dst->flags);
      ia_logd(" > channel count %d", dst->nb_channels);
      for (int c = 0; c < dst->nb_channels; ++c)
        ia_logd(" > > channel %s(%d) : recon gain %f(0x%02x)",
                ia_channel_name(dst->order[c]), dst->order[c],
                dst->recon_gain[c], src->recon_gain[c]);
    }
  }
  ia_logt("recon gain info .");

  return ret;
}

static int iamf_stream_scale_decoder_decode(IAMF_StreamDecoder *decoder,
                                            float *pcm) {
  IAMF_Stream *stream = decoder->stream;
  ChannelLayerContext *ctx = (ChannelLayerContext *)stream->priv;
  ScalableChannelDecoder *scale = decoder->scale;
  int ret = IAMF_OK;
  float *out = pcm;
  IAMF_CoreDecoder *dec;
  uint32_t substream_offset = 0;

  if (!scale->nb_layers) return ret;

  decoder->frame_padding = 0;

  for (int i = 0; i <= ctx->layer; ++i) {
    dec = scale->sub_decoders[i];
    ia_logd(
        "CG#%d: channels %d, streams %d, decoder %p, out %p, offset %lX, "
        "size %lu",
        i, ctx->conf_s[i].nb_channels, ctx->conf_s[i].nb_substreams, dec, out,
        (float *)out - (float *)pcm,
        sizeof(float) * decoder->frame_size * ctx->conf_s[i].nb_channels);
    for (int k = 0; k < ctx->conf_s[i].nb_substreams; ++k) {
      ia_logd(" > sub-packet %d (%p) size %d", k,
              decoder->packet.sub_packets[substream_offset + k],
              decoder->packet.sub_packet_sizes[substream_offset + k]);
    }
    ret = iamf_core_decoder_decode(
        dec, &decoder->packet.sub_packets[substream_offset],
        &decoder->packet.sub_packet_sizes[substream_offset],
        ctx->conf_s[i].nb_substreams, out, decoder->frame_size);
    if (ret < 0) {
      ia_loge("sub packet %d decode fail.", i);
      break;
    }
    out += (decoder->frame_size * ctx->conf_s[i].nb_channels);
    substream_offset += ctx->conf_s[i].nb_substreams;
  }

  if (ret > 0 && ret != decoder->frame_size) {
    ia_logw("decoded frame size is not %d (%d).", decoder->frame_size, ret);
    decoder->frame_padding = decoder->frame_size - ret;
    ret = decoder->frame_size;
  }

  return ret;
}

static int32_t iamf_stream_scale_decoder_demix(IAMF_StreamDecoder *decoder,
                                               float *src, float *dst,
                                               uint32_t frame_size) {
  IAMF_Stream *stream = decoder->stream;
  ScalableChannelDecoder *scale = decoder->scale;
  ChannelLayerContext *ctx = (ChannelLayerContext *)stream->priv;

  Demixer *demixer = scale->demixer;
  IAMF_ReconGain *re = ctx->conf_s[ctx->layer].recon_gain;

  ia_logt("demixer info update :");
  if (re) {
    demixer_set_recon_gain(demixer, re->nb_channels, re->order, re->recon_gain,
                           re->flags);
    ia_logd("channel flags 0x%04x", re->flags & U16_MASK);
    for (int c = 0; c < re->nb_channels; ++c) {
      ia_logd("channel %s(%d) recon gain %f", ia_channel_name(re->order[c]),
              re->order[c], re->recon_gain[c]);
    }
  }

  if (ctx->dmx_mode > INVALID_VALUE)
    demixer_set_demixing_info(scale->demixer, ctx->dmx_mode, -1);

  return demixer_demixing(scale->demixer, dst, src, frame_size);
}

int iamf_stream_scale_demixer_configure(IAMF_StreamDecoder *decoder) {
  IAMF_Stream *stream = decoder->stream;
  ScalableChannelDecoder *scale = decoder->scale;
  Demixer *demixer = scale->demixer;
  IAChannel chs[IA_CH_LAYOUT_MAX_CHANNELS];
  float gains[IA_CH_LAYOUT_MAX_CHANNELS];
  uint8_t flags;
  uint32_t count = 0;
  SubLayerConf *layer_conf;
  ChannelLayerContext *ctx = (ChannelLayerContext *)stream->priv;

  demixer_set_channel_layout(demixer, ctx->layout);
  demixer_set_channels_order(demixer, ctx->channels_order, ctx->channels);

  for (int l = 0; l <= ctx->layer; ++l) {
    layer_conf = &ctx->conf_s[l];
    if (layer_conf->output_gain) {
      flags = layer_conf->output_gain->flags;
      for (int c = 0; c < IA_CH_GAIN_COUNT; ++c) {
        if (flags & RSHIFT(c)) {
          chs[count] = iamf_output_gain_channel_map(layer_conf->layout, c);
          if (chs[count] != IA_CH_INVALID) {
            gains[count++] = layer_conf->output_gain->gain;
          }
        }
      }
    }
  }

  demixer_set_output_gain(demixer, chs, gains, count);
  demixer_set_demixing_info(demixer, ctx->dmx_default_mode,
                            ctx->dmx_default_w_idx);
  iamf_stream_scale_decoder_set_default_recon_gain(decoder);

  ia_logd("demixer info :");
  ia_logd("layout %s(%d)", ia_channel_layout_name(ctx->layout), ctx->layout);
  ia_logd("input channels order :");

  for (int c = 0; c < ctx->channels; ++c) {
    ia_logd("channel %s(%d)", ia_channel_name(ctx->channels_order[c]),
            ctx->channels_order[c]);
  }

  ia_logd("output gain info : ");
  for (int c = 0; c < count; ++c) {
    ia_logd("channel %s(%d) gain %f", ia_channel_name(chs[c]), chs[c],
            gains[c]);
  }

  return 0;
}

static uint32_t iamf_stream_ambisionisc_order(int channels) {
  if (channels == 1)
    return IAMF_ZOA;
  else if (channels == 4)
    return IAMF_FOA;
  else if (channels == 9)
    return IAMF_SOA;
  else if (channels == 16)
    return IAMF_TOA;
  return UINT32_MAX;
}

int iamf_stream_ambisonics_decoder_decode(IAMF_StreamDecoder *decoder,
                                          float *pcm) {
  AmbisonicsDecoder *amb = decoder->ambisonics;
  int ret = 0;
  IAMF_CoreDecoder *dec;

  dec = amb->decoder;
  for (int k = 0; k < decoder->packet.nb_sub_packets; ++k) {
    ia_logd(" > sub-packet %d (%p) size %d", k, decoder->packet.sub_packets[k],
            decoder->packet.sub_packet_sizes[k]);
  }
  ret = iamf_core_decoder_decode(
      dec, decoder->packet.sub_packets, decoder->packet.sub_packet_sizes,
      decoder->packet.nb_sub_packets, pcm, decoder->frame_size);
  if (ret < 0) {
    ia_loge("ambisonics stream packet decode fail.");
  } else if (ret != decoder->frame_size) {
    ia_loge("decoded frame size is not %d (%d).", decoder->frame_size, ret);
  }

  return ret;
}

static int iamf_stream_renderer_update_info(IAMF_StreamRenderer *sr,
                                            IAMF_MixPresentation *mp,
                                            uint32_t frame_size) {
  IAMF_Stream *s = sr->stream;
  ElementConf *ec = iamf_mix_presentation_get_element_conf(mp, s->element_id);
  if (ec) sr->headphones_rendering_mode = ec->conf_r.headphones_rendering_mode;
  sr->frame_size = frame_size;
  return IAMF_OK;
}

int iamf_stream_renderer_enable_downmix(IAMF_StreamRenderer *sr) {
  IAMF_Stream *s;
  ChannelLayerContext *ctx;
  int in, out;
  if (!sr) return IAMF_ERR_BAD_ARG;

  s = sr->stream;
  if (s->scheme == AUDIO_ELEMENT_TYPE_SCENE_BASED || !s->final_layout ||
      s->final_layout->layout.type !=
          IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
    return IAMF_ERR_BAD_ARG;
  }

  ctx = (ChannelLayerContext *)s->priv;
  if (ctx->dmx_default_mode == INVALID_VALUE) return IAMF_ERR_BAD_ARG;

  in = ctx->conf_s[ctx->layer].layout;
  out = iamf_sound_system_get_channel_layout(
      s->final_layout->layout.sound_system.sound_system);

  if (out == IA_CHANNEL_LAYOUT_INVALID) return IAMF_ERR_BAD_ARG;

  if (sr->downmixer) DMRenderer_close(sr->downmixer);
  sr->downmixer = DMRenderer_open(in, out);
  if (!sr->downmixer) return IAMF_ERR_BAD_ARG;

  DMRenderer_set_mode_weight(sr->downmixer, ctx->dmx_default_mode,
                             ctx->dmx_default_w_idx);

  return IAMF_OK;
}

IAMF_StreamRenderer *iamf_stream_renderer_open(IAMF_Stream *s,
                                               IAMF_MixPresentation *mp,
                                               int frame_size) {
  IAMF_StreamRenderer *sr = IAMF_MALLOCZ(IAMF_StreamRenderer, 1);
  if (!sr) return 0;

  sr->stream = s;
  iamf_stream_renderer_enable_downmix(sr);
  iamf_stream_renderer_update_info(sr, mp, frame_size);

#if ENABLE_HOA_TO_BINAURAL || ENABLE_MULTICHANNEL_TO_BINAURAL
  if (s->final_layout) {
    sr->renderer.layout = &s->final_layout->sp;
    if (s->final_layout->layout.type == IAMF_LAYOUT_TYPE_BINAURAL) {
      if (s->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
#if ENABLE_MULTICHANNEL_TO_BINAURAL
        ChannelLayerContext *ctx = (ChannelLayerContext *)s->priv;
        uint32_t in_layout = iamf_layer_layout_get_rendering_id(ctx->layout);
        IAMF_element_renderer_init_M2B(&sr->renderer.layout->binaural_f,
                                       in_layout, s->element_id, frame_size,
                                       s->sampling_rate);
#else
        ia_loge(
            "Channel based audio but multichannel to binaural disabled, use "
            "default rendering");
#endif
      } else if (s->scheme == AUDIO_ELEMENT_TYPE_SCENE_BASED) {
#if ENABLE_HOA_TO_BINAURAL
        int in_channels = s->nb_channels;
        IAMF_element_renderer_init_H2B(&sr->renderer.layout->binaural_f,
                                       in_channels, s->element_id, frame_size,
                                       s->sampling_rate);
#else
        ia_loge(
            "Scene based audio but HOA to binaural disabled, use default "
            "rendering");
#endif
      }
    }
  }
#endif
  return sr;
}

void iamf_stream_renderer_close(IAMF_StreamRenderer *sr) {
  if (!sr) return;

  if (sr->downmixer) DMRenderer_close(sr->downmixer);

#if ENABLE_HOA_TO_BINAURAL || ENABLE_MULTICHANNEL_TO_BINAURAL
  IAMF_Stream *s = sr->stream;
  if (s->final_layout &&
      s->final_layout->layout.type == IAMF_LAYOUT_TYPE_BINAURAL) {
    if (s->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
#if ENABLE_MULTICHANNEL_TO_BINAURAL
      IAMF_element_renderer_deinit_M2B(&sr->renderer.layout->binaural_f,
                                       s->element_id);
      ia_logd("deinit M2B");
#endif
    } else if (s->scheme == AUDIO_ELEMENT_TYPE_SCENE_BASED) {
#if ENABLE_HOA_TO_BINAURAL
      IAMF_element_renderer_deinit_H2B(&sr->renderer.layout->binaural_f,
                                       s->element_id);
      ia_logd("deinit H2B");
#endif
    }
  }
#endif

  free(sr);
}

/**
 * @brief     Rendering an Audio Element.
 * @param     [in] sr : stream render handle.
 * @param     [in] in : input audio pcm
 * @param     [in] out : output audio pcm
 * @param     [in] frame_size : the size of audio frame.
 * @return    the number of rendering samples
 */
static int iamf_stream_render(IAMF_StreamRenderer *sr, float *in, float *out,
                              int frame_size) {
  IAMF_Stream *stream = sr->stream;
  int ret = IAMF_OK;
  int inchs;
  int outchs = stream->final_layout->channels;
  float **sout = IAMF_MALLOCZ(float *, outchs);
  float **sin = 0;
  lfe_filter_t *plfe = 0;

  if (!sout) {
    ret = IAMF_ERR_ALLOC_FAIL;
    goto render_end;
  }
  for (int i = 0; i < outchs; ++i) sout[i] = &out[frame_size * i];

  inchs = stream->nb_channels;
  if (stream->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
    ChannelLayerContext *ctx = (ChannelLayerContext *)stream->priv;
    inchs = ia_channel_layout_get_channels_count(ctx->layout);
  }
  sin = IAMF_MALLOCZ(float *, inchs);
  if (!sin) {
    ret = IAMF_ERR_ALLOC_FAIL;
    goto render_end;
  }
  for (int i = 0; i < inchs; ++i) sin[i] = &in[frame_size * i];

  if (stream->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
#if ENABLE_MULTICHANNEL_TO_BINAURAL
    if (stream->final_layout->layout.type == IAMF_LAYOUT_TYPE_BINAURAL &&
        sr->headphones_rendering_mode == 1) {
      IAMF_element_renderer_render_M2B(&sr->renderer.layout->binaural_f,
                                       stream->element_id, sin, sout,
                                       frame_size);

    } else
#endif
        if (sr->downmixer) {
      ChannelLayerContext *ctx = (ChannelLayerContext *)stream->priv;
      ia_logd("rendering: offset %u", sr->offset);
      if (sr->offset)
        DMRenderer_downmix(sr->downmixer, in, out, 0, sr->offset, frame_size);
      if (ctx->dmx_mode > INVALID_VALUE)
        DMRenderer_set_mode_weight(sr->downmixer, ctx->dmx_mode, INVALID_VALUE);
      if (frame_size > sr->offset)
        DMRenderer_downmix(sr->downmixer, in, out, sr->offset,
                           frame_size - sr->offset, frame_size);
    } else {
      ChannelLayerContext *ctx = (ChannelLayerContext *)stream->priv;
      struct m2m_rdr_t m2m;
      IAMF_SP_LAYOUT lin;
      IAMF_PREDEFINED_SP_LAYOUT pin;

      memset(&lin, 0, sizeof(IAMF_SP_LAYOUT));
      memset(&pin, 0, sizeof(IAMF_PREDEFINED_SP_LAYOUT));

      lin.sp_layout.predefined_sp = &pin;
      if (stream->nb_channels == 1) {
        pin.system = IAMF_MONO;
      } else {
        pin.system = iamf_layer_layout_get_rendering_id(ctx->layout);
        pin.lfe1 = iamf_layer_layout_lfe1(ctx->layout);
      }

      IAMF_element_renderer_get_M2M_matrix(&lin, &stream->final_layout->sp,
                                           &m2m);
      IAMF_element_renderer_render_M2M(&m2m, sin, sout, frame_size);
    }
  } else if (stream->scheme == AUDIO_ELEMENT_TYPE_SCENE_BASED) {
#if ENABLE_HOA_TO_BINAURAL
    if (stream->final_layout->layout.type == IAMF_LAYOUT_TYPE_BINAURAL) {
      IAMF_element_renderer_render_H2B(&sr->renderer.layout->binaural_f,
                                       stream->element_id, sin, sout,
                                       frame_size);
    } else {
#endif
      struct h2m_rdr_t h2m;
      IAMF_HOA_LAYOUT hin;
      hin.order = iamf_stream_ambisionisc_order(inchs);
      ia_logd("ambisonics order is %d", hin.order);
      if (hin.order == UINT32_MAX) {
        ret = IAMF_ERR_INTERNAL;
        goto render_end;
      }
#if DISABLE_LFE_HOA == 1
      hin.lfe_on = 0;
      IAMF_element_renderer_get_H2M_matrix(
          &hin, stream->final_layout->sp.sp_layout.predefined_sp, &h2m);
#else
    hin.lfe_on = 1;
    IAMF_element_renderer_get_H2M_matrix(
        &hin, stream->final_layout->sp.sp_layout.predefined_sp, &h2m);

    if (hin.lfe_on && iamf_layout_lfe_check(&stream->final_layout->layout)) {
      plfe = &stream->final_layout->sp.lfe_f;
      if (plfe->init == 0) lfefilter_init(plfe, 120, stream->sampling_rate);
    }
#endif

      IAMF_element_renderer_render_H2M(&h2m, sin, sout, frame_size, plfe);
#if ENABLE_HOA_TO_BINAURAL
    }
#endif
  }

render_end:

  if (sin) {
    free(sin);
  }
  if (sout) {
    free(sout);
  }
  return ret;
}

void iamf_mixer_reset(IAMF_Mixer *m) {
  IAMF_FREE(m->element_ids);
  IAMF_FREE(m->frames);
  memset(m, 0, sizeof(IAMF_Mixer));
}

static int iamf_mixer_init(IAMF_DecoderHandle handle) {
  IAMF_DecoderContext *ctx = &handle->ctx;
  IAMF_Presentation *pst = ctx->presentation;
  IAMF_CodecConf *cc = 0;
  IAMF_Mixer *mixer = &pst->mixer;
  int cnt = pst->nb_streams;

  if (!cnt) {
    return IAMF_ERR_INTERNAL;
  }

  memset(mixer, 0, sizeof(IAMF_Mixer));
  mixer->nb_elements = cnt;
  mixer->element_ids = IAMF_MALLOCZ(uint64_t, cnt);
  mixer->frames = IAMF_MALLOCZ(Frame *, cnt);
  if (!mixer->element_ids || !mixer->frames) {
    iamf_mixer_reset(mixer);
    return IAMF_ERR_ALLOC_FAIL;
  }
  for (int i = 0; i < cnt; ++i) {
    mixer->element_ids[i] = pst->streams[i]->element_id;
  }

  cc = iamf_database_element_get_codec_conf(&ctx->db,
                                            pst->streams[0]->element_id);
  if (!cc) {
    iamf_mixer_reset(mixer);
    return IAMF_ERR_INTERNAL;
  }

  return 0;
}

static int iamf_mixer_add_frame(IAMF_Mixer *mixer, Frame *f) {
  for (int i = 0; i < mixer->nb_elements; ++i) {
    if (mixer->element_ids[i] == f->id) {
      mixer->frames[i] = f;
      break;
    }
  }
  return IAMF_OK;
}

/**
 * @brief     Mix audio frame from different audio element.
 * @param     [in] mixer : the mixer handle
 * @param     [in] f : the input audio frame.
 * @return    the number of samples after mix
 */
static int iamf_mixer_mix(IAMF_Mixer *mixer, Frame *f) {
  int s = mixer->frames[0]->samples;
  int64_t pts = mixer->frames[0]->pts;
  int chs = mixer->frames[0]->channels;
  int soff;

  for (int i = 1; i < mixer->nb_elements; ++i) {
    if (s != mixer->frames[i]->samples || pts != mixer->frames[i]->pts) {
      ia_loge("Frame ( 0, %d) has different samples (%d, %d) or pts (%" PRId64
              ", %" PRId64 ")",
              i, s, mixer->frames[i]->samples, pts, mixer->frames[i]->pts);
      return 0;
    }
  }

  f->pts = pts;
  f->samples = s;
  f->strim = mixer->frames[0]->strim;
  ia_logd("mixed frame pts %" PRId64 ", samples %d", f->pts, s);
  memset(f->data, 0, sizeof(float) * s * chs);

  for (int e = 0; e < mixer->nb_elements; ++e) {
    for (int c = 0; c < chs; ++c) {
      soff = c * f->samples;
      for (int i = 0; i < f->samples; ++i) {
        f->data[soff + i] += mixer->frames[e]->data[soff + i];
      }
    }
  }

  return s;
}

/* >>>>>>>>>>>>>>>>>> STREAM DECODER MIXER >>>>>>>>>>>>>>>>>> */

static void iamf_extra_data_reset(IAMF_extradata *data);

static int32_t iamf_decoder_internal_reset(IAMF_DecoderHandle handle) {
  IAMF_DecoderContext *ctx = &handle->ctx;

  iamf_database_reset(&ctx->db);
  iamf_extra_data_reset(&ctx->metadata);
  if (ctx->presentation) iamf_presentation_free(ctx->presentation);
  if (ctx->output_layout) iamf_layout_info_free(ctx->output_layout);
  memset(ctx, 0, sizeof(IAMF_DecoderContext));
  if (handle->limiter) audio_effect_peak_limiter_uninit(handle->limiter);

  return 0;
}

static int iamf_decoder_internal_init(IAMF_DecoderHandle handle,
                                      const uint8_t *data, uint32_t size,
                                      uint32_t *rsize) {
  int32_t ret = 0;
  uint32_t pos = 0, consume = 0;
  IAMF_DecoderContext *ctx = &handle->ctx;

  if (~ctx->flags & IAMF_FLAG_MAGIC_CODE) {
    // search magic code obu
    IAMF_OBU obj;
    ia_logi("Without magic code flag.");
    while (pos < size) {
      consume = IAMF_OBU_split(data, size, &obj);
      if (!consume || obj.type == IAMF_OBU_SEQUENCE_HEADER) {
        ia_logi("Get magic code.");
        break;
      }
      pos += consume;
    }
  }

  if (consume || ctx->flags & IAMF_FLAG_MAGIC_CODE) {
    pos += iamf_decoder_internal_read_descriptors_OBUs(handle, data + pos,
                                                       size - pos);
  }

  if (~ctx->flags & IAMF_FLAG_CONFIG) ret = IAMF_ERR_BUFFER_TOO_SMALL;

  *rsize = pos;
  return ret;
}

uint32_t iamf_decoder_internal_read_descriptors_OBUs(IAMF_DecoderHandle handle,
                                                     const uint8_t *data,
                                                     uint32_t size) {
  IAMF_OBU obu;
  uint32_t pos = 0, ret = 0, rsize = 0;

  while (pos < size) {
    ret = IAMF_OBU_split(data + pos, size - pos, &obu);
    if (!ret) {
      ia_logw("consume size is 0.");
      break;
    }
    rsize = ret;
    ia_logt("consume size %d, obu type (%d) %s", ret, obu.type,
            IAMF_OBU_type_string(obu.type));
    if ((obu.redundant && !(~handle->ctx.flags & IAMF_FLAG_DESCRIPTORS)) ||
        IAMF_OBU_is_reserved_OBU(&obu)) {
      pos += rsize;
      continue;
    }
    if (IAMF_OBU_is_descrptor_OBU(&obu)) {
      ret = iamf_decoder_internal_add_descrptor_OBU(handle, &obu);
      if (ret == IAMF_OK) {
        switch (obu.type) {
          case IAMF_OBU_SEQUENCE_HEADER:
            handle->ctx.flags = IAMF_FLAG_MAGIC_CODE;
            break;
          case IAMF_OBU_CODEC_CONFIG:
            handle->ctx.flags |= IAMF_FLAG_CODEC_CONFIG;
            break;
          case IAMF_OBU_AUDIO_ELEMENT:
            handle->ctx.flags |= IAMF_FLAG_AUDIO_ELEMENT;
            break;
          case IAMF_OBU_MIX_PRESENTATION:
            handle->ctx.flags |= IAMF_FLAG_MIX_PRESENTATION;
            break;
          default:
            break;
        }
      }
    } else {
      if (!(~handle->ctx.flags & IAMF_FLAG_DESCRIPTORS))
        handle->ctx.flags |= IAMF_FLAG_CONFIG;
      break;
    }
    pos += rsize;
  }
  return pos;
}

static uint32_t iamf_decoder_internal_parameter_prepare(
    IAMF_DecoderHandle handle, uint64_t pid) {
  IAMF_DecoderContext *ctx = &handle->ctx;
  IAMF_Presentation *pst = ctx->presentation;
  IAMF_DataBase *db = &ctx->db;
  IAMF_StreamDecoder *dec = 0;
  IAMF_Element *e;

  e = iamf_database_get_element_by_parameterID(&ctx->db, pid);
  if (e) {
    for (int i = 0; i < pst->nb_streams; ++i) {
      if (pst->streams[i]->element_id == e->element_id) {
        dec = pst->decoders[i];
        break;
      }
    }
    if (dec) iamf_stream_decoder_update_parameter(dec, db, pid);
  }
  return IAMF_OK;
}

static int iamf_decoder_internal_update_statue(IAMF_DecoderHandle handle) {
  IAMF_DecoderContext *ctx = &handle->ctx;
  IAMF_Presentation *pst = ctx->presentation;
  int ret = 1;

  for (int s = 0; s < pst->nb_streams; ++s) {
    if (!iamf_packet_check_count(&pst->decoders[s]->packet)) {
      ret &= 0;
      break;
    }
  }

  if (ret) ctx->status = IAMF_DECODER_STATUS_RUN;

  return ret;
}

uint32_t iamf_decoder_internal_parse_OBUs(IAMF_DecoderHandle handle,
                                          const uint8_t *data, uint32_t size) {
  IAMF_OBU obu;
  uint32_t pos = 0, ret = 0;

  while (pos < size) {
    ret = IAMF_OBU_split(data + pos, size - pos, &obu);
    if (!ret) {
      ia_logt("need more data.");
      break;
    }

    if (obu.type == IAMF_OBU_PARAMETER_BLOCK) {
      uint64_t pid = IAMF_OBU_get_object_id(&obu);
      if (pid != INVALID_ID) {
        IAMF_ParameterParam ext;
        memset(&ext, 0, sizeof(IAMF_ParameterParam));
        IAMF_ObjectParameter *param = IAMF_OBJECT_PARAM(&ext);
        IAMF_Object *obj;
        ext.base.type = IAMF_OBU_PARAMETER_BLOCK;
        ext.param_base = iamf_database_parameter_viewer_get_parmeter_base(
            &handle->ctx.db, pid);
        IAMF_Element *e = IAMF_ELEMENT(
            iamf_database_get_element_by_parameterID(&handle->ctx.db, pid));
        if (e) {
          if (e->element_type == AUDIO_ELEMENT_TYPE_CHANNEL_BASED &&
              e->channels_conf && e->channels_conf->layer_conf_s) {
            ScalableChannelLayoutConf *conf = e->channels_conf;
            ext.nb_layers = conf->nb_layers;

            for (int i = 0; i < ext.nb_layers; ++i) {
              if (conf->layer_conf_s[i].recon_gain_flag)
                ext.recon_gain_present_flags |= RSHIFT(i);
            }
          }
        }
        obj = IAMF_object_new(&obu, param);
        iamf_database_add_object(&handle->ctx.db, obj);
        iamf_decoder_internal_parameter_prepare(handle, pid);
      }
    } else if (obu.type >= IAMF_OBU_AUDIO_FRAME &&
               obu.type <= IAMF_OBU_AUDIO_FRAME_ID17) {
      IAMF_Object *obj = IAMF_object_new(&obu, 0);
      IAMF_Frame *o = (IAMF_Frame *)obj;
      iamf_decoder_internal_deliver(handle, o);
      IAMF_object_free(obj);
      iamf_decoder_internal_update_statue(handle);
    } else if (obu.type == IAMF_OBU_SEQUENCE_HEADER && !obu.redundant) {
      ia_logi("*********** FOUND NEW MAGIC CODE **********");
      handle->ctx.status = IAMF_DECODER_STATUS_RECONFIGURE;
      break;
    }
    pos += ret;

    if (handle->ctx.status == IAMF_DECODER_STATUS_RUN) break;
  }
  return pos;
}

int32_t iamf_decoder_internal_add_descrptor_OBU(IAMF_DecoderHandle handle,
                                                IAMF_OBU *obu) {
  IAMF_DataBase *db;
  IAMF_Object *obj;

  db = &handle->ctx.db;
  obj = IAMF_object_new(obu, 0);
  if (!obj) {
    ia_loge("fail to new object for %s(%d)", IAMF_OBU_type_string(obu->type),
            obu->type);
    return IAMF_ERR_ALLOC_FAIL;
  }

  return iamf_database_add_object(db, obj);
}

int iamf_decoder_internal_deliver(IAMF_DecoderHandle handle, IAMF_Frame *obj) {
  IAMF_DecoderContext *ctx = &handle->ctx;
  IAMF_DataBase *db = &ctx->db;
  IAMF_Presentation *pst = ctx->presentation;
  int idx = -1, i;
  IAMF_Stream *stream;
  IAMF_StreamDecoder *decoder;

  for (i = 0; i < pst->nb_streams; ++i) {
    idx = iamf_database_element_get_substream_index(
        db, pst->streams[i]->element_id, obj->id);
    if (idx > -1) {
      break;
    }
  }

  if (idx > -1) {
    stream = pst->streams[i];
    decoder = pst->decoders[i];

    if (idx == 0) {
      ia_logd("frame id %" PRIu64 " and its stream (%d) id %" PRIu64, obj->id,
              i, pst->streams[i]->element_id);

      if (obj->trim_start != stream->trimming_start) {
        stream->trimming_start = obj->trim_start;
      }

      if (obj->trim_end != stream->trimming_end) {
        stream->trimming_end = obj->trim_end;
      }

      if (obj->trim_start > 0)
        ia_logd("trimming start %" PRIu64 " ", obj->trim_start);
      if (obj->trim_end > 0) ia_logd("trimming end %" PRIu64, obj->trim_end);

#if 0
      if (decoder->packet.sub_packets[idx]) {
        /* when the frame of stream has been overwrite, the timestamp of stream
         * should be elapsed and the global time should be updated together. */

        stream->timestamp += decoder->frame_size;
      }
#endif
    }
    iamf_stream_decoder_receive_packet(decoder, idx, obj);
  }

  return 0;
}

static int iamf_target_layout_matching_calculation(TargetLayout *target,
                                                   LayoutInfo *layout) {
  SoundSystemLayout *ss;
  int s = 0;
  if (target->type == layout->layout.type) {
    if (layout->layout.type == IAMF_LAYOUT_TYPE_BINAURAL) {
      s = 100;
    } else if (target->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
      ss = SOUND_SYSTEM_LAYOUT(target);
      if (ss->sound_system == layout->layout.sound_system.sound_system) {
        s = 100;
      }
    }
  }

  if (!s) {
    s = 50;
    int chs = 0;
    if (target->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
      ss = SOUND_SYSTEM_LAYOUT(target);
      chs = IAMF_layout_sound_system_channels_count(ss->sound_system);
    } else if (target->type == IAMF_LAYOUT_TYPE_BINAURAL)
      chs = IAMF_layout_binaural_channels_count();
    if (layout->channels < chs) {
      s += (chs - layout->channels);
    } else {
      s -= (layout->channels - chs);
    }
  }

  return s;
}

static float iamf_mix_presentation_get_best_loudness(IAMF_MixPresentation *obj,
                                                     LayoutInfo *layout) {
  int score = 0, s, idx = INVALID_VALUE;
  SubMixPresentation *sub;
  float loudness_db = 0.f;

  if (obj->num_sub_mixes) {
    /* for (int n = 0; n < obj->num_sub_mixes; ++n) { */
    /* sub = &obj->sub_mixes[n]; */
    sub = &obj->sub_mixes[0];  // support only 1 sub mix.

    if (sub->num_layouts) {
      for (int i = 0; i < sub->num_layouts; ++i) {
        s = iamf_target_layout_matching_calculation(sub->layouts[i], layout);
        if (s > score) {
          score = s;
          idx = i;
        }
      }
      if (idx > INVALID_VALUE) {
        loudness_db = q_to_float(sub->loudness[idx].integrated_loudness, 8);
        ia_logi("selected loudness is %f db <- 0x%x", loudness_db,
                sub->loudness[idx].integrated_loudness & U16_MASK);
      }
    }
    /* } */
  }

  return loudness_db;
}

static int iamf_mix_presentation_matching_calculation(IAMF_DataBase *db,
                                                      IAMF_MixPresentation *obj,
                                                      LayoutInfo *layout) {
  int score = 0, s = 0;
  SubMixPresentation *sub;
  IAMF_Element *e = 0;

  if (obj->num_sub_mixes) {
    /* for (int n = 0; n < obj->num_sub_mixes; ++n) { */
    /* sub = &obj->sub_mixes[n]; */
    sub = &obj->sub_mixes[0];  // support only 1 sub mix.

    if (layout->layout.type == IAMF_LAYOUT_TYPE_BINAURAL) {
      for (int i = 0; i < sub->nb_elements; i++) {
        e = iamf_database_get_element(db, sub->conf_s[i].element_id);
        if (e && e->element_type == AUDIO_ELEMENT_TYPE_CHANNEL_BASED &&
            e->channels_conf && e->channels_conf->layer_conf_s &&
            e->channels_conf->layer_conf_s->loudspeaker_layout ==
                IA_CHANNEL_LAYOUT_BINAURAL)
          return 100;
      }
    }
    if (sub->num_layouts) {
      for (int i = 0; i < sub->num_layouts; ++i) {
        s = iamf_target_layout_matching_calculation(sub->layouts[i], layout);
        if (s > score) score = s;
      }
    }
    /* } */
  }

  return score;
}

static IAMF_MixPresentation *iamf_decoder_get_best_mix_presentation(
    IAMF_DecoderHandle handle) {
  IAMF_DecoderContext *ctx = &handle->ctx;
  IAMF_DataBase *db = &ctx->db;
  IAMF_MixPresentation *mp = 0, *obj;

  if (db->mixPresentation->count > 0) {
    if (db->mixPresentation->count == 1) {
      mp = IAMF_MIX_PRESENTATION(db->mixPresentation->items[0]);
    } else if (ctx->mix_presentation_id >= 0) {
      mp = iamf_database_get_mix_presentation(db, ctx->mix_presentation_id);
    }

    if (!mp) {
      int max_percentage = 0, sub_percentage;

      for (int i = 0; i < db->mixPresentation->count; ++i) {
        obj = IAMF_MIX_PRESENTATION(db->mixPresentation->items[i]);
        sub_percentage = iamf_mix_presentation_matching_calculation(
            db, obj, ctx->output_layout);
        if (max_percentage < sub_percentage) {
          max_percentage = sub_percentage;
          mp = obj;
        }
      }
    }
  }
  return mp;
}

static int iamf_decoder_enable_mix_presentation(IAMF_DecoderHandle handle,
                                                IAMF_MixPresentation *mixp) {
  IAMF_DecoderContext *ctx = &handle->ctx;
  IAMF_DataBase *db = &ctx->db;
  IAMF_Element *elem;
  IAMF_CodecConf *cc;
  IAMF_Presentation *old = ctx->presentation;
  IAMF_Presentation *pst;
  SubMixPresentation *sub;
  uint64_t pid;
  ParameterItem *pi = 0;
  int ret = IAMF_OK;

  pst = IAMF_MALLOCZ(IAMF_Presentation, 1);
  if (!pst) return IAMF_ERR_ALLOC_FAIL;

  pst->obj = mixp;
  pst->output_gain_id = INVALID_ID;
  ctx->presentation = pst;

  ia_logd("enable mix presentation id %" PRIu64 ", %p",
          mixp->mix_presentation_id, mixp);

  // There is only one sub mix in the mix presentation for simple and base
  // profiles. so the sub mix is selected the first.
  sub = mixp->sub_mixes;
  for (uint32_t i = 0; i < sub->nb_elements; ++i) {
    elem = iamf_database_get_element(db, sub->conf_s[i].element_id);
    cc = iamf_database_element_get_codec_conf(db, elem->element_id);
    pid = sub->conf_s[i].conf_m.gain.base.id;
    pi = iamf_database_parameter_get_item(db, pid);
    // the mix gain parameter may be used by multiple stream.
    if (!pi && iamf_database_parameter_add_item(
                   db, &sub->conf_s[i].conf_m.gain.base, INVALID_ID,
                   iamf_codec_conf_get_sampling_rate(cc)) == IAMF_OK) {
      float gain_db;
      pi = iamf_database_parameter_get_item(db, pid);
      gain_db = q_to_float(sub->conf_s[i].conf_m.gain.mix_gain, 8);
      pi->value.mix_gain.default_mix_gain = db2lin(gain_db);
      ia_logi("element %" PRIu64 " : mix gain %f (%f db) <- 0x%x",
              sub->conf_s[i].element_id, pi->value.mix_gain.default_mix_gain,
              gain_db, sub->conf_s[i].conf_m.gain.mix_gain & U16_MASK);
    }

    iamf_database_element_set_mix_gain_parameter(
        db, elem->element_id, sub->conf_s[i].conf_m.gain.base.id);
    if (!elem || !cc) continue;
    if (elem->obj.flags & cc->obj.flags & IAMF_OBU_FLAG_REDUNDANT) {
      if (iamf_presentation_reuse_stream(pst, old, elem->element_id) !=
          IAMF_OK) {
        ret = iamf_stream_enable(handle, elem);
      }
    } else {
      ret = iamf_stream_enable(handle, elem);
    }
    if (ret != IAMF_OK) return ret;
  }

  pst->frame.channels = ctx->output_layout->channels;
  iamf_set_stream_info(handle);
  iamf_mixer_init(handle);

  pid = sub->output_mix_config.gain.base.id;
  pi = iamf_database_parameter_get_item(db, pid);
  if (pi || iamf_database_parameter_add_item(
                db, &sub->output_mix_config.gain.base, INVALID_ID,
                ctx->sampling_rate) == IAMF_OK) {
    float gain_db;
    pi = iamf_database_parameter_get_item(db, pid);
    gain_db = q_to_float(sub->output_mix_config.gain.mix_gain, 8);
    pi->value.mix_gain.default_mix_gain = db2lin(gain_db);
    pst->output_gain_id = pi->id;
    ia_logi("output mix gain %f (%f db) <- 0x%x",
            pi->value.mix_gain.default_mix_gain, gain_db,
            sub->output_mix_config.gain.mix_gain & U16_MASK);
  }
  IAMF_Resampler *resampler = 0;
  if (old) {
    resampler = iamf_presentation_take_resampler(old);
  }
  IAMF_Stream *stream = pst->streams[0];
  if (!resampler && stream->sampling_rate != ctx->sampling_rate) {
    resampler = iamf_stream_resampler_open(stream, ctx->sampling_rate,
                                           SPEEX_RESAMPLER_QUALITY);
    if (!resampler) return IAMF_ERR_INTERNAL;
  }
  pst->resampler = resampler;

  if (old) iamf_presentation_free(old);

  return IAMF_OK;
}

/*
AOM-IAMF Standard Deliverable Status:
The following function is out of scope and not part of the IAMF Final Deliverable.
*/

static int iamf_loudness_process(float *block, int frame_size, int channels,
                                 float gain) {
  int idx = 0;
  if (!block || frame_size < 0 || channels < 0) return IAMF_ERR_BAD_ARG;

  if (!frame_size || gain == 1.0f) return IAMF_OK;

  for (int c = 0; c < channels; ++c) {
    idx = c * frame_size;
    for (int i = 0; i < frame_size; ++i) {
      block[idx + i] *= gain;
    }
  }

  return IAMF_OK;
}

static int iamf_resample(IAMF_Resampler *resampler, float *in, float *out,
                         int frame_size) {
  SpeexResamplerState *speex_resampler = resampler->speex_resampler;
  int resample_size =
      frame_size * (speex_resampler->out_rate / speex_resampler->in_rate + 1);
  ia_logt("input samples %d", frame_size);
  if (resampler->rest_flag == 2) {
    resample_size = speex_resampler_get_output_latency(speex_resampler);
    int input_size = speex_resampler_get_input_latency(speex_resampler);
    speex_resampler_process_interleaved_float(
        speex_resampler, (const float *)NULL, (uint32_t *)&input_size,
        (float *)in, (uint32_t *)&resample_size);
  } else {
    ia_decoder_plane2stride_out_float(resampler->buffer, in, frame_size,
                                      speex_resampler->nb_channels);
    speex_resampler_process_interleaved_float(
        speex_resampler, (const float *)resampler->buffer,
        (uint32_t *)&frame_size, (float *)in, (uint32_t *)&resample_size);
  }
  if (!resampler->rest_flag) {
    resampler->rest_flag = 1;
  }
  ia_decoder_stride2plane_out_float(out, in, resample_size,
                                    speex_resampler->nb_channels);
  ia_logt("read samples %d, output samples %d", frame_size, resample_size);
  return resample_size;
}

/**
 * @brief     Handle the buffer delay from limiter or resampler.
 * @param     [in] handle : the iamf decoder handle
 * @param     [in] pcm : the audio pcm.
 * @return    the number of samples after hanlding
 */
static int iamf_delay_buffer_handle(IAMF_DecoderHandle handle, void *pcm) {
  IAMF_DecoderContext *ctx = &handle->ctx;
  IAMF_Presentation *pst = ctx->presentation;
  IAMF_Resampler *resampler = pst->resampler;
  AudioEffectPeakLimiter *limiter = handle->limiter;
  int frame_size = limiter ? limiter->delaySize : 0;
  int buffer_size = ctx->info.max_frame_size * ctx->output_layout->channels;
  float *in, *out;

  if (!limiter && !resampler) return 0;

  in = IAMF_MALLOCZ(float, buffer_size);
  out = IAMF_MALLOCZ(float, buffer_size);

  if (!in || !out) {
    if (in) free(in);
    if (out) free(out);
    return IAMF_ERR_ALLOC_FAIL;
  }

  if (resampler) {
    resampler->rest_flag = 2;
    int resample_size = iamf_resample(resampler, in, out, 0);
    frame_size += resample_size;
    swap((void **)&in, (void **)&out);
    memset(out, 0, sizeof(float) * buffer_size);
    for (int c = 0; c < resampler->speex_resampler->nb_channels; c++) {
      memcpy(out + c * frame_size, in + c * resample_size,
             sizeof(float) * resample_size);
    }
    swap((void **)&in, (void **)&out);
  }

  if (limiter) {
    frame_size =
        audio_effect_peak_limiter_process_block(limiter, in, out, frame_size);
  }

  iamf_decoder_plane2stride_out(pcm, out, frame_size,
                                ctx->output_layout->channels, ctx->bit_depth);
  free(in);
  free(out);
  return frame_size;
}

static int iamf_decoder_internal_decode(IAMF_DecoderHandle handle,
                                        const uint8_t *data, int32_t size,
                                        uint32_t *rsize, void *pcm) {
  IAMF_DecoderContext *ctx = &handle->ctx;
  IAMF_DataBase *db = &ctx->db;
  IAMF_Presentation *pst = ctx->presentation;
  IAMF_StreamDecoder *decoder;
  IAMF_StreamRenderer *renderer;
  IAMF_Stream *stream;
  IAMF_Mixer *mixer = &pst->mixer;
  IAMF_Resampler *resampler = pst->resampler;
  Frame *f;
  int ret = 0, lret = 1;
  uint32_t r = 0;
  int real_frame_size = 0;
  float *out = 0;
  MixGainUnit *u = 0;
  ElementItem *ei = 0;

  if (pst->nb_streams <= 0) return IAMF_ERR_INTERNAL;

  if (data && size) {
    r = iamf_decoder_internal_parse_OBUs(handle, data, size);
    *rsize = r;

    if (ctx->status == IAMF_DECODER_STATUS_RECONFIGURE) {
      return IAMF_ERR_INVALID_STATE;
    } else if (ctx->status != IAMF_DECODER_STATUS_RUN) {
      return 0;
    }
  }

  if ((data && size) || pst->decoders[0]->delay > 0) {
    for (int s = 0; s < pst->nb_streams; ++s) {
      renderer = pst->renderers[s];
      decoder = pst->decoders[s];
      stream = decoder->stream;
      f = &decoder->frame;
      f->data = decoder->buffers[0];
      out = decoder->buffers[1];

      f->pts = stream->timestamp;
      if (decoder->delay > 0) f->pts -= decoder->delay;

      ret = iamf_stream_decoder_decode(decoder, f->data);
      iamf_stream_decoder_decode_finish(decoder);

      if (ret > 0) {
        ia_logd("strim %" PRIu64 ", etrim %" PRIu64
                ", frame size %u, delay size %d",
                f->strim, f->etrim, decoder->frame_size, decoder->delay);
        if (f->strim == decoder->frame_size ||
            f->etrim == decoder->frame_size) {
          ret = 0;
          ia_logd("The whole frame which size is %d has been cut.", f->samples);
        } else {
          f->samples = ret;

#if SR
          // decoding
          iamf_rec_stream_log(stream->element_id, f->channels, f->data, ret);
#endif

          if (decoder->frame_padding > 0) {
            ia_logw("decoded result is %d, different frame size %d",
                    ret - decoder->frame_padding, decoder->frame_size);
            f->etrim += decoder->frame_padding;
          }

          renderer->offset = decoder->delay > 0 ? decoder->delay : 0;
          if (stream->trimming_start) renderer->offset = 0;
          iamf_stream_render(renderer, f->data, out, ret);

#if SR
          // rendering
          iamf_ren_stream_log(stream->element_id,
                              stream->final_layout->channels, out, ret);
#endif

          swap((void **)&f->data, (void **)&out);
          f->channels = ctx->output_layout->channels;

          if (!data || size <= 0) {
            f->etrim = decoder->frame_size - decoder->delay;
            decoder->delay = 0;
          }

          if ((f->strim && f->strim < decoder->frame_size) ||
              (f->etrim && f->etrim < decoder->frame_size) ||
              stream->trimming_start) {
            if (f->etrim > 0 && decoder->delay > 0) {
              if (decoder->delay > f->etrim) {
                decoder->delay -= f->etrim;
                f->etrim = 0;
              } else {
                f->etrim -= decoder->delay;
                decoder->delay = 0;
              }
            }
            ret = iamf_frame_trim(f, f->strim, f->etrim,
                                  stream->trimming_start - f->strim);

            ia_logd("The remaining samples %d after cutting.", ret);
          }
        }
      }

      if (!s && f->strim > 0) {
        ia_logd("external pts is %" PRId64, ctx->pts);
        ctx->pts +=
            time_transform(f->strim, stream->sampling_rate, ctx->pts_time_base);
        ia_logd("external pts changes to %" PRId64, ctx->pts);
      }

      if (ret <= 0) {
        if (ret < 0) ia_loge("fail to decode audio packet. error no. %d", ret);
        stream->timestamp += decoder->frame_size;
        lret = ret;
        continue;
      }
      real_frame_size = ret;

      ei = iamf_database_element_get_item(db, stream->element_id);
      if (ei && ei->mixGain) {
        u = iamf_database_parameter_get_mix_gain_unit(
            db, ei->mixGain->id, f->pts, f->samples, stream->sampling_rate);
        if (u) {
          iamf_frame_gain(f, u);
          mix_gain_unit_free(u);
        }
      }

      // metadata
      if (decoder->stream->scheme == AUDIO_ELEMENT_TYPE_CHANNEL_BASED &&
          ctx->metadata.param) {
        IAMF_Stream *stream = decoder->stream;
        ChannelLayerContext *cctx = (ChannelLayerContext *)stream->priv;
        if (cctx->dmx_mode >= 0)
          ctx->metadata.param->dmixp_mode = cctx->dmx_mode;
      }

      iamf_mixer_add_frame(mixer, f);

      // timestamp
      stream->timestamp += decoder->frame_size;

      pst->frame.data = out;
      out = f->data;
    }

    if (lret <= 0) {
      ctx->status = IAMF_DECODER_STATUS_RECEIVE;
      return lret;
    }

    f = &pst->frame;
    real_frame_size = iamf_mixer_mix(mixer, f);

    ia_logd("frame pts %" PRIu64 ", id %" PRIu64, f->pts, pst->output_gain_id);

    u = iamf_database_parameter_get_mix_gain_unit(
        db, pst->output_gain_id, f->pts, f->samples,
        pst->streams[0]->sampling_rate);
    if (u) {
      iamf_frame_gain(f, u);
      mix_gain_unit_free(u);
    }

    iamf_database_parameters_time_elapse(db, real_frame_size,
                                         pst->streams[0]->sampling_rate);

    if (resampler) {
      real_frame_size = iamf_resample(resampler, f->data, out, real_frame_size);
      swap((void **)&f->data, (void **)&out);
    }

    if (ctx->normalization_loudness) {
      iamf_loudness_process(
          f->data, real_frame_size, ctx->output_layout->channels,
          db2lin(ctx->normalization_loudness - ctx->loudness));
    }

    if (handle->limiter) {
      real_frame_size = audio_effect_peak_limiter_process_block(
          handle->limiter, f->data, out, real_frame_size);
      swap((void **)&f->data, (void **)&out);
    }

    iamf_decoder_plane2stride_out(pcm, f->data, real_frame_size,
                                  ctx->output_layout->channels, ctx->bit_depth);

#if SR
    // mixing
    iamf_mix_stream_log(ctx->output_layout->channels, out, real_frame_size);
#endif
  }

  if (!data) {
    if (real_frame_size > 0)
      pcm = (char *)pcm +
            ctx->bit_depth / 8 * real_frame_size * ctx->output_layout->channels;
    real_frame_size += iamf_delay_buffer_handle(handle, pcm);
  }

  ctx->duration += real_frame_size;
  ctx->last_frame_size = real_frame_size;
  ctx->status = IAMF_DECODER_STATUS_RECEIVE;
  return real_frame_size;
}

static LayoutInfo *iamf_layout_info_new_sound_system(IAMF_SoundSystem ss) {
  IAMF_PREDEFINED_SP_LAYOUT *l;
  LayoutInfo *t = 0;

  t = IAMF_MALLOCZ(LayoutInfo, 1);
  if (!t) {
    ia_loge("fail to allocate memory to Layout.");
    return t;
  }

  t->layout.sound_system.type = IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION;
  t->layout.sound_system.sound_system = ss;
  t->channels = IAMF_layout_sound_system_channels_count(ss);
  t->sp.sp_type = 0;
  l = IAMF_MALLOCZ(IAMF_PREDEFINED_SP_LAYOUT, 1);
  if (!l) {
    ia_loge("fail to allocate memory to Predefined SP Layout.");
    if (t) free(t);
    return 0;
  }
  l->system = iamf_sound_system_get_rendering_id(ss);
  l->lfe1 = iamf_sound_system_lfe1(ss);
  l->lfe2 = iamf_sound_system_lfe2(ss);

  t->sp.sp_layout.predefined_sp = l;

  return t;
}

static LayoutInfo *iamf_layout_info_new_binaural() {
  IAMF_PREDEFINED_SP_LAYOUT *l;
  LayoutInfo *t = 0;

  t = IAMF_MALLOCZ(LayoutInfo, 1);
  if (!t) {
    ia_loge("fail to allocate memory to Layout.");
    return t;
  }

  t->layout.binaural.type = IAMF_LAYOUT_TYPE_BINAURAL;
  t->channels = IAMF_layout_binaural_channels_count();
  t->sp.sp_type = 0;
  l = IAMF_MALLOCZ(IAMF_PREDEFINED_SP_LAYOUT, 1);
  if (!l) {
    ia_loge("fail to allocate memory to Predefined SP Layout.");
    if (t) free(t);
    return 0;
  }
  l->system = IAMF_BINAURAL;
  l->lfe1 = 0;
  l->lfe2 = 0;

  t->sp.sp_layout.predefined_sp = l;

  return t;
}

static void iamf_extra_data_dump(IAMF_extradata *metadata) {
  ia_logt("metadata: target layout >");

  ia_logt("metadata: sound system %u", metadata->output_sound_system);
  ia_logt("metadata: number of samples %u", metadata->number_of_samples);
  ia_logt("metadata: bitdepth %u", metadata->bitdepth);
  ia_logt("metadata: sampling rate %u", metadata->sampling_rate);
  ia_logt("metadata: sound mode %d", metadata->output_sound_mode);
  ia_logt("metadata: number loudness layout %d ",
          metadata->num_loudness_layouts);

  for (int i = 0; i < metadata->num_loudness_layouts; ++i) {
    ia_logt("metadata: loudness layout %d >", i);
    iamf_layout_dump(&metadata->loudness_layout[i]);

    ia_logt("metadata: loudness info %d >", i);
    ia_logt("\tinfo type %u", metadata->loudness[i].info_type & U8_MASK);
    ia_logt("\tintegrated loudness 0x%x",
            metadata->loudness[i].integrated_loudness & U16_MASK);
    ia_logt("\tdigital peak 0x%d",
            metadata->loudness[i].digital_peak & U16_MASK);
    if (metadata->loudness[i].info_type & 1)
      ia_logt("\ttrue peak %d", metadata->loudness[i].true_peak);
  }
  ia_logt("metadata: number parameters %d ", metadata->num_parameters);

  for (int i = 0; i < metadata->num_parameters; ++i) {
    ia_logt("parameter size %d", metadata->param[i].parameter_length);
    ia_logt("parameter type %d", metadata->param[i].parameter_definition_type);
    if (metadata->param[i].parameter_definition_type ==
        IAMF_PARAMETER_TYPE_DEMIXING)
      ia_logt("demix mode %d", metadata->param[i].dmixp_mode);
  }
}

static int iamf_extra_data_init(IAMF_DecoderHandle handle) {
  IAMF_DecoderContext *ctx = &handle->ctx;
  IAMF_Presentation *pst = ctx->presentation;
  IAMF_extradata *metadata = &ctx->metadata;

  ia_logt("initialize iamf extra data.");
  metadata->output_sound_system =
      iamf_layout_get_sound_system(&ctx->output_layout->layout);
  metadata->bitdepth = ctx->bit_depth;
  metadata->sampling_rate = OUTPUT_SAMPLERATE;
  metadata->output_sound_mode = iamf_presentation_get_output_sound_mode(pst);

  ia_logt("mix presentation %p", ctx->presentation->obj);
  metadata->num_loudness_layouts =
      ctx->presentation->obj->sub_mixes->num_layouts;
  metadata->loudness_layout =
      IAMF_MALLOCZ(IAMF_Layout, metadata->num_loudness_layouts);
  metadata->loudness =
      IAMF_MALLOCZ(IAMF_LoudnessInfo, metadata->num_loudness_layouts);
  if (!metadata->loudness_layout || !metadata->loudness)
    return IAMF_ERR_ALLOC_FAIL;
  for (int i = 0; i < metadata->num_loudness_layouts; ++i) {
    iamf_layout_copy2(&metadata->loudness_layout[i],
                      ctx->presentation->obj->sub_mixes->layouts[i]);
  }
  memcpy(metadata->loudness, pst->obj->sub_mixes->loudness,
         sizeof(IAMF_LoudnessInfo) * metadata->num_loudness_layouts);

  if (pst) {
    ElementItem *ei;
    for (int i = 0; i < pst->obj->sub_mixes->nb_elements; ++i) {
      ei = iamf_database_element_get_item(
          &ctx->db, pst->obj->sub_mixes->conf_s[i].element_id);
      if (ei && ei->demixing) {
        metadata->num_parameters = 1;
        metadata->param = IAMF_MALLOCZ(IAMF_Param, 1);
        if (!metadata->param) return IAMF_ERR_ALLOC_FAIL;
        metadata->param->parameter_length = 8;
        metadata->param->parameter_definition_type =
            IAMF_PARAMETER_TYPE_DEMIXING;
        break;
      }
    }
  }

  iamf_extra_data_dump(metadata);
  return IAMF_OK;
}

static int iamf_extra_data_copy(IAMF_extradata *dst, IAMF_extradata *src) {
  if (!src) return IAMF_ERR_BAD_ARG;

  if (!dst) return IAMF_ERR_INTERNAL;

  dst->output_sound_system = src->output_sound_system;
  dst->number_of_samples = src->number_of_samples;
  dst->bitdepth = src->bitdepth;
  dst->sampling_rate = src->sampling_rate;
  dst->num_loudness_layouts = src->num_loudness_layouts;
  dst->output_sound_mode = src->output_sound_mode;

  if (dst->num_loudness_layouts) {
    dst->loudness_layout = IAMF_MALLOCZ(IAMF_Layout, dst->num_loudness_layouts);
    dst->loudness = IAMF_MALLOCZ(IAMF_LoudnessInfo, dst->num_loudness_layouts);

    if (!dst->loudness_layout || !dst->loudness) return IAMF_ERR_ALLOC_FAIL;
    for (int i = 0; i < dst->num_loudness_layouts; ++i) {
      iamf_layout_copy(&dst->loudness_layout[i], &src->loudness_layout[i]);
      memcpy(&dst->loudness[i], &src->loudness[i], sizeof(IAMF_LoudnessInfo));
    }
  } else {
    dst->loudness_layout = 0;
    dst->loudness = 0;
  }

  dst->num_parameters = src->num_parameters;

  if (dst->num_parameters) {
    dst->param = IAMF_MALLOCZ(IAMF_Param, dst->num_parameters);
    if (!dst->param) return IAMF_ERR_ALLOC_FAIL;
    for (int i = 0; i < src->num_parameters; ++i)
      memcpy(&dst->param[i], &src->param[i], sizeof(IAMF_Param));
  } else {
    dst->param = 0;
  }

  return IAMF_OK;
}

void iamf_extra_data_reset(IAMF_extradata *data) {
  if (data) {
    if (data->loudness_layout) {
      for (int i = 0; i < data->num_loudness_layouts; ++i)
        iamf_layout_reset(&data->loudness_layout[i]);

      free(data->loudness_layout);
    }

    if (data->loudness) free(data->loudness);
    if (data->param) free(data->param);

    memset(data, 0, sizeof(IAMF_extradata));
    data->output_sound_mode = IAMF_SOUND_MODE_NONE;
  }
}
/* ----------------------------- APIs ----------------------------- */

IAMF_DecoderHandle IAMF_decoder_open(void) {
  IAMF_DecoderHandle handle = 0;
  handle = IAMF_MALLOCZ(struct IAMF_Decoder, 1);
  if (handle) {
    IAMF_DataBase *db = &handle->ctx.db;

    handle->ctx.time_precision = OUTPUT_SAMPLERATE;
    handle->ctx.threshold_db = LIMITER_MaximumTruePeak;
    handle->ctx.loudness = 1.0f;
    handle->ctx.sampling_rate = OUTPUT_SAMPLERATE;
    handle->ctx.status = IAMF_DECODER_STATUS_INIT;
    handle->ctx.mix_presentation_id = INVALID_ID;
    handle->limiter = audio_effect_peak_limiter_create();
    if (!handle->limiter || iamf_database_init(db) != IAMF_OK) {
      IAMF_decoder_close(handle);
      handle = 0;
    }
  }
  return handle;
}

int IAMF_decoder_close(IAMF_DecoderHandle handle) {
  if (handle) {
    iamf_decoder_internal_reset(handle);
    if (handle->limiter) audio_effect_peak_limiter_destroy(handle->limiter);
    free(handle);
  }
#if SR
  iamf_stream_log_free();
#endif

  return 0;
}

int iamf_decoder_internal_configure(IAMF_DecoderHandle handle,
                                    const uint8_t *data, uint32_t size,
                                    uint32_t *rsize) {
  int ret = IAMF_OK;
  IAMF_DecoderContext *ctx;
  IAMF_DataBase *db;

  ia_logt("handle %p, data %p, size %d", handle, data, size);

  if (!handle) return IAMF_ERR_BAD_ARG;

  ctx = &handle->ctx;
  db = &ctx->db;
  if (ctx->need_configure & IAMF_DECODER_CONFIG_OUTPUT_LAYOUT) {
    LayoutInfo *old = ctx->output_layout;
    ia_logd("old layout %p", old);
    if (ctx->layout.type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION)
      ctx->output_layout = iamf_layout_info_new_sound_system(
          ctx->layout.sound_system.sound_system);
    else if (ctx->layout.type == IAMF_LAYOUT_TYPE_BINAURAL)
      ctx->output_layout = iamf_layout_info_new_binaural();

    ia_logd("new layout %p", ctx->output_layout);
    if (!ctx->output_layout) {
      ret = IAMF_ERR_ALLOC_FAIL;
      ctx->output_layout = old;
    } else if (old) {
      if (old == ctx->output_layout)
        ret = IAMF_ERR_INTERNAL;
      else
        iamf_layout_info_free(old);
    }

    if (ret < 0) return IAMF_ERR_INTERNAL;
  }

  if (data && size > 0) {
    if (ctx->status == IAMF_DECODER_STATUS_INIT)
      ctx->status = IAMF_DECODER_STATUS_CONFIGURE;
    else if (ctx->status == IAMF_DECODER_STATUS_RECEIVE)
      ctx->status = IAMF_DECODER_STATUS_RECONFIGURE;

    if (ctx->status == IAMF_DECODER_STATUS_RECONFIGURE) {
      ia_logi("Reconfigure decoder.");
      iamf_database_reset(db);
      iamf_database_init(db);
      iamf_extra_data_reset(&ctx->metadata);
      ctx->status = IAMF_DECODER_STATUS_CONFIGURE;
    }

    if (handle->limiter) {
      ia_logi("Initialize limiter.");
      audio_effect_peak_limiter_init(
          handle->limiter, handle->ctx.threshold_db, OUTPUT_SAMPLERATE,
          iamf_layout_channels_count(&handle->ctx.output_layout->layout),
          LIMITER_AttackSec, LIMITER_ReleaseSec, LIMITER_LookAhead);
    }
    ret = iamf_decoder_internal_init(handle, data, size, rsize);

    if (ret == IAMF_OK) ctx->need_configure = 0;
  } else if (ctx->need_configure) {
    if (ctx->status < IAMF_DECODER_STATUS_RECEIVE) {
      ia_loge("Decoder need configure with descriptor obus.");
      return IAMF_ERR_BAD_ARG;
    }

    if (ctx->need_configure & IAMF_DECODER_CONFIG_MIX_PRESENTATION) {
      if (!ctx->presentation) {
        ret = IAMF_ERR_INTERNAL;
      } else if (ctx->mix_presentation_id !=
                     ctx->presentation->obj->mix_presentation_id &&
                 !iamf_database_get_mix_presentation(
                     db, ctx->mix_presentation_id)) {
        ia_logw("Invalid mix presentation id %lu.", ctx->mix_presentation_id);
        ret = IAMF_ERR_INTERNAL;
      }
    }

    if (ctx->need_configure & IAMF_DECODER_CONFIG_OUTPUT_LAYOUT) {
      if (handle->limiter) {
        audio_effect_peak_limiter_uninit(handle->limiter);
        audio_effect_peak_limiter_init(
            handle->limiter, handle->ctx.threshold_db, OUTPUT_SAMPLERATE,
            iamf_layout_channels_count(&handle->ctx.output_layout->layout),
            LIMITER_AttackSec, LIMITER_ReleaseSec, LIMITER_LookAhead);
      }
    }

    ctx->need_configure = 0;
  } else {
    ia_loge("Decoder need configure with descriptor obus.");
    return IAMF_ERR_BAD_ARG;
  }

  if (ret == IAMF_OK) {
    IAMF_MixPresentation *mixp = iamf_decoder_get_best_mix_presentation(handle);
    ia_logi("get mix presentation id %" PRId64,
            mixp ? mixp->mix_presentation_id : INVALID_ID);
    if (mixp) {
      ret = iamf_decoder_enable_mix_presentation(handle, mixp);
      if (ret == IAMF_OK) {
        iamf_extra_data_init(handle);
        handle->ctx.status = IAMF_DECODER_STATUS_RECEIVE;
      }
      handle->ctx.loudness = iamf_mix_presentation_get_best_loudness(
          mixp, handle->ctx.output_layout);
      iamf_database_parameters_clear_segments(db);
    } else {
      ret = IAMF_ERR_INVALID_PACKET;
      ia_loge("Fail to find the mix presentation obu, try again.");
    }
  }

  return ret;
}

int IAMF_decoder_configure(IAMF_DecoderHandle handle, const uint8_t *data,
                           uint32_t size, uint32_t *rsize) {
  uint32_t rs = 0;
  int ret = iamf_decoder_internal_configure(handle, data, size, &rs);

  if (rsize) {
    *rsize = rs;
    return ret;
  }

  if (ret == IAMF_ERR_BUFFER_TOO_SMALL &&
      !(~handle->ctx.flags & IAMF_FLAG_DESCRIPTORS)) {
    handle->ctx.flags |= IAMF_FLAG_CONFIG;
    handle->ctx.need_configure = IAMF_DECODER_CONFIG_PRESENTATION;
    handle->ctx.status = IAMF_DECODER_STATUS_RECEIVE;
    ia_logd("configure with complete descriptor OBUs.");
    ret = iamf_decoder_internal_configure(handle, 0, 0, 0);
  }

  return ret;
}

int IAMF_decoder_decode(IAMF_DecoderHandle handle, const uint8_t *data,
                        int32_t size, uint32_t *rsize, void *pcm) {
  uint32_t rs = 0;
  int ret = IAMF_OK;

  if (!handle) return IAMF_ERR_BAD_ARG;
  if (handle->ctx.status != IAMF_DECODER_STATUS_RECEIVE)
    return IAMF_ERR_INVALID_STATE;
  ret = iamf_decoder_internal_decode(handle, data, size, &rs, pcm);
  if (rsize) *rsize = rs;
  return ret;
}

int IAMF_decoder_output_layout_set_sound_system(IAMF_DecoderHandle handle,
                                                IAMF_SoundSystem ss) {
  IAMF_DecoderContext *ctx;
  if (!handle) return IAMF_ERR_BAD_ARG;
  if (!iamf_sound_system_valid(ss)) return IAMF_ERR_BAD_ARG;

  ctx = &handle->ctx;
  if (ctx->layout.type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION &&
      ctx->layout.sound_system.sound_system == ss)
    return IAMF_OK;

  ia_logd("sound system %d, channels %d", ss,
          IAMF_layout_sound_system_channels_count(ss));

  ctx->layout.type = IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION;
  ctx->layout.sound_system.sound_system = ss;
  ctx->need_configure |= IAMF_DECODER_CONFIG_OUTPUT_LAYOUT;
  return 0;
}

int IAMF_decoder_output_layout_set_binaural(IAMF_DecoderHandle handle) {
  IAMF_DecoderContext *ctx;

  if (!handle) return IAMF_ERR_BAD_ARG;

  ctx = &handle->ctx;
  if (ctx->layout.type == IAMF_LAYOUT_TYPE_BINAURAL) return IAMF_OK;

  ia_logd("binaural channels %d", IAMF_layout_binaural_channels_count());
  ctx->layout.type = IAMF_LAYOUT_TYPE_BINAURAL;
  ctx->need_configure |= IAMF_DECODER_CONFIG_OUTPUT_LAYOUT;
  return 0;
}

int IAMF_decoder_set_mix_presentation_id(IAMF_DecoderHandle handle,
                                         uint64_t id) {
  IAMF_DecoderContext *ctx;

  if (!handle) return IAMF_ERR_BAD_ARG;

  ctx = &handle->ctx;

  if (ctx->mix_presentation_id == id) return IAMF_OK;

  ctx->mix_presentation_id = id;
  ctx->need_configure |= IAMF_DECODER_CONFIG_MIX_PRESENTATION;
  ia_logd("set new mix presentation id %" PRIu64 ".", id);
  return IAMF_OK;
}

int IAMF_layout_sound_system_channels_count(IAMF_SoundSystem ss) {
  int ret = 0;
  if (!iamf_sound_system_valid(ss)) {
    return IAMF_ERR_BAD_ARG;
  }
  ret = iamf_sound_system_channels_count_without_lfe(ss);
  ret += iamf_sound_system_lfe1(ss);
  ret += iamf_sound_system_lfe2(ss);
  ia_logd("sound system %x, channels %d", ss, ret);
  return ret;
}

int IAMF_layout_binaural_channels_count() { return 2; }

static int append_codec_string(char *buffer, int codec_index) {
  char codec[10] = "";
  switch (codec_index) {
    case 1:
      strcpy(codec, "Opus");
      break;
    case 2:
      strcpy(codec, "mp4a.40.2");
      break;
    case 3:
      strcpy(codec, "ipcm");
      break;
    case 4:
      strcpy(codec, "fLaC");
      break;
    default:
      return 0;
  }
  strcat(buffer, "iamf.");
  strcat(buffer, STR(IAMF_RIMARY_PROFILE));
  strcat(buffer, ".");
  strcat(buffer, STR(IAMF_ADDITIONAL_PROFILE));
  strcat(buffer, ".");
  strcat(buffer, codec);
  return 0;
}
char *IAMF_decoder_get_codec_capability() {
  int flag = 0, index = 0, max_len = 1024;
  int first = 1;
  char *list = IAMF_MALLOCZ(char, max_len);

#ifdef CONFIG_OPUS_CODEC
  flag |= 0x1;
#endif

#ifdef CONFIG_AAC_CODEC
  flag |= 0x2;
#endif

  // IPCM
  flag |= 0x4;

#ifdef CONFIG_FLAC_CODEC
  flag |= 0x8;
#endif

  while (flag) {
    index++;
    if (flag & 0x1) {
      if (!first) {
        strcat(list, ";");
      }
      append_codec_string(list, index);
      first = 0;
    }
    flag >>= 1;
  }

  return list;
}

int IAMF_decoder_set_normalization_loudness(IAMF_DecoderHandle handle,
                                            float loudness) {
  if (!handle) return IAMF_ERR_BAD_ARG;
  handle->ctx.normalization_loudness = loudness;
  return IAMF_OK;
}

int IAMF_decoder_set_bit_depth(IAMF_DecoderHandle handle, uint32_t bit_depth) {
  if (!handle) return IAMF_ERR_BAD_ARG;
  handle->ctx.bit_depth = bit_depth;
  return IAMF_OK;
}

int IAMF_decoder_peak_limiter_enable(IAMF_DecoderHandle handle,
                                     uint32_t enable) {
  if (!handle) return IAMF_ERR_BAD_ARG;
  if (!!enable && !handle->limiter) {
    handle->limiter = audio_effect_peak_limiter_create();
    if (!handle->limiter) return IAMF_ERR_ALLOC_FAIL;
  } else if (!enable && handle->limiter) {
    audio_effect_peak_limiter_destroy(handle->limiter);
    handle->limiter = 0;
  }

  return IAMF_OK;
}

int IAMF_decoder_peak_limiter_set_threshold(IAMF_DecoderHandle handle,
                                            float db) {
  if (!handle) return IAMF_ERR_BAD_ARG;
  handle->ctx.threshold_db = db;
  return IAMF_OK;
}

float IAMF_decoder_peak_limiter_get_threshold(IAMF_DecoderHandle handle) {
  if (!handle) return LIMITER_MaximumTruePeak;
  return handle->ctx.threshold_db;
}

int IAMF_decoder_set_sampling_rate(IAMF_DecoderHandle handle, uint32_t rate) {
  uint32_t sampling_rates[] = {8000, 12000, 16000, 24000, 32000, 44100, 48000};
  int ret = IAMF_ERR_BAD_ARG;

  if (!handle) return IAMF_ERR_BAD_ARG;

  for (int i = 0; i < sizeof(sampling_rates) / sizeof(uint32_t); ++i) {
    if (rate == sampling_rates[i]) {
      if (rate != handle->ctx.sampling_rate) handle->ctx.sampling_rate = rate;
      ret = IAMF_OK;
      break;
    }
  }
  return ret;
}

IAMF_StreamInfo *IAMF_decoder_get_stream_info(IAMF_DecoderHandle handle) {
  return &handle->ctx.info;
}

int IAMF_decoder_set_pts(IAMF_DecoderHandle handle, int64_t pts,
                         uint32_t time_base) {
  IAMF_DecoderContext *ctx;
  if (!handle) return IAMF_ERR_BAD_ARG;

  ctx = &handle->ctx;
  ctx->pts = pts;
  ctx->pts_time_base = time_base;
  ctx->duration = 0;
  ia_logd("set pts %" PRId64 "/%u", pts, time_base);

  return IAMF_OK;
}

int IAMF_decoder_get_last_metadata(IAMF_DecoderHandle handle, int64_t *pts,
                                   IAMF_extradata *metadata) {
  IAMF_DecoderContext *ctx;
  int64_t d;
  if (!handle || !pts || !metadata) return IAMF_ERR_BAD_ARG;

  ctx = &handle->ctx;
  d = time_transform(ctx->duration - ctx->last_frame_size, ctx->sampling_rate,
                     ctx->pts_time_base);
  *pts = ctx->pts + d;
  ia_logd("pts %" PRId64 "/%u, last duration %" PRIu64 "/%d", *pts,
          ctx->pts_time_base, ctx->duration - ctx->last_frame_size,
          ctx->sampling_rate);

  iamf_extra_data_copy(metadata, &ctx->metadata);
  metadata->number_of_samples = ctx->last_frame_size;
  iamf_extra_data_dump(metadata);
  return IAMF_OK;
}
