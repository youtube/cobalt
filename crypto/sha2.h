// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_SHA2_H_
#define CRYPTO_SHA2_H_
#pragma once

#include <string>

namespace crypto {

// These functions perform SHA-256 operations.
//
// Functions for SHA-384 and SHA-512 can be added when the need arises.

enum {
  SHA256_LENGTH = 32  // length in bytes of a SHA-256 hash
};

// Computes the SHA-256 hash of the input string 'str' and stores the first
// 'len' bytes of the hash in the output buffer 'output'.  If 'len' > 32,
// only 32 bytes (the full hash) are stored in the 'output' buffer.
void SHA256HashString(const std::string& str,
                      void* output, size_t len);

// Convenience version of the above that returns the result in a 32-byte
// string.
std::string SHA256HashString(const std::string& str);

}  // namespace crypto

#endif  // CRYPTO_SHA2_H_
