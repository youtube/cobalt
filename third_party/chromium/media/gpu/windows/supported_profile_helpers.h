// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_GPU_WINDOWS_SUPPORTED_PROFILE_HELPERS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_WINDOWS_SUPPORTED_PROFILE_HELPERS_H_

#include "base/containers/flat_map.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "third_party/chromium/media/base/video_codecs.h"
#include "third_party/chromium/media/gpu/media_gpu_export.h"
#include "third_party/chromium/media/gpu/windows/d3d11_com_defs.h"
#include "ui/gfx/geometry/size.h"

namespace media_m96 {

struct SupportedResolutionRange {
  gfx::Size min_resolution;
  gfx::Size max_landscape_resolution;
  gfx::Size max_portrait_resolution;
};

using SupportedResolutionRangeMap =
    base::flat_map<VideoCodecProfile, SupportedResolutionRange>;

// Enumerates the extent of hardware decoding support for H.264, VP8, VP9, and
// AV1. If a codec is supported, its minimum and maximum supported resolutions
// are returned under the appropriate VideoCodecProfile entry.
//
// Notes:
// - VP8 and AV1 are only tested if their base::Feature entries are enabled.
// - Only baseline, main, and high H.264 profiles are supported.
MEDIA_GPU_EXPORT
SupportedResolutionRangeMap GetSupportedD3D11VideoDecoderResolutions(
    ComD3D11Device device,
    const gpu::GpuDriverBugWorkarounds& workarounds,
    bool provide_av1_resolutions = true);

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_GPU_WINDOWS_SUPPORTED_PROFILE_HELPERS_H_
