// Copyright 2020 The BoringSSL Authors
// Copyright 2014-2016 The OpenSSL Project Authors. All Rights Reserved.
// Copyright (c) 2014, Intel Corporation. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <openssl/base.h>

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/mem.h>

#include <assert.h>

#include <iterator>

#include "../../internal.h"
#include "../delocate.h"
#include "internal.h"

#include "p256_internal.h"

using namespace bssl;

static void fiat_p256_opp_conditional(fiat_p256_felem x, crypto_word_t c) {
  alignas(32) fiat_p256_felem n;
  fiat_p256_opp(n, x);
  for (size_t i = 0; i < P256_LIMBS; ++i) {
    x[i] = br_cmov(c, n[i], x[i]);
  }
}

// bit returns the `i`th bit in `in`.
static crypto_word_t bit(const uint8_t in[32], int i) {
  if (i < 0 || i >= 256) {
    return 0;
  }
  return (in[i >> 3] >> (i & 7)) & 1;
}

// point multiplication
static void fiat_p256_select_point_16(fiat_p256_felem dst[3],
                                      const fiat_p256_felem src[16][3],
                                      size_t i) {
  OPENSSL_memset(dst, 0, 3 * sizeof(fiat_p256_felem));
  OPENSSL_CLANG_PRAGMA("clang loop unroll_count(4)")
  for (size_t j = 0; j < 16; j++) {
    constant_time_conditional_memxor(dst, &src[j], 3 * sizeof(fiat_p256_felem),
                                     constant_time_eq_w(i, j));
  }
}

// Precompute multiples of `p`. p_pre_comp[i] is (i+1) * `p`.
static void p256_point_mul(fiat_p256_felem out[3], const fiat_p256_felem p[3],
                           const uint8_t s[32]) {
  alignas(32) fiat_p256_felem p_pre_comp[16][3];
  OPENSSL_memcpy(p_pre_comp[0], p, sizeof(p_pre_comp[0]));
  for (size_t j = 2; j <= 16; ++j) {
    if (j & 1) {
      p256_point_add_vartime_if_doubling((uintptr_t)p_pre_comp[j - 1],
                                         (uintptr_t)p_pre_comp[j - 2],
                                         (uintptr_t)p_pre_comp[0]);
    } else {
      p256_point_double((uintptr_t)p_pre_comp[j - 1],
                        (uintptr_t)p_pre_comp[(j - 1) / 2]);
    }
  }
  alignas(32) fiat_p256_felem ret[3];
  bool ret_is_zero = true;

  for (size_t i = 51; i < 52; i--) {
    if (!ret_is_zero) {
      for (size_t k = 4; k < 5; k--) {
        p256_point_double((uintptr_t)ret, (uintptr_t)ret);
      }
    }

    crypto_word_t bits = 0;
    OPENSSL_CLANG_PRAGMA("clang loop unroll(full)")
    for (size_t k = 5; k < 6; k--) {
      bits |= bit(s, i * 5 - 1 + k) << k;
    }
    crypto_word_t sign, digit;
    ec_GFp_nistp_recode_scalar_bits(&sign, &digit, bits);

    alignas(32) fiat_p256_felem t[3];
    fiat_p256_select_point_16(t, p_pre_comp, digit - 1);
    fiat_p256_opp_conditional(t[1], sign);

    if (!ret_is_zero) {
      p256_point_add_vartime_if_doubling((uintptr_t)ret, (uintptr_t)ret,
                                         (uintptr_t)t);
    } else {
      OPENSSL_memcpy(ret, t, sizeof(ret));
      ret_is_zero = false;
    }
  }
  // The first loop iteration will initialize `ret`.
  assert(!ret_is_zero);

  OPENSSL_memcpy(out, ret, sizeof(ret));
}

// point_mul_public
#include "./p256_table.h"

