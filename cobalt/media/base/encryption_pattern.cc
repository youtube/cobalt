// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/encryption_pattern.h"

namespace cobalt {
namespace media {

EncryptionPattern::EncryptionPattern() = default;

EncryptionPattern::EncryptionPattern(uint32_t crypt_byte_block,
                                     uint32_t skip_byte_block)
    : crypt_byte_block_(crypt_byte_block), skip_byte_block_(skip_byte_block) {}

EncryptionPattern::~EncryptionPattern() = default;

bool EncryptionPattern::Matches(const EncryptionPattern& other) const {
  return crypt_byte_block_ == other.crypt_byte_block() &&
         skip_byte_block_ == other.skip_byte_block();
}

bool EncryptionPattern::IsInEffect() const {
  return crypt_byte_block_ != 0 && skip_byte_block_ != 0;
}

}  // namespace media
}  // namespace cobalt
