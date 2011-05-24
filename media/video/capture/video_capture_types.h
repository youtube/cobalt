// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_TYPES_H_
#define MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_TYPES_H_

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

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_TYPES_H_

