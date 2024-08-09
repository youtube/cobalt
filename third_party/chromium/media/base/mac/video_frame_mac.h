// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_BASE_MAC_VIDEO_FRAME_MAC_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_BASE_MAC_VIDEO_FRAME_MAC_H_

#include <CoreVideo/CVPixelBuffer.h>

#include "base/mac/scoped_cftyperef.h"
#include "third_party/chromium/media/base/media_export.h"

namespace media_m96 {

class VideoFrame;

// Wrap a VideoFrame's data in a CVPixelBuffer object. The frame's lifetime is
// extended for the duration of the pixel buffer's lifetime. If the frame's data
// is already managed by a CVPixelBuffer (the frame was created using
// |WrapCVPixelBuffer()|, then the underlying CVPixelBuffer is returned.
//
// The only supported formats are I420 and NV12. Frames with extended pixels
// (the visible rect's size does not match the coded size) are not supported.
// If an unsupported frame is specified, null is returned.
MEDIA_EXPORT base::ScopedCFTypeRef<CVPixelBufferRef>
WrapVideoFrameInCVPixelBuffer(const VideoFrame& frame);

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_BASE_MAC_VIDEO_FRAME_MAC_H_
