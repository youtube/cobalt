// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/gpu/android/mock_codec_image.h"

namespace media_m96 {

MockCodecImage::MockCodecImage(const gfx::Size& coded_size)
    : CodecImage(coded_size, /*lock=*/nullptr) {}

MockCodecImage::~MockCodecImage() = default;

}  // namespace media_m96
