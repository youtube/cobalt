// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECRYPT_CONFIG_H_
#define MEDIA_BASE_DECRYPT_CONFIG_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"

namespace media {

// Contains all information that a decryptor needs to decrypt.
class MEDIA_EXPORT DecryptConfig {
 public:
  explicit DecryptConfig(const uint8* key_id, int key_id_size);
  ~DecryptConfig();

  const uint8* key_id() const { return key_id_.get(); }
  int key_id_size() const { return key_id_size_; }

 private:
  scoped_array<uint8> key_id_;
  int key_id_size_;

  DISALLOW_COPY_AND_ASSIGN(DecryptConfig);
};

}  // namespace media

#endif  // MEDIA_BASE_DECRYPT_CONFIG_H_
