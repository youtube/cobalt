// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decrypt_config.h"

#include "base/logging.h"

namespace media {

DecryptConfig::DecryptConfig(const uint8* key_id, int key_id_size,
                             const uint8* iv, int iv_size,
                             const uint8* checksum, int checksum_size,
                             int encrypted_frame_offset)
    :  key_id_(new uint8[key_id_size]),
       key_id_size_(key_id_size),
       iv_(new uint8[iv_size]),
       iv_size_(iv_size),
       checksum_(checksum_size > 0 ? new uint8[checksum_size] : NULL),
       checksum_size_(checksum_size),
       encrypted_frame_offset_(encrypted_frame_offset) {
  CHECK_GT(key_id_size, 0);
  CHECK_EQ(iv_size, DecryptConfig::kDecryptionKeySize);
  CHECK_GE(checksum_size, 0);
  CHECK_GE(encrypted_frame_offset, 0);
  memcpy(key_id_.get(), key_id, key_id_size);
  memcpy(iv_.get(), iv, iv_size);
  if (checksum_size > 0)
    memcpy(checksum_.get(), checksum, checksum_size);
}

DecryptConfig::~DecryptConfig() {}

}  // namespace media
