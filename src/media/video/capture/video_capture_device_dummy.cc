// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/video_capture_device_dummy.h"

namespace media {

VideoCaptureDevice* VideoCaptureDevice::Create(const Name& device_name) {
  return NULL;
}

void VideoCaptureDevice::GetDeviceNames(Names* device_names) {}

VideoCaptureDeviceDummy::VideoCaptureDeviceDummy() {}

VideoCaptureDeviceDummy::~VideoCaptureDeviceDummy() {}

void VideoCaptureDeviceDummy::Allocate(
    int width, int height, int frame_rate,
    VideoCaptureDevice::EventHandler* observer) {}

void VideoCaptureDeviceDummy::Start() {}

void VideoCaptureDeviceDummy::Stop() {}

void VideoCaptureDeviceDummy::DeAllocate() {}

}  // namespace media
