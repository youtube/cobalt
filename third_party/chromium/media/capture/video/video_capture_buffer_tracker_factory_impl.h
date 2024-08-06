// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_IMPL_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_IMPL_H_

#include <memory>

#include "third_party/chromium/media/capture/capture_export.h"
#include "third_party/chromium/media/capture/video/video_capture_buffer_tracker_factory.h"

namespace media_m96 {

class CAPTURE_EXPORT VideoCaptureBufferTrackerFactoryImpl
    : public VideoCaptureBufferTrackerFactory {
 public:
  std::unique_ptr<VideoCaptureBufferTracker> CreateTracker(
      VideoCaptureBufferType buffer_type) override;
  std::unique_ptr<VideoCaptureBufferTracker>
  CreateTrackerForExternalGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle) override;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_IMPL_H_
