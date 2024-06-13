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
