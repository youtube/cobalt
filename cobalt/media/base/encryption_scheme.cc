// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/base/encryption_scheme.h"

namespace cobalt {
namespace media {

EncryptionScheme::EncryptionScheme() : mode_(CIPHER_MODE_UNENCRYPTED) {}

EncryptionScheme::EncryptionScheme(CipherMode mode,
                                   const EncryptionPattern& pattern)
    : mode_(mode), pattern_(pattern) {}

EncryptionScheme::~EncryptionScheme() {}

bool EncryptionScheme::Matches(const EncryptionScheme& other) const {
  return mode_ == other.mode_ && pattern_ == other.pattern_;
}

}  // namespace media
}  // namespace cobalt
