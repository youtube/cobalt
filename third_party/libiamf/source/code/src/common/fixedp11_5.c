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
 * @file fixedp11_5.c
 * @brief Convert to fixed functions
 * @version 0.1
 * @date Created 3/3/2023
**/

#include "fixedp11_5.h"

float q_to_float(q16_t q, int frac) {
  return ((float)q) * powf(2.0f, (float)-frac);
}

float qf_to_float(qf_t qf, int frac) {
  return ((float)qf / (pow(2.0f, (float)frac) - 1.0));  // f = q / 255
}

// dB to linear
float db2lin(float db) { return powf(10.0f, 0.05f * db); }

// Mapping of wIdx(k) to w(k)
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
static float widx2w_table[11] = {0.0,    0.0179, 0.0391, 0.0658, 0.1038, 0.25,
                                 0.3962, 0.4342, 0.4609, 0.4821, 0.5};
float calc_w(int w_idx_offset, int w_idx_prev, int *w_idx) {
  if (w_idx_offset > 0)
    *w_idx = MIN(w_idx_prev + 1, 10);
  else
    *w_idx = MAX(w_idx_prev - 1, 0);

  return widx2w_table[*w_idx];
}

float get_w(int w_idx) {
  if (w_idx < 0)
    return widx2w_table[0];
  else if (w_idx > 10)
    return widx2w_table[10];

  return widx2w_table[w_idx];
}
