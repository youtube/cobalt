// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of a fake VideoCaptureDevice class. Used for testing other
// video capture classes when no real hardware is available.

#ifndef MEDIA_VIDEO_CAPTURE_FAKE_VIDEO_CAPTURE_DEVICE_H_
#define MEDIA_VIDEO_CAPTURE_FAKE_VIDEO_CAPTURE_DEVICE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "media/video/capture/video_capture_device.h"

namespace media {

class MEDIA_EXPORT FakeVideoCaptureDevice : public VideoCaptureDevice {
 public:
  static VideoCaptureDevice* Create(const Name& device_name);
  virtual ~FakeVideoCaptureDevice();

  static void GetDeviceNames(Names* device_names);

  // VideoCaptureDevice implementation.
  virtual void Allocate(int width,
                        int height,
                        int frame_rate,
                        VideoCaptureDevice::EventHandler* observer);
  virtual void Start();
  virtual void Stop();
  virtual void DeAllocate();
  virtual const Name& device_name();

 private:
  // Flag indicating the internal state.
  enum InternalState {
    kIdle,
    kAllocated,
    kCapturing,
    kError
  };
  explicit FakeVideoCaptureDevice(const Name& device_name);

  // Called on the capture_thread_.
  void OnCaptureTask();

  Name device_name_;
  VideoCaptureDevice::EventHandler* observer_;
  InternalState state_;
  base::Thread capture_thread_;
  scoped_array<uint8> fake_frame_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FakeVideoCaptureDevice);
};

}  // namespace media

DISABLE_RUNNABLE_METHOD_REFCOUNT(media::FakeVideoCaptureDevice);

#endif  // MEDIA_VIDEO_CAPTURE_FAKE_VIDEO_CAPTURE_DEVICE_H_