static void ec_GFp_nistp256_point_mul_public(const EC_GROUP *group,
                                             EC_JACOBIAN *r,
                                             const EC_SCALAR *gs,
                                             const EC_JACOBIAN *p,
                                             const EC_SCALAR *ps) {
  const uint8_t *g_scalar = (uint8_t *)gs->words;
  int8_t p_wNAF[257] = {0};

  alignas(32) fiat_p256_felem p_pre_comp[1 << (4 - 1)][3];
  OPENSSL_memcpy(p_pre_comp[0][2], &p->Z, 32);
  if (!p256_point_iszero((uintptr_t)p_pre_comp[0])) {
    ec_compute_wNAF(group, p_wNAF, ps, /*bits=*/256, /*w=*/4);
    // Precompute multiples of `p`. p_pre_comp[i] is (2*i+1) * `p`.
    OPENSSL_memcpy(p_pre_comp[0][0], &p->X, 32);
    OPENSSL_memcpy(p_pre_comp[0][1], &p->Y, 32);
    alignas(32) fiat_p256_felem p2[3];
    p256_point_double((uintptr_t)p2, (uintptr_t)p_pre_comp[0]);
    for (size_t i = 1; i < std::size(p_pre_comp); i++) {
      p256_point_add_nz_nz_neq((uintptr_t)p_pre_comp[i],
                               (uintptr_t)p_pre_comp[i - 1], (uintptr_t)p2);
    }
  }

  alignas(32) fiat_p256_felem ret[3] = {};
  bool ret_is_zero = true;  // Save some point operations, avoid 0+Q
  for (int i = 256; i >= 0; i--) {
    if (!ret_is_zero) {
      p256_point_double((uintptr_t)ret, (uintptr_t)ret);
    }

    if (i <= 31) {
      OPENSSL_CLANG_PRAGMA("clang loop unroll(full)")
      for (size_t j = 1; j < 2; j--) {
        crypto_word_t bits = 0;
        OPENSSL_CLANG_PRAGMA("clang loop unroll(full)")
        for (size_t k = 3; k < 4; k--) {
          bits |= bit(g_scalar, i + j * 32 + k * 64) << k;
        }
        if (bits != 0) {
          if (!ret_is_zero) {
            alignas(32) fiat_p256_felem t[3];
            fiat_p256_set_one(t[2]);
            OPENSSL_memcpy(t, fiat_p256_g_pre_comp[j][bits - 1], 64);
            p256_point_add_affinenz_conditional_vartime_if_doubling(
                (uintptr_t)ret, (uintptr_t)ret, (uintptr_t)t, 1);
            ret_is_zero = p256_point_iszero((uintptr_t)ret);
          } else {
            OPENSSL_memcpy(ret, fiat_p256_g_pre_comp[j][bits - 1],
                           sizeof(fiat_p256_g_pre_comp[j][bits - 1]));
            fiat_p256_set_one(ret[2]);
            ret_is_zero = false;
          }
        }
      }
    }

    int digit = p_wNAF[i];
    if (digit != 0) {
      assert(digit & 1);
      size_t idx = (size_t)(digit < 0 ? (-digit) >> 1 : digit >> 1);
      fiat_p256_felem t[3];
      OPENSSL_memcpy(t, p_pre_comp[idx], sizeof(t));
      if (digit < 0) {
        fiat_p256_opp(t[1], t[1]);
      }
      if (!ret_is_zero) {
        p256_point_add_vartime_if_doubling((uintptr_t)ret, (uintptr_t)ret,
                                           (uintptr_t)t);
        ret_is_zero = p256_point_iszero((uintptr_t)ret);
      } else {
        OPENSSL_memcpy(ret, t, sizeof(ret));
        ret_is_zero = false;
      }
    }
  }

  OPENSSL_memcpy(&r->X, ret[0], 32);
  OPENSSL_memcpy(&r->Y, ret[1], 32);
  OPENSSL_memcpy(&r->Z, ret[2], 32);
}

// p256_point_mul_base (small and full)

// fiat_p256_select_point_affine selects the `i`th point from a precomputation
// table and copies it to `dst`. If `n<=i`, the output is (0, 0).
static void fiat_p256_select_point_affine(fiat_p256_felem dst[2],
                                          const fiat_p256_felem src[/*n*/][2],
                                          size_t i, size_t n) {
  OPENSSL_memset(dst, 0, 2 * sizeof(fiat_p256_felem));
  OPENSSL_CLANG_PRAGMA("clang loop unroll_count(8)")
  for (size_t j = 0; j < n; j++) {
    constant_time_conditional_memxor(dst, &src[j], 2 * sizeof(fiat_p256_felem),
                                     constant_time_eq_w(i, j));
  }
}

