// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_POOL_UTIL_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_POOL_UTIL_H_

#include "third_party/chromium/media/capture/capture_export.h"

namespace media_m96 {

constexpr int kVideoCaptureDefaultMaxBufferPoolSize = 4;
CAPTURE_EXPORT int DeviceVideoCaptureMaxBufferPoolSize();

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_POOL_UTIL_H_
