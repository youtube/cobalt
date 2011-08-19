// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/hmac.h"

#include <algorithm>

#include "base/logging.h"

namespace crypto {

// Performs a constant-time comparison of two strings, returning true if the
// strings are equal.
//
// For cryptographic operations, comparison functions such as memcmp() may
// expose side-channel information about input, allowing an attacker to
// perform timing analysis to determine what the expected bits should be. In
// order to avoid such attacks, the comparison must execute in constant time,
// so as to not to reveal to the attacker where the difference(s) are.
// For an example attack, see
// http://groups.google.com/group/keyczar-discuss/browse_thread/thread/5571eca0948b2a13
static bool SecureMemcmp(const void* s1, const void* s2, size_t n) {
  const unsigned char* s1_ptr = reinterpret_cast<const unsigned char*>(s1);
  const unsigned char* s2_ptr = reinterpret_cast<const unsigned char*>(s2);
  unsigned char tmp = 0;
  for (size_t i = 0; i < n; ++i, ++s1_ptr, ++s2_ptr)
    tmp |= *s1_ptr ^ *s2_ptr;
  return (tmp == 0);
}

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

  return SecureMemcmp(digest.data(), computed_digest.get(),
                      std::min(digest.size(), digest_length));
}

}  // namespace crypto
