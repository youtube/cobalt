// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/buffers.h"

namespace media {

StreamSample::StreamSample() {}

StreamSample::~StreamSample() {}

bool Buffer::IsEndOfStream() const {
  return GetData() == NULL;
}

}  // namespace media
