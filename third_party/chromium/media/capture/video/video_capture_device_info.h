// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_INFO_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_INFO_H_

#include "third_party/chromium/media/capture/video/video_capture_device_descriptor.h"
#include "third_party/chromium/media/capture/video_capture_types.h"

namespace media_m96 {

// Bundles a VideoCaptureDeviceDescriptor with corresponding supported
// video formats.
struct CAPTURE_EXPORT VideoCaptureDeviceInfo {
  VideoCaptureDeviceInfo();
  VideoCaptureDeviceInfo(VideoCaptureDeviceDescriptor descriptor);
  VideoCaptureDeviceInfo(const VideoCaptureDeviceInfo& other);
  ~VideoCaptureDeviceInfo();
  VideoCaptureDeviceInfo& operator=(const VideoCaptureDeviceInfo& other);

  VideoCaptureDeviceDescriptor descriptor;
  VideoCaptureFormats supported_formats;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_INFO_H_
