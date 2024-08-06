// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/capture/video/video_capture_device_factory.h"

#include <utility>

#include "base/command_line.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/chromium/media/base/media_switches.h"
#include "third_party/chromium/media/capture/video/fake_video_capture_device_factory.h"
#include "third_party/chromium/media/capture/video/file_video_capture_device_factory.h"

namespace media_m96 {

VideoCaptureDeviceFactory::VideoCaptureDeviceFactory() {
  thread_checker_.DetachFromThread();
}

VideoCaptureDeviceFactory::~VideoCaptureDeviceFactory() = default;

}  // namespace media_m96