static void fiat_p256_conditional_zero_or_one(fiat_p256_felem out,
                                              crypto_word_t c) {
  fiat_p256_set_one(out);
  crypto_word_t mask = value_barrier_w(~constant_time_is_zero_w(c));
  for (size_t i = 0; i < P256_LIMBS; ++i) {
    out[i] &= mask;
  }
}

#if defined(OPENSSL_SMALL)

#ifdef __clang__
__attribute__((noinline)) // Do not inline into unrolled loop below.
#endif
static void fiat_p256_select_point_affine_15(
    fiat_p256_felem dst[2], const fiat_p256_felem src[/*n*/][2], size_t i) {
  fiat_p256_select_point_affine(dst, src, i, 15); // Intended to be inlined.
}

static void p256_point_mul_base(fiat_p256_felem ret[3], const uint8_t s[32]) {
  bool ret_is_zero = true;  // Save two point operations in the first round.
  alignas(32) fiat_p256_felem t[3];
  fiat_p256_set_one(t[2]);
  for (size_t i = 31; i < 32; i--) {
    if (!ret_is_zero) {
      p256_point_double((uintptr_t)ret, (uintptr_t)ret);
    }
    OPENSSL_CLANG_PRAGMA("clang loop unroll(full)")
    for (size_t j = 1; j < 2; j--) {
      crypto_word_t bits = 0;
      OPENSSL_CLANG_PRAGMA("clang loop unroll(full)")
      for (size_t k = 3; k < 4; k--) {
        bits |= bit(s, i + 32 * j + 64 * k) << k;
      }
      fiat_p256_select_point_affine_15(t, fiat_p256_g_pre_comp[j], bits - 1);

      if (!ret_is_zero) {
        p256_point_add_affinenz_conditional_vartime_if_doubling(
            (uintptr_t)ret, (uintptr_t)ret, (uintptr_t)t, (uintptr_t)bits);
      } else {
        OPENSSL_memcpy(ret, t, sizeof(t));
        fiat_p256_conditional_zero_or_one(ret[2], bits);
        ret_is_zero = false;
      }
    }
  }

  // The first loop iteration will initialize `ret`.
  assert(!ret_is_zero);
}
#else  // defined(OPENSSL_SMALL)

/*
 * Originally written by Shay Gueron (1, 2), and Vlad Krasnov (1)
 * (1) Intel Corporation, Israel Development Center, Haifa, Israel
 * (2) University of Haifa, Israel
 *
 * Reference:
 * S.Gueron and V.Krasnov, "Fast Prime Field Elliptic Curve Cryptography with
 *                          256 Bit Primes"
 */

// Precomputed tables for the default generator
typedef fiat_p256_felem PRECOMP256_ROW[64][2];
#include "p256-nistz-table.h"

#ifdef __clang__
__attribute__((noinline)) // Do not inline into unrolled loop below.
#endif
static void fiat_p256_select_point_affine_16(fiat_p256_felem dst[2],
                                             const fiat_p256_felem src[16][2],
                                             size_t i) {
  fiat_p256_select_point_affine(dst, src, i, 16);
}

#ifdef __clang__
__attribute__((noinline)) // Do not inline into unrolled loop below.
#endif
static void fiat_p256_select_point_affine_64(fiat_p256_felem dst[2],
                                             const fiat_p256_felem src[64][2],
                                             size_t i) {
  fiat_p256_select_point_affine(dst, src, i, 64);
}

// See `ec_GFp_nistp_recode_scalar_bits` in util.c for details
static crypto_word_t booth_recode_w7(crypto_word_t in) {
  crypto_word_t s, d;
  s = ~((in >> 7) - 1);
  d = (1 << 8) - in - 1;
  d = (d & s) | (in & ~s);
  d = (d >> 1) + (d & 1);
  return (d << 1) + (s & 1);
}

