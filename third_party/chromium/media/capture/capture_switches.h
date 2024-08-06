// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_CAPTURE_SWITCHES_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_CAPTURE_SWITCHES_H_

#include "third_party/chromium/media/capture/capture_export.h"

namespace switches {

CAPTURE_EXPORT extern const char kVideoCaptureUseGpuMemoryBuffer[];
CAPTURE_EXPORT extern const char kDisableVideoCaptureUseGpuMemoryBuffer[];

CAPTURE_EXPORT bool IsVideoCaptureUseGpuMemoryBufferEnabled();

}  // namespace switches

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_CAPTURE_SWITCHES_H_
