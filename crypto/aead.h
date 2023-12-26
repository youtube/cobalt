// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_AEAD_H_
#define CRYPTO_AEAD_H_

#include <stddef.h>

#include <string>

#include "base/strings/string_piece.h"
#include "crypto/crypto_export.h"

struct evp_aead_st;

namespace crypto {

// This class exposes the AES-128-CTR-HMAC-SHA256 and AES_256_GCM AEAD.
class CRYPTO_EXPORT Aead {
 public:
  enum AeadAlgorithm { AES_128_CTR_HMAC_SHA256, AES_256_GCM, AES_256_GCM_SIV };

  explicit Aead(AeadAlgorithm algorithm);

  ~Aead();

  void Init(const std::string* key);

  bool Seal(base::StringPiece plaintext,
            base::StringPiece nonce,
            base::StringPiece additional_data,
            std::string* ciphertext) const;

  bool Open(base::StringPiece ciphertext,
            base::StringPiece nonce,
            base::StringPiece additional_data,
            std::string* plaintext) const;

  size_t KeyLength() const;

  size_t NonceLength() const;

 private:
  const std::string* key_;
  const evp_aead_st* aead_;
};

}  // namespace crypto

#endif  // CRYPTO_AEAD_H_
