// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "media/webm/webm_content_encodings.h"

namespace media {

ContentEncoding::ContentEncoding()
    : order_(kOrderInvalid),
      scope_(kScopeInvalid),
      type_(kTypeInvalid),
      encryption_algo_(kEncAlgoInvalid),
      encryption_key_id_size_(0) {
}

ContentEncoding::~ContentEncoding() {}

void ContentEncoding::SetEncryptionKeyId(const uint8* encryption_key_id,
                                         int size) {
  DCHECK(encryption_key_id);
  DCHECK_GT(size, 0);
  encryption_key_id_.reset(new uint8[size]);
  memcpy(encryption_key_id_.get(), encryption_key_id, size);
  encryption_key_id_size_ = size;
}

}  // namespace media
