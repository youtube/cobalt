// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/video/gpu_video_accelerator_factories.h"

namespace media_m96 {

GpuVideoAcceleratorFactories::Supported
GpuVideoAcceleratorFactories::IsDecoderConfigSupportedOrUnknown(
    const VideoDecoderConfig& config) {
  if (!IsDecoderSupportKnown())
    return Supported::kUnknown;
  return IsDecoderConfigSupported(config);
}

}  // namespace media_m96
