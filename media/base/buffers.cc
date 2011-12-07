// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/buffers.h"

namespace media {

const base::TimeDelta kNoTimestamp =
    base::TimeDelta::FromMicroseconds(kint64min);

const base::TimeDelta kInfiniteDuration =
    base::TimeDelta::FromMicroseconds(kint64max);

StreamSample::StreamSample() {}

StreamSample::~StreamSample() {}

bool Buffer::IsEndOfStream() const {
  return GetData() == NULL;
}

}  // namespace media
