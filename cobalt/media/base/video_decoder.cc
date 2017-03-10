// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/base/video_decoder.h"

#include "cobalt/media/base/video_frame.h"

namespace cobalt {
namespace media {

VideoDecoder::VideoDecoder() {}

VideoDecoder::~VideoDecoder() {}

bool VideoDecoder::NeedsBitstreamConversion() const { return false; }

bool VideoDecoder::CanReadWithoutStalling() const { return true; }

int VideoDecoder::GetMaxDecodeRequests() const { return 1; }

}  // namespace media
}  // namespace cobalt
