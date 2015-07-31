// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A dummy implementation of VideoCaptureDevice to use for platforms without
// real video capture support. The class will be removed once the other
// platforms have real video capture device support.
//
// TODO(mflodman) Remove when video_capture_device_mac and
// video_capture_device_win are available.

#ifndef MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_DUMMY_H_
#define MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_DUMMY_H_

#include "base/compiler_specific.h"
#include "media/video/capture/video_capture_device.h"

namespace media {

class VideoCaptureDeviceDummy : public VideoCaptureDevice {
 public:
  virtual void Allocate(int width, int height, int frame_rate,
                        VideoCaptureDevice::EventHandler* observer) OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void DeAllocate() OVERRIDE;

 private:
  VideoCaptureDeviceDummy();
  virtual ~VideoCaptureDeviceDummy();

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceDummy);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_DUMMY_H_
