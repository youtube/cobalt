// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/buffers.h"

namespace media {

Buffer::Buffer(base::TimeDelta timestamp, base::TimeDelta duration)
    : timestamp_(timestamp),
      duration_(duration) {
}

Buffer::~Buffer() {}

bool Buffer::IsEndOfStream() const {
  return GetData() == NULL;
}

}  // namespace media