static void p256_point_mul_base(fiat_p256_felem ret[3], const uint8_t s[32]) {
  bool ret_is_zero = true;
  alignas(32) fiat_p256_felem t[3];
  fiat_p256_set_one(t[2]);
  for (size_t i = 36; i < 37; i--) {
    // Load 7-bit windows, plus one bit below the window for `booth_recode_w7`,
    // i.e. 8 bits from 7 * i - 1 to 7 * (i + 1).
    constexpr size_t kMask = (1 << (7 + 1)) - 1;
    crypto_word_t wvalue;
    if (i == 0) {
      wvalue = (s[0] << 1) & kMask;
    } else {
      size_t first_bit = 7 * i - 1;
      size_t idx = first_bit / 8;
      // The window may span two bytes.
      wvalue =
          s[idx] | (static_cast<crypto_word_t>(idx < 31 ? s[idx + 1] : 0) << 8);
      wvalue = (wvalue >> (first_bit % 8)) & kMask;
    }
    wvalue = booth_recode_w7(wvalue);
    if (i == 36) {
      // The last window has only 4 bits instead of the full 7.
      declassify_assert((wvalue >> 1) <= 16);
      fiat_p256_select_point_affine_16(t, ecp_nistz256_precomputed[i],
                                       (wvalue >> 1) - 1);
    } else {
      fiat_p256_select_point_affine_64(t, ecp_nistz256_precomputed[i],
                                       (wvalue >> 1) - 1);
    }
    fiat_p256_opp_conditional(t[1], wvalue & 1);

    if (!ret_is_zero) {
      p256_point_add_affinenz_conditional_vartime_if_doubling(
          (uintptr_t)ret, (uintptr_t)ret, (uintptr_t)t, wvalue >> 1);
    } else {
      OPENSSL_memcpy(ret, t, sizeof(t));
      fiat_p256_conditional_zero_or_one(ret[2], wvalue >> 1);
      ret_is_zero = false;
    }
  }

  // The first loop iteration will initialize `ret`.
  assert(!ret_is_zero);
}
#endif  // !OPENSSL_SMALL


// FIELD-ELEMENT INVERSION


// fiat_p256_inv_square calculates `out` = `in`^{-2}
//
// Based on Fermat's Little Theorem:
//   a^p = a (mod p)
//   a^{p-1} = 1 (mod p)
//   a^{p-3} = a^{-2} (mod p)
static void fiat_p256_inv_square(fiat_p256_felem out,
                                 const fiat_p256_felem in) {
  // This implements the addition chain described in
  // https://briansmith.org/ecc-inversion-addition-chains-01#p256_field_inversion
  fiat_p256_felem x2, x3, x6, x12, x15, x30, x32;
  p256_coord_sqr(x2, in);      // 2^2 - 2^1
  p256_coord_mul(x2, x2, in);  // 2^2 - 2^0

  p256_coord_sqr(x3, x2);      // 2^3 - 2^1
  p256_coord_mul(x3, x3, in);  // 2^3 - 2^0

  p256_coord_sqr(x6, x3);
  for (int i = 1; i < 3; i++) {
    p256_coord_sqr(x6, x6);  // 2^6 - 2^3
  }
  p256_coord_mul(x6, x6, x3);  // 2^6 - 2^0

  p256_coord_sqr(x12, x6);
  for (int i = 1; i < 6; i++) {
    p256_coord_sqr(x12, x12);  // 2^12 - 2^6
  }
  p256_coord_mul(x12, x12, x6);  // 2^12 - 2^0

  p256_coord_sqr(x15, x12);
  for (int i = 1; i < 3; i++) {
    p256_coord_sqr(x15, x15);  // 2^15 - 2^3
  }
  p256_coord_mul(x15, x15, x3);  // 2^15 - 2^0

  p256_coord_sqr(x30, x15);
  for (int i = 1; i < 15; i++) {
    p256_coord_sqr(x30, x30);  // 2^30 - 2^15
  }
  p256_coord_mul(x30, x30, x15);  // 2^30 - 2^0

  p256_coord_sqr(x32, x30);
  p256_coord_sqr(x32, x32);      // 2^32 - 2^2
  p256_coord_mul(x32, x32, x2);  // 2^32 - 2^0

  fiat_p256_felem ret;
  p256_coord_sqr(ret, x32);
  for (int i = 1; i < 31 + 1; i++) {
    p256_coord_sqr(ret, ret);  // 2^64 - 2^32
  }
  p256_coord_mul(ret, ret, in);  // 2^64 - 2^32 + 2^0

  for (int i = 0; i < 96 + 32; i++) {
    p256_coord_sqr(ret, ret);  // 2^192 - 2^160 + 2^128
  }
  p256_coord_mul(ret, ret, x32);  // 2^192 - 2^160 + 2^128 + 2^32 - 2^0

  for (int i = 0; i < 32; i++) {
    p256_coord_sqr(ret, ret);  // 2^224 - 2^192 + 2^160 + 2^64 - 2^32
  }
  p256_coord_mul(ret, ret, x32);  // 2^224 - 2^192 + 2^160 + 2^64 - 2^0

  for (int i = 0; i < 30; i++) {
    p256_coord_sqr(ret, ret);  // 2^254 - 2^222 + 2^190 + 2^94 - 2^30
  }
  p256_coord_mul(ret, ret, x30);  // 2^254 - 2^222 + 2^190 + 2^94 - 2^0

  p256_coord_sqr(ret, ret);
  p256_coord_sqr(out, ret);  // 2^256 - 2^224 + 2^192 + 2^96 - 2^2
}


