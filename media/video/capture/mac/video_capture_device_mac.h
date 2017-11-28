// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// OS X implementation of VideoCaptureDevice, using QTKit as native capture API.

#ifndef MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_MAC_H_
#define MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_MAC_H_

#include <string>

#include "base/compiler_specific.h"
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
                        VideoCaptureDevice::EventHandler* observer) override;
  virtual void Start() override;
  virtual void Stop() override;
  virtual void DeAllocate() override;
  virtual const Name& device_name() override;

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

  VideoCaptureDeviceQTKit* capture_device_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceMac);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_MAC_H_
