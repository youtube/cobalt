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
 * @file downmix_renderer.c
 * @brief DMRenderer.
 * @version 0.1
 * @date Created 06/21/2023
 **/

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IAMF_DMR"

#include "downmix_renderer.h"

#include "IAMF_debug.h"
#include "IAMF_utils.h"
#include "fixedp11_5.h"

typedef struct DependOnChannel {
  IAChannel ch;
  float s;
  float *sp;
} DependOnChannel;

struct Downmixer {
  int mode;
  int w_idx;
  IAChannel chs_in[IA_CH_LAYOUT_MAX_CHANNELS];
  IAChannel chs_out[IA_CH_LAYOUT_MAX_CHANNELS];
  int chs_icount;
  int chs_ocount;
  float *chs_data[IA_CH_COUNT];
  DependOnChannel *deps[IA_CH_COUNT];
  MixFactors mix_factors;
};

static DependOnChannel chmono[] = {{IA_CH_R2, 0.5f}, {IA_CH_L2, 0.5}, {0}};
static DependOnChannel chl2[] = {{IA_CH_L3, 1.f}, {IA_CH_C, 0.707}, {0}};
static DependOnChannel chr2[] = {{IA_CH_R3, 1.f}, {IA_CH_C, 0.707}, {0}};
static DependOnChannel chtl[] = {{IA_CH_HL, 1.f}, {IA_CH_SL5}, {0}};
static DependOnChannel chtr[] = {{IA_CH_HR, 1.f}, {IA_CH_SR5}, {0}};
static DependOnChannel chl3[] = {{IA_CH_L5, 1.f}, {IA_CH_SL5}, {0}};
static DependOnChannel chr3[] = {{IA_CH_R5, 1.f}, {IA_CH_SR5}, {0}};
static DependOnChannel chsl5[] = {{IA_CH_SL7}, {IA_CH_BL7}, {0}};
static DependOnChannel chsr5[] = {{IA_CH_SR7}, {IA_CH_BR7}, {0}};
static DependOnChannel chhl[] = {{IA_CH_HFL, 1.f}, {IA_CH_HBL}, {0}};
static DependOnChannel chhr[] = {{IA_CH_HFR, 1.f}, {IA_CH_HBR}, {0}};

static int _valid_channel_layout(IAChannelLayoutType in) {
  return ia_channel_layout_type_check(in) && in != IA_CHANNEL_LAYOUT_BINAURAL;
}

static int _valid_downmix(IAChannelLayoutType in, IAChannelLayoutType out) {
  uint32_t s1, s2, t1, t2;

  s1 = ia_channel_layout_get_category_channels_count(in, IA_CH_CATE_SURROUND);
  s2 = ia_channel_layout_get_category_channels_count(out, IA_CH_CATE_SURROUND);
  t1 = ia_channel_layout_get_category_channels_count(in, IA_CH_CATE_TOP);
  t2 = ia_channel_layout_get_category_channels_count(out, IA_CH_CATE_TOP);

  if (t1 && !t2) return 0;
  return !(s1 < s2 || t1 < t2);
}

static void _downmix_dump(DMRenderer *thisp, IAChannel c) {
  DependOnChannel *cs = thisp->deps[c];
  if (thisp->chs_data[c]) return;
  if (!thisp->deps[c]) {
    ia_loge("channel %s(%d) can not be found.", ia_channel_name(c), c);
    return;
  }

  while (cs->ch) {
    if (cs->sp) {
      ia_logd("channel %s(%d), scale point %f%s", ia_channel_name(cs->ch),
              cs->ch, *cs->sp, thisp->chs_data[cs->ch] ? ", s." : " m.");
      _downmix_dump(thisp, cs->ch);
    } else {
      ia_logd("channel %s(%d), scale %f%s", ia_channel_name(cs->ch), cs->ch,
              cs->s, thisp->chs_data[cs->ch] ? ", s." : " m.");
      _downmix_dump(thisp, cs->ch);
    }
    ++cs;
  }
}

static float _downmix_channel_data(DMRenderer *thisp, IAChannel c, int i) {
  DependOnChannel *cs = thisp->deps[c];
  float sum = 0.f;
  if (thisp->chs_data[c]) return thisp->chs_data[c][i];
  if (!thisp->deps[c]) return 0.f;

  while (cs->ch) {
    if (cs->sp)
      sum += _downmix_channel_data(thisp, cs->ch, i) * (*cs->sp);
    else
      sum += _downmix_channel_data(thisp, cs->ch, i) * cs->s;
    ++cs;
  }
  return sum;
}

