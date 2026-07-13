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
