// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Windows specific implementation of VideoCaptureDevice.
// DirectShow is used for capturing. DirectShow provide its own threads
// for capturing.

#ifndef MEDIA_VIDEO_CAPTURE_WIN_CAPABILITY_LIST_WIN_H_
#define MEDIA_VIDEO_CAPTURE_WIN_CAPABILITY_LIST_WIN_H_

#include <list>

#include "base/threading/non_thread_safe.h"
#include "media/video/capture/video_capture_types.h"

namespace media {

struct VideoCaptureCapabilityWin : public VideoCaptureCapability {
  explicit VideoCaptureCapabilityWin(int index) : stream_index(index) {}
  int stream_index;
};

class CapabilityList : public base::NonThreadSafe {
 public:
  CapabilityList();
  ~CapabilityList();

  bool empty() const { return capabilities_.empty(); }

  // Appends an entry to the list.
  void Add(const VideoCaptureCapabilityWin& capability);

  // Loops through the list of capabilities and returns an index of the best
  // matching capability.  The algorithm prioritizes height, width, frame rate
  // and color format in that order.
  const VideoCaptureCapabilityWin& GetBestMatchedCapability(
      int requested_width, int requested_height,
      int requested_frame_rate) const;

 private:
  typedef std::list<VideoCaptureCapabilityWin> Capabilities;
  Capabilities capabilities_;

  DISALLOW_COPY_AND_ASSIGN(CapabilityList);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_WIN_CAPABILITY_LIST_WIN_H_
