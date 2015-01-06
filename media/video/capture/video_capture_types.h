// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_TYPES_H_
#define MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_TYPES_H_

#include "media/base/video_frame.h"

namespace media {

// TODO(wjia): this type should be defined in a common place and
// shared with device manager.
typedef int VideoCaptureSessionId;

// Parameters for starting video capture and device information.
struct VideoCaptureParams {
  int width;
  int height;
  int frame_per_second;
  VideoCaptureSessionId session_id;
};

// Capabilities describe the format a camera capture video in.
struct VideoCaptureCapability {
  // Color formats from camera.
  enum Format {
    kColorUnknown,  // Color format not set.
    kI420,
    kYUY2,
    kUYVY,
    kRGB24,
    kARGB,
    kMJPEG,
    kNV21,
    kYV12,
  };

  int width;  // Desired width.
  int height;  // Desired height.
  int frame_rate;  // Desired frame rate.
  Format color;  // Desired video type.
  int expected_capture_delay;  // Expected delay in millisecond.
  bool interlaced;  // Need interlace format.
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_TYPES_H_
