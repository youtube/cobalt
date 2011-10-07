// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/hmac.h"

#include <algorithm>

#include "base/logging.h"
#include "crypto/secure_util.h"

namespace crypto {

size_t HMAC::DigestLength() const {
  switch (hash_alg_) {
    case SHA1:
      return 20;
    case SHA256:
      return 32;
    default:
      NOTREACHED();
      return 0;
  }
}

bool HMAC::Verify(const base::StringPiece& data,
                  const base::StringPiece& digest) const {
  if (digest.size() != DigestLength())
    return false;
  return VerifyTruncated(data, digest);
}

bool HMAC::VerifyTruncated(const base::StringPiece& data,
                           const base::StringPiece& digest) const {
  if (digest.empty())
    return false;
  size_t digest_length = DigestLength();
  scoped_array<unsigned char> computed_digest(
      new unsigned char[digest_length]);
  if (!Sign(data, computed_digest.get(), static_cast<int>(digest_length)))
    return false;

  return SecureMemEqual(digest.data(), computed_digest.get(),
                        std::min(digest.size(), digest_length));
}

}  // namespace crypto
