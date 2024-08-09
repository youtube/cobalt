// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_BASE_FORMAT_UTILS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_BASE_FORMAT_UTILS_H_

#include "third_party/chromium/media/base/media_export.h"
#include "third_party/chromium/media/base/video_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/buffer_types.h"

namespace media_m96 {

MEDIA_EXPORT absl::optional<VideoPixelFormat> GfxBufferFormatToVideoPixelFormat(
    gfx::BufferFormat format);

MEDIA_EXPORT absl::optional<gfx::BufferFormat>
VideoPixelFormatToGfxBufferFormat(VideoPixelFormat pixel_format);

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_BASE_FORMAT_UTILS_H_
