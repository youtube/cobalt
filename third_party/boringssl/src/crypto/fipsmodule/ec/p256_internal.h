// Copyright 2026 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_EC_P256_INTERNAL_H
#define OPENSSL_HEADER_EC_P256_INTERNAL_H

#include <openssl/base.h>
#include <openssl/bn.h>

#include "../../internal.h"
#include "../../../third_party/fiat/bedrock_unverified_platform.c.inc"

#define P256_LIMBS (32 / sizeof(bssl::crypto_word_t))
typedef bssl::crypto_word_t fiat_p256_felem[P256_LIMBS];


// Choose implementation of arithmetic in the coordinate field.

#if defined(BORINGSSL_HAS_UINT128)
#include "../../../third_party/fiat/p256_field_64.br.c.inc"

#include "../../../third_party/fiat/p256_64.h"
#elif defined(OPENSSL_64_BIT)
#include "../../../third_party/fiat/p256_field_64.br.c.inc"

#include "../../../third_party/fiat/p256_64_msvc.h"
#else
#include "../../../third_party/fiat/p256_field_32.br.c.inc"

#include "../../../third_party/fiat/p256_32.h"
// Add Bedrock versions of p256_32.h functions for p256_point.br.c.inc to call.
static inline void p256_coord_add(br_word_t out, br_word_t x, br_word_t y) {
  fiat_p256_add((uint32_t *)out, (const uint32_t *)x, (const uint32_t *)y);
}
static inline void p256_coord_sub(br_word_t out, br_word_t x, br_word_t y) {
  fiat_p256_sub((uint32_t *)out, (const uint32_t *)x, (const uint32_t *)y);
}
#endif

extern "C" {
#if !defined(OPENSSL_NO_ASM) && (defined(__ELF__) || defined(__APPLE__)) && \
    defined(OPENSSL_X86_64) && !defined(OPENSSL_NANOLIBC)
// These functions are only available with gas and SysV ABI, used by Apple and
// ELF-based platforms. Unlike most of our SysV assembly, they currently rely on
// the SysV redzone. This trips one target which looks like it targets SysV but
// has no redzone. This happens to define `OPENSSL_NANOLIBC`, so gate on that.
//
// TODO(crbug.com/522255483): Come up with a clearer story for the redzone
// situation.
#define FIAT_P256_ADX_ASM
void fiat_p256_adx_mul(uint64_t x0[4], const uint64_t x1[4],
                       const uint64_t x2[4]);
void fiat_p256_adx_sqr(uint64_t x0[4], const uint64_t x1[4]);
#elif !defined(OPENSSL_NO_ASM) && defined(OPENSSL_AARCH64)
void ecp_nistz256_mul_mont(uint64_t pr[4], const uint64_t py[4], uint64_t y0,
                           uint64_t x0, uint64_t x1, uint64_t x2, uint64_t x3);
void ecp_nistz256_sqr_mont(uint64_t pr[4], const uint64_t py[4], uint64_t y0,
                           uint64_t x0, uint64_t x1, uint64_t x2, uint64_t x3);
#endif
}

static inline void p256_coord_mul(fiat_p256_felem out, const fiat_p256_felem x,
                                  const fiat_p256_felem y) {
#if defined(FIAT_P256_ADX_ASM)
  if (bssl::CRYPTO_is_BMI1_capable() && bssl::CRYPTO_is_BMI2_capable() &&
      bssl::CRYPTO_is_ADX_capable()) {
    return fiat_p256_adx_mul(out, x, y);
  }
  fiat_p256_mul(out, x, y);
#elif !defined(OPENSSL_NO_ASM) && defined(OPENSSL_AARCH64)
  ecp_nistz256_mul_mont(out, y, y[0], x[0], x[1], x[2], x[3]);
#else
  fiat_p256_mul(out, x, y);
#endif
}

static inline void p256_coord_sqr(fiat_p256_felem out,
                                  const fiat_p256_felem x) {
#if defined(FIAT_P256_ADX_ASM)
  if (bssl::CRYPTO_is_BMI1_capable() && bssl::CRYPTO_is_BMI2_capable() &&
      bssl::CRYPTO_is_ADX_capable()) {
    return fiat_p256_adx_sqr(out, x);
  }
  fiat_p256_square(out, x);
#elif !defined(OPENSSL_NO_ASM) && defined(OPENSSL_AARCH64)
  ecp_nistz256_sqr_mont(out, x, x[0], x[0], x[1], x[2], x[3]);
#else
  fiat_p256_square(out, x);
#endif
}

