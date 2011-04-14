// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/hmac.h"

#include <openssl/hmac.h>

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "crypto/openssl_util.h"

namespace crypto {

struct HMACPlatformData {
  std::vector<unsigned char> key;
};

HMAC::HMAC(HashAlgorithm hash_alg)
    : hash_alg_(hash_alg), plat_(new HMACPlatformData()) {
  // Only SHA-1 and SHA-256 hash algorithms are supported now.
  DCHECK(hash_alg_ == SHA1 || hash_alg_ == SHA256);
}

bool HMAC::Init(const unsigned char* key, int key_length) {
  // Init must not be called more than once on the same HMAC object.
  DCHECK(plat_->key.empty());

  plat_->key.assign(key, key + key_length);
  return true;
}

HMAC::~HMAC() {
  // Zero out key copy.
  plat_->key.assign(plat_->key.size(), 0);
  STLClearObject(&plat_->key);
}

bool HMAC::Sign(const std::string& data,
                unsigned char* digest,
                int digest_length) {
  DCHECK_GE(digest_length, 0);
  DCHECK(!plat_->key.empty());  // Init must be called before Sign.

  ScopedOpenSSLSafeSizeBuffer<EVP_MAX_MD_SIZE> result(digest, digest_length);
  return ::HMAC(hash_alg_ == SHA1 ? EVP_sha1() : EVP_sha256(),
                &plat_->key[0], plat_->key.size(),
                reinterpret_cast<const unsigned char*>(data.data()),
                data.size(),
                result.safe_buffer(), NULL);
}

}  // namespace crypto