// Arithmetic modulo curve order


static void p256_order_inv0(const EC_GROUP *group, EC_SCALAR *out,
                            const EC_SCALAR *in) {
  // table[i] stores a power of `in` corresponding to the matching enum value.
  enum {
    // The following indices specify the power in binary.
    i_1 = 0,
    i_10,
    i_11,
    i_101,
    i_111,
    i_1010,
    i_1111,
    i_10101,
    i_101010,
    i_101111,
    // The following indices specify 2^N-1, or N ones in a row.
    i_x6,
    i_x8,
    i_x16,
    i_x32
  };
  BN_ULONG table[15][P256_LIMBS];

  // https://briansmith.org/ecc-inversion-addition-chains-01#p256_scalar_inversion
  // This code uses specialized field arithmetic and saves 12 sqr and 13 mul.

  // Pre-calculate powers.
  OPENSSL_memcpy(table[i_1], in->words, P256_LIMBS * sizeof(BN_ULONG));

  p256_order_sqr(group, table[i_10], table[i_1], 1);

  p256_order_mul(group, table[i_11], table[i_1], table[i_10]);

  p256_order_mul(group, table[i_101], table[i_11], table[i_10]);

  p256_order_mul(group, table[i_111], table[i_101], table[i_10]);

  p256_order_sqr(group, table[i_1010], table[i_101], 1);

  p256_order_mul(group, table[i_1111], table[i_1010], table[i_101]);

  p256_order_sqr(group, table[i_10101], table[i_1010], 1);
  p256_order_mul(group, table[i_10101], table[i_10101], table[i_1]);

  p256_order_sqr(group, table[i_101010], table[i_10101], 1);

  p256_order_mul(group, table[i_101111], table[i_101010], table[i_101]);

  p256_order_mul(group, table[i_x6], table[i_101010], table[i_10101]);

  p256_order_sqr(group, table[i_x8], table[i_x6], 2);
  p256_order_mul(group, table[i_x8], table[i_x8], table[i_11]);

  p256_order_sqr(group, table[i_x16], table[i_x8], 8);
  p256_order_mul(group, table[i_x16], table[i_x16], table[i_x8]);

  p256_order_sqr(group, table[i_x32], table[i_x16], 16);
  p256_order_mul(group, table[i_x32], table[i_x32], table[i_x16]);

  // Compute `in` raised to the order-2.
  p256_order_sqr(group, out->words, table[i_x32], 64);
  p256_order_mul(group, out->words, out->words, table[i_x32]);
  static const struct {
    uint8_t p, i;
  } kChain[27] = {{32, i_x32},    {6, i_101111}, {5, i_111},    {4, i_11},
                  {5, i_1111},    {5, i_10101},  {4, i_101},    {3, i_101},
                  {3, i_101},     {5, i_111},    {9, i_101111}, {6, i_1111},
                  {2, i_1},       {5, i_1},      {6, i_1111},   {5, i_111},
                  {4, i_111},     {5, i_111},    {5, i_101},    {3, i_11},
                  {10, i_101111}, {2, i_11},     {5, i_11},     {5, i_11},
                  {3, i_1},       {7, i_10101},  {6, i_1111}};
  for (const auto &step : kChain) {
    p256_order_sqr(group, out->words, out->words, step.p);
    p256_order_mul(group, out->words, out->words, table[step.i]);
  }
}

