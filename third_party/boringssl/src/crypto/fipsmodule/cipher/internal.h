// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_CRYPTO_FIPSMODULE_CIPHER_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_FIPSMODULE_CIPHER_INTERNAL_H

#include <openssl/base.h>

#include <openssl/aead.h>
#include <openssl/aes.h>
#include <openssl/span.h>

#include "../../internal.h"
#include "../aes/internal.h"

#include <algorithm>
#include <functional>
#include <optional>

extern "C" {


// EVP_CIPH_MODE_MASK contains the bits of |flags| that represent the mode.
#define EVP_CIPH_MODE_MASK 0x3f

// EVP_AEAD represents a specific AEAD algorithm.
struct evp_aead_st {
  uint8_t key_len;
  uint8_t nonce_len;
  uint8_t overhead;
  uint8_t max_tag_len;
  int seal_scatter_supports_extra_in;

  // init initialises an |EVP_AEAD_CTX|. If this call returns zero then
  // |cleanup| will not be called for that context.
  int (*init)(EVP_AEAD_CTX *, const uint8_t *key, size_t key_len,
              size_t tag_len);
  int (*init_with_direction)(EVP_AEAD_CTX *, const uint8_t *key, size_t key_len,
                             size_t tag_len, enum evp_aead_direction_t dir);
  void (*cleanup)(EVP_AEAD_CTX *);

  // AEADs need to provide one of the following sets of methods:
  //
  // - openv + sealv: variable tag lenght AEAD.
  // - openv_detached + sealv: fixed tag length AEAD.
  // - open + seal_scatter: legacy variable tag length AEAD.
  // - open_gather + seal_scatter: legacy fixed tag length AEAD.

  int (*open)(const EVP_AEAD_CTX *ctx, uint8_t *out, size_t *out_len,
              size_t max_out_len, const uint8_t *nonce, size_t nonce_len,
              const uint8_t *in, size_t in_len, const uint8_t *ad,
              size_t ad_len);

  int (*seal_scatter)(const EVP_AEAD_CTX *ctx, uint8_t *out, uint8_t *out_tag,
                      size_t *out_tag_len, size_t max_out_tag_len,
                      const uint8_t *nonce, size_t nonce_len, const uint8_t *in,
                      size_t in_len, const uint8_t *extra_in,
                      size_t extra_in_len, const uint8_t *ad, size_t ad_len);

  int (*open_gather)(const EVP_AEAD_CTX *ctx, uint8_t *out,
                     const uint8_t *nonce, size_t nonce_len, const uint8_t *in,
                     size_t in_len, const uint8_t *in_tag, size_t in_tag_len,
                     const uint8_t *ad, size_t ad_len);

  int (*openv)(const EVP_AEAD_CTX *ctx, bssl::Span<const CRYPTO_IOVEC> iovecs,
               size_t *out_total_bytes, const uint8_t *nonce, size_t nonce_len,
               bssl::Span<const CRYPTO_IVEC> aadvecs);

  int (*sealv)(const EVP_AEAD_CTX *ctx, bssl::Span<const CRYPTO_IOVEC> iovecs,
               uint8_t *out_tag, size_t *out_tag_len, size_t max_out_tag_len,
               const uint8_t *nonce, size_t nonce_len,
               bssl::Span<const CRYPTO_IVEC> aadvecs);

  int (*openv_detached)(const EVP_AEAD_CTX *ctx,
                        bssl::Span<const CRYPTO_IOVEC> iovecs,
                        const uint8_t *nonce, size_t nonce_len,
                        const uint8_t *in_tag, size_t in_tag_len,
                        bssl::Span<const CRYPTO_IVEC> aadvecs);

  int (*get_iv)(const EVP_AEAD_CTX *ctx, const uint8_t **out_iv,
                size_t *out_len);

  size_t (*tag_len)(const EVP_AEAD_CTX *ctx, size_t in_len,
                    size_t extra_in_len);
};

struct evp_cipher_st {
  // type contains a NID identifying the cipher. (e.g. NID_aes_128_gcm.)
  int nid;

  // block_size contains the block size, in bytes, of the cipher, or 1 for a
  // stream cipher.
  unsigned block_size;

  // key_len contains the key size, in bytes, for the cipher. If the cipher
  // takes a variable key size then this contains the default size.
  unsigned key_len;

  // iv_len contains the IV size, in bytes, or zero if inapplicable.
  unsigned iv_len;

  // ctx_size contains the size, in bytes, of the per-key context for this
  // cipher.
  unsigned ctx_size;

  // flags contains the OR of a number of flags. See |EVP_CIPH_*|.
  uint32_t flags;

  int (*init)(EVP_CIPHER_CTX *ctx, const uint8_t *key, const uint8_t *iv,
              int enc);

  // cipher encrypts/decrypts |in|, write output to |out|. Writes exactly |len|
  // bytes, which must be a multiple of the |block_size|.
  //
  // For ciphers where encryption and decryption operations differ, |init|
  // shall set an internal state for this.
  //
  // Returns 1 on success, or 0 on error.
  int (*cipher_update)(EVP_CIPHER_CTX *ctx, uint8_t *out, const uint8_t *in,
                       size_t len);

  // cipher_final finalizes the cipher, performing possible final
  // authentication checks.
  //
  // Only used for |EVP_CIPH_FLAG_CUSTOM_CIPHER| ciphers.
  //
  // Returns 1 on success, or 0 on error. When decrypting, if an error is
  // returned, the decrypted data must not be used.
  int (*cipher_final)(EVP_CIPHER_CTX *ctx);

  // update_aad adds |in| (of length |inl|) to the authenticated data for the
  // encryption operation.
  //
  // Only used for |EVP_CIPH_FLAG_CUSTOM_CIPHER| ciphers.
  //
  // Returns 1 on success, or 0 on error.
  int (*update_aad)(EVP_CIPHER_CTX *ctx, const uint8_t *in, size_t inl);

  // cleanup, if non-NULL, releases memory associated with the context. It is
  // called if |EVP_CTRL_INIT| succeeds. Note that |init| may not have been
  // called at this point.
  void (*cleanup)(EVP_CIPHER_CTX *);

  int (*ctrl)(EVP_CIPHER_CTX *, int type, int arg, void *ptr);
};

}  // extern C