// Add Bedrock versions of these functions for p256_point.br.c.inc to call.
static inline void p256_coord_mul(br_word_t out, br_word_t x, br_word_t y) {
  p256_coord_mul((br_word_t *)out, (const br_word_t *)x, (const br_word_t *)y);
}

static inline void p256_coord_sqr(br_word_t out, br_word_t x) {
  p256_coord_sqr((br_word_t *)out, (const br_word_t *)x);
}


// Choose implementation of arithmetic in the field modulo curve order.

extern "C" {

#if !defined(OPENSSL_NO_ASM) && \
    (defined(OPENSSL_X86_64) || defined(OPENSSL_AARCH64))
// beeu_mod_inverse_vartime sets out = a^-1 mod p using a Euclidean algorithm.
// Assumption: 0 < a < p < 2^(256) and p is odd.
int beeu_mod_inverse_vartime(BN_ULONG out[4], const BN_ULONG a[4],
                             const BN_ULONG p[4]);
#endif

// P-256 scalar operations.
//
// The following functions compute modulo N, where N is the order of P-256. They
// take fully-reduced inputs and give fully-reduced outputs.

#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_AARCH64)
// ecp_nistz256_ord_mul_mont sets `res` to `a` * `b` where inputs and outputs
// are in Montgomery form. That is, `res` is `a` * `b` * 2^-256 mod N.
void ecp_nistz256_ord_mul_mont(BN_ULONG res[4], const BN_ULONG a[4],
                               const BN_ULONG b[4]);

// ecp_nistz256_ord_sqr_mont sets `res` to `a`^(2*`rep`) where inputs and
// outputs are in Montgomery form. That is, `res` is
// (`a` * 2^-256)^(2*`rep`) * 2^256 mod N.
void ecp_nistz256_ord_sqr_mont(BN_ULONG res[4], const BN_ULONG a[4],
                               BN_ULONG rep);

#elif !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86_64)
void ecp_nistz256_ord_mul_mont_nohw(BN_ULONG res[4], const BN_ULONG a[4],
                                    const BN_ULONG b[4]);
void ecp_nistz256_ord_mul_mont_adx(BN_ULONG res[4], const BN_ULONG a[4],
                                   const BN_ULONG b[4]);

void ecp_nistz256_ord_sqr_mont_nohw(BN_ULONG res[4], const BN_ULONG a[4],
                                    BN_ULONG rep);
void ecp_nistz256_ord_sqr_mont_adx(BN_ULONG res[4], const BN_ULONG a[4],
                                   BN_ULONG rep);
#endif

}  // extern C

static inline void p256_order_mul(const EC_GROUP *group,
                                  BN_ULONG res[P256_LIMBS],
                                  const BN_ULONG a[P256_LIMBS],
                                  const BN_ULONG b[P256_LIMBS]) {
#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_AARCH64)
  return ecp_nistz256_ord_mul_mont(res, a, b);
#elif !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86_64)
  if (bssl::CRYPTO_is_BMI2_capable() && bssl::CRYPTO_is_ADX_capable()) {
    return ecp_nistz256_ord_mul_mont_adx(res, a, b);
  }
  return ecp_nistz256_ord_mul_mont_nohw(res, a, b);
#else
  const BIGNUM *order = &group->order.N;
  bssl::bn_mod_mul_montgomery_small(res, a, b, order->width, &group->order);
#endif
}

static inline void p256_order_sqr(const EC_GROUP *group,
                                  BN_ULONG res[P256_LIMBS],
                                  const BN_ULONG a[P256_LIMBS], BN_ULONG rep) {
#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_AARCH64)
  return ecp_nistz256_ord_sqr_mont(res, a, rep);
#elif !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86_64)
  if (bssl::CRYPTO_is_BMI2_capable() && bssl::CRYPTO_is_ADX_capable()) {
    return ecp_nistz256_ord_sqr_mont_adx(res, a, rep);
  }
  return ecp_nistz256_ord_sqr_mont_nohw(res, a, rep);
#else
  bssl::OPENSSL_memmove(res, a, sizeof(BN_ULONG) * P256_LIMBS);
  for (BN_ULONG i = 0; i < rep; i++) {
    p256_order_mul(group, res, res, res);
  }
#endif
}

#include "../../../third_party/fiat/p256_point.br.c.inc"

#endif  // OPENSSL_HEADER_EC_P256_INTERNAL_H