static int p256_order_mont_inv_vartime(const EC_GROUP *group, EC_SCALAR *out,
                                       const EC_SCALAR *in) {
#if !defined(OPENSSL_NO_ASM) && \
    (defined(OPENSSL_X86_64) || defined(OPENSSL_AARCH64))
#if defined(OPENSSL_X86_64)
  if (!CRYPTO_is_AVX_capable()) {
    // No AVX support; fallback to generic code.
    return ec_simple_scalar_to_montgomery_inv_vartime(group, out, in);
  }
#endif

  assert(group->order.N.width == P256_LIMBS);
  if (!beeu_mod_inverse_vartime(out->words, in->words, group->order.N.d)) {
    return 0;
  }

  // The result should be returned in the Montgomery domain.
  ec_scalar_to_montgomery(group, out, out);
  return 1;
#else
  return ec_simple_scalar_to_montgomery_inv_vartime(group, out, in);
#endif
}


// OPENSSL EC_METHOD WRAPPERS


static void fiat_p256_from_generic(fiat_p256_felem out, const EC_FELEM *in) {
  OPENSSL_memcpy(out, in->words, 32);
}

static void fiat_p256_to_generic(EC_FELEM *out, const fiat_p256_felem in) {
  OPENSSL_memcpy(out->words, in, 32);
}

// Takes the Jacobian coordinates (X, Y, Z) of a point and returns (X', Y') =
// (X/Z^2, Y/Z^3).
static int ec_GFp_nistp256_point_get_affine_coordinates(
    const EC_GROUP *group, const EC_JACOBIAN *point, EC_FELEM *x_out,
    EC_FELEM *y_out) {
  if (constant_time_declassify_int(
          ec_GFp_simple_is_at_infinity(group, point))) {
    OPENSSL_PUT_ERROR(EC, EC_R_POINT_AT_INFINITY);
    return 0;
  }

  fiat_p256_felem z1, z2;
  fiat_p256_from_generic(z1, &point->Z);
  fiat_p256_inv_square(z2, z1);

  if (x_out != nullptr) {
    fiat_p256_felem x;
    fiat_p256_from_generic(x, &point->X);
    p256_coord_mul(x, x, z2);
    fiat_p256_to_generic(x_out, x);
  }

  if (y_out != nullptr) {
    fiat_p256_felem y;
    fiat_p256_from_generic(y, &point->Y);
    p256_coord_sqr(z2, z2);    // z^-4
    p256_coord_mul(y, y, z1);  // y * z
    p256_coord_mul(y, y, z2);  // y * z^-3
    fiat_p256_to_generic(y_out, y);
  }

  return 1;
}

static int ec_GFp_nistp256_cmp_x_coordinate(const EC_GROUP *group,
                                            const EC_JACOBIAN *p,
                                            const EC_SCALAR *r) {
  if (ec_GFp_simple_is_at_infinity(group, p)) {
    return 0;
  }

  // We wish to compare X/Z^2 with r. This is equivalent to comparing X with
  // r*Z^2. Note that X and Z are represented in Montgomery form, while r is
  // not.
  fiat_p256_felem Z2_mont;
  fiat_p256_from_generic(Z2_mont, &p->Z);
  p256_coord_mul(Z2_mont, Z2_mont, Z2_mont);

  fiat_p256_felem r_Z2;
  OPENSSL_memcpy(r_Z2, r->words, 32);  // r < order < p, so this is valid.
  p256_coord_mul(r_Z2, r_Z2, Z2_mont);

  fiat_p256_felem X;
  fiat_p256_from_generic(X, &p->X);
  fiat_p256_from_montgomery(X, X);

  if (OPENSSL_memcmp(&r_Z2, &X, sizeof(r_Z2)) == 0) {
    return 1;
  }

  // During signing the x coefficient is reduced modulo the group order.
  // Therefore there is a small possibility, less than 1/2^128, that group_order
  // < p.x < P. in that case we need not only to compare against `r` but also to
  // compare against r+group_order.
  assert(group->field.N.width == group->order.N.width);
  EC_FELEM tmp;
  BN_ULONG carry =
      bn_add_words(tmp.words, r->words, group->order.N.d, group->field.N.width);
  if (carry == 0 &&
      bn_less_than_words(tmp.words, group->field.N.d, group->field.N.width)) {
    fiat_p256_from_generic(r_Z2, &tmp);
    p256_coord_mul(r_Z2, r_Z2, Z2_mont);
    if (OPENSSL_memcmp(&r_Z2, &X, sizeof(r_Z2)) == 0) {
      return 1;
    }
  }

  return 0;
}

