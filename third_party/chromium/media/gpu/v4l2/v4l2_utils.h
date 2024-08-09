// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_GPU_V4L2_V4L2_UTILS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_V4L2_V4L2_UTILS_H_

#include <string>

#include <linux/videodev2.h>

namespace media_m96 {

// Returns a human readable description of |memory|.
const char* V4L2MemoryToString(v4l2_memory memory);

// Returns a human readable description of |format|.
std::string V4L2FormatToString(const struct v4l2_format& format);

// Returns a human readable description of |buffer|
std::string V4L2BufferToString(const struct v4l2_buffer& buffer);
}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_GPU_V4L2_V4L2_UTILS_H_
