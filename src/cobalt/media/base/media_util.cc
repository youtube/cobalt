// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/base/media_util.h"

namespace media {

std::vector<uint8_t> EmptyExtraData() { return std::vector<uint8_t>(); }

EncryptionScheme Unencrypted() { return EncryptionScheme(); }

EncryptionScheme AesCtrEncryptionScheme() {
  return EncryptionScheme(EncryptionScheme::CIPHER_MODE_AES_CTR,
                          EncryptionScheme::Pattern());
}

}  // namespace media