static void ec_GFp_nistp256_point_mul(const EC_GROUP *group, EC_JACOBIAN *r,
                                      const EC_JACOBIAN *p,
                                      const EC_SCALAR *scalar) {
  fiat_p256_felem t[3];
  fiat_p256_from_generic(t[0], &p->X);
  fiat_p256_from_generic(t[1], &p->Y);
  fiat_p256_from_generic(t[2], &p->Z);
  p256_point_mul(t, t, (uint8_t *)scalar->words);

  fiat_p256_to_generic(&r->X, t[0]);
  fiat_p256_to_generic(&r->Y, t[1]);
  fiat_p256_to_generic(&r->Z, t[2]);
}

static void ec_GFp_nistp256_point_mul_base(const EC_GROUP *group,
                                           EC_JACOBIAN *r, const EC_SCALAR *s) {
  alignas(32) fiat_p256_felem ret[3];
  p256_point_mul_base(ret, (uint8_t *)s->words);
  fiat_p256_to_generic(&r->X, ret[0]);
  fiat_p256_to_generic(&r->Y, ret[1]);
  fiat_p256_to_generic(&r->Z, ret[2]);
}

static void ec_GFp_nistp256_add(const EC_GROUP *group, EC_JACOBIAN *r,
                                const EC_JACOBIAN *a, const EC_JACOBIAN *b) {
  fiat_p256_felem p[3], q[3];
  fiat_p256_from_generic(p[0], &a->X);
  fiat_p256_from_generic(p[1], &a->Y);
  fiat_p256_from_generic(p[2], &a->Z);
  fiat_p256_from_generic(q[0], &b->X);
  fiat_p256_from_generic(q[1], &b->Y);
  fiat_p256_from_generic(q[2], &b->Z);
  p256_point_add_vartime_if_doubling((uintptr_t)p, (uintptr_t)p, (uintptr_t)q);
  fiat_p256_to_generic(&r->X, p[0]);
  fiat_p256_to_generic(&r->Y, p[1]);
  fiat_p256_to_generic(&r->Z, p[2]);
}

static void ec_GFp_nistp256_dbl(const EC_GROUP *group, EC_JACOBIAN *r,
                                const EC_JACOBIAN *a) {
  fiat_p256_felem p[3];
  fiat_p256_from_generic(p[0], &a->X);
  fiat_p256_from_generic(p[1], &a->Y);
  fiat_p256_from_generic(p[2], &a->Z);
  p256_point_double((uintptr_t)p, (uintptr_t)p);
  fiat_p256_to_generic(&r->X, p[0]);
  fiat_p256_to_generic(&r->Y, p[1]);
  fiat_p256_to_generic(&r->Z, p[2]);
}

BSSL_NAMESPACE_BEGIN

DEFINE_METHOD_FUNCTION(EC_METHOD, EC_GFp_nistp256_method) {
  out->point_get_affine_coordinates =
      ec_GFp_nistp256_point_get_affine_coordinates;
  out->add = ec_GFp_nistp256_add;
  out->dbl = ec_GFp_nistp256_dbl;
  out->mul = ec_GFp_nistp256_point_mul;
  out->mul_base = ec_GFp_nistp256_point_mul_base;
  out->mul_public = ec_GFp_nistp256_point_mul_public;
  out->scalar_inv0_montgomery = p256_order_inv0;
  out->scalar_to_montgomery_inv_vartime = p256_order_mont_inv_vartime;
  out->cmp_x_coordinate = ec_GFp_nistp256_cmp_x_coordinate;
}

BSSL_NAMESPACE_END
