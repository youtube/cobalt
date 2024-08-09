// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_GPU_GPU_VIDEO_ENCODE_ACCELERATOR_HELPERS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_GPU_VIDEO_ENCODE_ACCELERATOR_HELPERS_H_

#include "third_party/chromium/media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/size.h"

namespace media_m96 {

// Helper functions for VideoEncodeAccelerator implementations in GPU process.

// Calculate the bitstream buffer size for VideoEncodeAccelerator.
// |size|: the resolution of video stream
// |bitrate|: the bit rate in bps
// |framerate|: the frame rate in fps
MEDIA_GPU_EXPORT size_t GetEncodeBitstreamBufferSize(const gfx::Size& size,
                                                     uint32_t bitrate,
                                                     uint32_t framerate);

// Get the maximum bitstream buffer size for VideoEncodeAccelerator.
// |size|: the resolution of video stream
MEDIA_GPU_EXPORT size_t GetEncodeBitstreamBufferSize(const gfx::Size& size);

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_GPU_GPU_VIDEO_ENCODE_ACCELERATOR_HELPERS_H_
