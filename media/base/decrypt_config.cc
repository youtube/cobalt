// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decrypt_config.h"

#include "base/logging.h"

namespace media {

DecryptConfig::DecryptConfig(const uint8* key_id, int key_id_size)
    : key_id_size_(key_id_size) {
  CHECK_GT(key_id_size, 0);
  key_id_.reset(new uint8[key_id_size]);
  memcpy(key_id_.get(), key_id, key_id_size);
}

DecryptConfig::~DecryptConfig() {}

}  // namespace media
