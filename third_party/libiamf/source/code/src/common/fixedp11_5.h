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
 * @file fixedp11_5.h
 * @brief Convert to fixed functions
 * @version 0.1
 * @date Created 3/3/2023
 **/

#ifndef _FIXEDP11_5_H_
#define _FIXEDP11_5_H_
#include <math.h>
#include <stdint.h>

#define FIXED_POINT_FRACTIONAL_BITS 14

/// Fixed-point Format: 2.14 (16-bit)
typedef int16_t fixed16_t;
typedef int16_t q16_t;
typedef uint8_t qf_t;

/// Converts between double and q16_t
float q_to_float(q16_t q, int frac);

/// Converts between double and q8_t
float qf_to_float(qf_t q, int frac);

float db2lin(float db);

/// Mapping of wIdx(k) to w(k) for de-mixer
#define MIN_W_INDEX 0
#define MAX_W_INDEX 10
float calc_w(int w_idx_offset, int w_idx_prev, int *w_idx);
float get_w(int w_idx);
#endif
