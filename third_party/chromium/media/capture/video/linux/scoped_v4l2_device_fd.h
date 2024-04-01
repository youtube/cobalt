// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_LINUX_SCOPED_V4L2_DEVICE_FD_H_
#define MEDIA_CAPTURE_VIDEO_LINUX_SCOPED_V4L2_DEVICE_FD_H_

#include "media/capture/video/linux/v4l2_capture_device.h"

namespace media {

class ScopedV4L2DeviceFD {
 public:
  static constexpr int kInvalidId = -1;
  explicit ScopedV4L2DeviceFD(V4L2CaptureDevice* v4l2);
  ScopedV4L2DeviceFD(V4L2CaptureDevice* v4l2, int device_fd);
  ~ScopedV4L2DeviceFD();
  int get() const;
  void reset(int fd = kInvalidId);
  bool is_valid() const;

 private:
  int device_fd_;
  V4L2CaptureDevice* const v4l2_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_LINUX_SCOPED_V4L2_DEVICE_FD_H_
