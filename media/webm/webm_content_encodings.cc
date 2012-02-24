// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

}  // namespace media