DMRenderer *DMRenderer_open(IAChannelLayoutType in, IAChannelLayoutType out) {
  DMRenderer *thisp = 0;
  if (in == out || !_valid_channel_layout(in) || !_valid_channel_layout(out))
    return 0;

  ia_logd("%s downmix to %s", ia_channel_layout_name(in),
          ia_channel_layout_name(out));

  if (!_valid_downmix(in, out)) return 0;

  thisp = IAMF_MALLOCZ(DMRenderer, 1);
  if (!thisp) return 0;

  thisp->chs_icount = ia_channel_layout_get_channels(in, thisp->chs_in,
                                                     IA_CH_LAYOUT_MAX_CHANNELS);
  thisp->chs_ocount = ia_channel_layout_get_channels(out, thisp->chs_out,
                                                     IA_CH_LAYOUT_MAX_CHANNELS);

  thisp->mode = -1;
  thisp->w_idx = -1;

  thisp->deps[IA_CH_MONO] = chmono;
  thisp->deps[IA_CH_L2] = chl2;
  thisp->deps[IA_CH_R2] = chr2;
  thisp->deps[IA_CH_L3] = chl3;
  thisp->deps[IA_CH_R3] = chr3;
  thisp->deps[IA_CH_SL5] = chsl5;
  thisp->deps[IA_CH_SR5] = chsr5;
  thisp->deps[IA_CH_TL] = chtl;
  thisp->deps[IA_CH_TR] = chtr;
  thisp->deps[IA_CH_HL] = chhl;
  thisp->deps[IA_CH_HR] = chhr;

  thisp->deps[IA_CH_SR5][0].sp = thisp->deps[IA_CH_SL5][0].sp =
      &thisp->mix_factors.alpha;
  thisp->deps[IA_CH_SR5][1].sp = thisp->deps[IA_CH_SL5][1].sp =
      &thisp->mix_factors.beta;
  thisp->deps[IA_CH_L3][1].sp = thisp->deps[IA_CH_R3][1].sp =
      &thisp->mix_factors.delta;
  thisp->deps[IA_CH_HL][1].sp = thisp->deps[IA_CH_HR][1].sp =
      &thisp->mix_factors.gamma;

  for (int i = 0; i < thisp->chs_icount; ++i) thisp->deps[thisp->chs_in[i]] = 0;

  return thisp;
}

void DMRenderer_close(DMRenderer *thisp) { IAMF_FREE(thisp); }

int DMRenderer_set_mode_weight(DMRenderer *thisp, int mode, int w_idx) {
  if (!thisp || !iamf_valid_mix_mode(mode)) return IAMF_ERR_BAD_ARG;

  if (thisp->mode != mode) {
    ia_logd("dmixtypenum: %d -> %d", thisp->mode, mode);
    thisp->mode = mode;
    thisp->mix_factors = *iamf_get_mix_factors(mode);
    ia_logd("mode %d: a %f, b %f, g %f, d %f, w index offset %d", mode,
            thisp->mix_factors.alpha, thisp->mix_factors.beta,
            thisp->mix_factors.gamma, thisp->mix_factors.delta,
            thisp->mix_factors.w_idx_offset);
  }

  if (w_idx < MIN_W_INDEX || w_idx > MAX_W_INDEX) {
    int new_w_idx;

    calc_w(thisp->mix_factors.w_idx_offset, thisp->w_idx, &new_w_idx);
    ia_logd("weight state index : %d (%f) -> %d (%f)", thisp->w_idx,
            get_w(thisp->w_idx), new_w_idx, get_w(new_w_idx));
    thisp->w_idx = new_w_idx;
    if (thisp->deps[IA_CH_TL] && thisp->deps[IA_CH_TR])
      thisp->deps[IA_CH_TL][1].s = thisp->deps[IA_CH_TR][1].s =
          thisp->mix_factors.gamma * get_w(new_w_idx);
  } else {
    if (mode != thisp->mode) ia_logd("set default demixing mode %d", mode);
    if (thisp->w_idx != w_idx) {
      thisp->w_idx = w_idx;
      ia_logd("set default weight index %d, value %f", w_idx, get_w(w_idx));
      if (thisp->deps[IA_CH_TL] && thisp->deps[IA_CH_TR]) {
        thisp->deps[IA_CH_TL][1].s = thisp->deps[IA_CH_TR][1].s =
            thisp->mix_factors.gamma * get_w(w_idx);
      }
    }
  }

  return IAMF_OK;
}

int DMRenderer_downmix(DMRenderer *thisp, float *in, float *out, uint32_t s,
                       uint32_t duration, uint32_t size) {
  int off, e;
  if (!thisp || !in || !out || !size || s >= size) return IAMF_ERR_BAD_ARG;

  memset(thisp->chs_data, 0, IA_CH_COUNT * sizeof(float));
  for (int i = 0; i < thisp->chs_icount; ++i) {
    thisp->chs_data[thisp->chs_in[i]] = in + size * i;
  }

  e = s + duration;
  if (e > size) e = size;

  for (int i = 0; i < thisp->chs_ocount; ++i) {
    ia_logd("channel %s(%d) checking...", ia_channel_name(thisp->chs_out[i]),
            thisp->chs_out[i]);
    _downmix_dump(thisp, thisp->chs_out[i]);
    off = size * i;
    for (int j = s; j < e; ++j) {
      out[off + j] = _downmix_channel_data(thisp, thisp->chs_out[i], j);
    }
  }

  return IAMF_OK;
}
