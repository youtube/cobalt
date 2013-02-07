// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_decoder.h"

namespace media {

VideoDecoder::VideoDecoder() {}

VideoDecoder::~VideoDecoder() {}

bool VideoDecoder::HasAlpha() const {
  return false;
}

}  // namespace media
