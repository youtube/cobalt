// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility class for calculating the HMAC for a given message. We currently
// only support SHA1 for the hash algorithm, but this can be extended easily.

#ifndef CRYPTO_HMAC_H_
#define CRYPTO_HMAC_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "crypto/crypto_api.h"

namespace crypto {

// Simplify the interface and reduce includes by abstracting out the internals.
struct HMACPlatformData;

class CRYPTO_API HMAC {
 public:
  // The set of supported hash functions. Extend as required.
  enum HashAlgorithm {
    SHA1,
    SHA256,
  };

  explicit HMAC(HashAlgorithm hash_alg);
  ~HMAC();

  // Returns the length of digest that this HMAC will create.
  size_t DigestLength() const;

  // TODO(abarth): Add a PreferredKeyLength() member function.

  // Initializes this instance using |key| of the length |key_length|. Call Init
  // only once. It returns false on the second or later calls.
  // TODO(abarth): key_length should be a size_t.
  bool Init(const unsigned char* key, int key_length) WARN_UNUSED_RESULT;

  // Initializes this instance using |key|. Call Init only once. It returns
  // false on the second or later calls.
  bool Init(const base::StringPiece& key) WARN_UNUSED_RESULT {
    return Init(reinterpret_cast<const unsigned char*>(key.data()),
                static_cast<int>(key.size()));
  }

  // Calculates the HMAC for the message in |data| using the algorithm supplied
  // to the constructor and the key supplied to the Init method. The HMAC is
  // returned in |digest|, which has |digest_length| bytes of storage available.
  // TODO(abarth): digest_length should be a size_t.
  bool Sign(const base::StringPiece& data, unsigned char* digest,
            int digest_length) const WARN_UNUSED_RESULT;

  // Verifies that the HMAC for the message in |data| equals the HMAC provided
  // in |digest|, using the algorithm supplied to the constructor and the key
  // supplied to the Init method. Use of this method is strongly recommended
  // over using Sign() with a manual comparison (such as memcmp), as such
  // comparisons may result in side-channel disclosures, such as timing, that
  // undermine the cryptographic integrity. |digest| must be exactly
  // |DigestLength()| bytes long.
  bool Verify(const base::StringPiece& data,
              const base::StringPiece& digest) const WARN_UNUSED_RESULT;

  // Verifies a truncated HMAC, behaving identical to Verify(), except
  // that |digest| is allowed to be smaller than |DigestLength()|.
  bool VerifyTruncated(
      const base::StringPiece& data,
      const base::StringPiece& digest) const WARN_UNUSED_RESULT;

 private:
  HashAlgorithm hash_alg_;
  scoped_ptr<HMACPlatformData> plat_;

  DISALLOW_COPY_AND_ASSIGN(HMAC);
};

}  // namespace crypto

#endif  // CRYPTO_HMAC_H_
