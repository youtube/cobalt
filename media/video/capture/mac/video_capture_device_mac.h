// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// OS X implementation of VideoCaptureDevice, using QTKit as native capture API.

#ifndef MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_MAC_H_
#define MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_MAC_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"

@class VideoCaptureDeviceQTKit;

namespace media {

// Called by VideoCaptureManager to open, close and start, stop video capture
// devices.
class VideoCaptureDeviceMac : public VideoCaptureDevice {
 public:
  explicit VideoCaptureDeviceMac(const Name& device_name);
  virtual ~VideoCaptureDeviceMac();

  // VideoCaptureDevice implementation.
  virtual void Allocate(int width,
                        int height,
                        int frame_rate,
                        VideoCaptureDevice::EventHandler* observer) OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void DeAllocate() OVERRIDE;
  virtual const Name& device_name() OVERRIDE;

  bool Init();

  // Called to deliver captured video frames.
  void ReceiveFrame(const uint8* video_frame, int video_frame_length,
                    const VideoCaptureCapability& frame_info);

 private:
  void SetErrorState(const std::string& reason);

  // Flag indicating the internal state.
  enum InternalState {
    kNotInitialized,
    kIdle,
    kAllocated,
    kCapturing,
    kError
  };

  VideoCaptureDevice::Name device_name_;
  VideoCaptureDevice::EventHandler* observer_;
  InternalState state_;
  base::Lock lock_;

  VideoCaptureDeviceQTKit* capture_device_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceMac);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_MAC_H_