BSSL_NAMESPACE_BEGIN

// CopySpan copies an entire span of bytes from |from| to |to|.
//
// The spans need to have the same length.
inline void CopySpan(Span<const uint8_t> from, Span<uint8_t> to) {
  BSSL_CHECK(from.size() == to.size());
  std::copy(from.begin(), from.end(), to.begin());
}

// CopyToPrefix copies a span of bytes from |from| into |to|. It aborts
// if there is not enough space.
//
// TODO(crbug.com/404286922): Can we simplify this in a C++20 world (e.g.
// std::ranges::copy)? Must preserve range checking on the destination span.
inline void CopyToPrefix(Span<const uint8_t> from, Span<uint8_t> to) {
  CopySpan(from, to.first(from.size()));
}

// Generic CRYPTO_IOVEC/CRYPTO_IVEC helpers.
namespace iovec {

// IsValid returns whether the given |CRYPTO_IVEC| or |CRYPTO_IOVEC| is
// valid for use with public APIs, i.e. does not contain more than |SIZE_MAX|
// bytes and not more than |CRYPTO_IOVEC_MAX| chunks. Note that the `EVP_AEAD`
// methods need to accept an arbitrary number of chunks.
template <typename IVec>
inline bool IsValid(Span<IVec> ivecs) {
  if (ivecs.size() > CRYPTO_IOVEC_MAX) {
    return false;
  }
  size_t allowed = SIZE_MAX;
  for (const IVec &ivec : ivecs) {
    size_t len = ivec.len;
    if (len > allowed) {
      return false;
    }
    allowed -= len;
  }
  return true;
}

// Length returns the total length in bytes of a given |CRYPTO_IVEC| or
// |CRYPTO_IOVEC|.
template <typename IVec>
inline size_t TotalLength(Span<IVec> ivecs) {
  size_t total = 0;
  for (const IVec &ivec : ivecs) {
    total += ivec.len;
  }
  return total;
}

// GetAndRemoveSuffix takes |suffix_buf.size()| final bytes from the given
// |CRYPTO_IVEC| or |CRYPTO_IOVEC| (mutating said iovec to no longer contain
// those bytes) and returns them.
//
// If the byte range is contained in a single chunk of |ivecs|, it will just
// return that span pointing into |ivecs|; otherwise, it will copy the bytes
// into |out| and return that.
//
// If |ivecs| is too short, returns |nullopt|.
template <typename IVec, typename ReadFromT = const uint8_t *,
          ReadFromT IVec::*ReadFrom = &IVec::in>
inline std::optional<Span<const uint8_t>> GetAndRemoveSuffix(
    Span<uint8_t> suffix_buf, Span<IVec> ivecs) {
  // Get the trivial case out.
  if (suffix_buf.empty()) {
    return suffix_buf;
  }
  // Strip trailing zero length chunks.
  while (!ivecs.empty() && ivecs.back().len == 0) {
    ivecs = ivecs.first(ivecs.size() - 1);
  }
  if (ivecs.empty()) {
    return std::nullopt;
  }
  // Is the requested chunk entirely contained? If so, just return it.
  if (ivecs.back().len >= suffix_buf.size()) {
    ivecs.back().len -= suffix_buf.size();
    return Span(ivecs.back().*ReadFrom + ivecs.back().len, suffix_buf.size());
  }
  // Otherwise, collect it into the buffer while trimming |ivecs|.
  Span<uint8_t> remaining = suffix_buf;
  while (!ivecs.empty()) {
    Span<const uint8_t> src(ivecs.back().*ReadFrom, ivecs.back().len);
    if (src.size() >= remaining.size()) {
      CopySpan(src.last(remaining.size()), remaining);
      ivecs.back().len -= remaining.size();
      return suffix_buf;
    }
    CopySpan(src, remaining.last(src.size()));
    remaining = remaining.first(remaining.size() - src.size());
    ivecs.back().len = 0;
    ivecs = ivecs.first(ivecs.size() - 1);
  }
  return std::nullopt;
}

// GetAndRemoveOutSuffix is like |GetAndRemoveSuffix| but takes from a
// |CRYPTO_IOVEC|'s |out| member instead.
inline std::optional<Span<const uint8_t>> GetAndRemoveOutSuffix(
    Span<uint8_t> out, Span<CRYPTO_IOVEC> iovecs) {
  return GetAndRemoveSuffix<CRYPTO_IOVEC, /*ReadFromT=*/uint8_t *,
                            /*ReadFrom=*/&CRYPTO_IOVEC::out>(out, iovecs);
}

}  // namespace iovec

BSSL_NAMESPACE_END

#endif  // OPENSSL_HEADER_CRYPTO_FIPSMODULE_CIPHER_INTERNAL_H
