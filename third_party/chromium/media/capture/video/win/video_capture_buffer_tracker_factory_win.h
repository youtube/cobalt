// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_WIN_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_WIN_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "third_party/chromium/media/base/win/dxgi_device_manager.h"
#include "third_party/chromium/media/capture/capture_export.h"
#include "third_party/chromium/media/capture/video/video_capture_buffer_tracker_factory.h"

namespace media_m96 {

class CAPTURE_EXPORT VideoCaptureBufferTrackerFactoryWin
    : public VideoCaptureBufferTrackerFactory {
 public:
  VideoCaptureBufferTrackerFactoryWin();
  ~VideoCaptureBufferTrackerFactoryWin() override;
  std::unique_ptr<VideoCaptureBufferTracker> CreateTracker(
      VideoCaptureBufferType buffer_type) override;
  std::unique_ptr<VideoCaptureBufferTracker>
  CreateTrackerForExternalGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle) override;

 private:
  scoped_refptr<DXGIDeviceManager> dxgi_device_manager_;
  base::WeakPtrFactory<VideoCaptureBufferTrackerFactoryWin> weak_factory_{this};
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_WIN_H_