// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_TYPES_H_
#define MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_TYPES_H_

#include "content/common/media_stream/media_stream_types.h"

namespace media {

// Parameters for starting video capture and device information.
struct VideoCaptureParams {
  int width;
  int height;
  int frame_per_second;
  media_stream::MediaCaptureSessionId session_id;
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_TYPES_H_
