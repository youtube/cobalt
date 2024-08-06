// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_H_

#include <memory>

#include "third_party/chromium/media/capture/capture_export.h"
#include "third_party/chromium/media/capture/video_capture_types.h"

namespace gfx {
struct GpuMemoryBufferHandle;
}  // namespace gfx

namespace media_m96 {

class VideoCaptureBufferTracker;

class CAPTURE_EXPORT VideoCaptureBufferTrackerFactory {
 public:
  virtual ~VideoCaptureBufferTrackerFactory() {}
  virtual std::unique_ptr<VideoCaptureBufferTracker> CreateTracker(
      VideoCaptureBufferType buffer_type) = 0;
  virtual std::unique_ptr<VideoCaptureBufferTracker>
  CreateTrackerForExternalGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle) = 0;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_H_
