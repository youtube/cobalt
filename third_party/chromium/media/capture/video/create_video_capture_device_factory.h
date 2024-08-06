// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_CREATE_VIDEO_CAPTURE_DEVICE_FACTORY_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_CREATE_VIDEO_CAPTURE_DEVICE_FACTORY_H_

#include <memory>

#include "base/single_thread_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "third_party/chromium/media/capture/capture_export.h"
#include "third_party/chromium/media/capture/video/video_capture_device_factory.h"

namespace media_m96 {

bool CAPTURE_EXPORT ShouldUseFakeVideoCaptureDeviceFactory();

std::unique_ptr<VideoCaptureDeviceFactory> CAPTURE_EXPORT
CreateVideoCaptureDeviceFactory(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_CREATE_VIDEO_CAPTURE_DEVICE_FACTORY_H_
