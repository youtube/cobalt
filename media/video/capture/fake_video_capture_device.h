// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  // Used for testing. This will make sure the next call to Create will
  // return NULL;
  static void SetFailNextCreate();

  static void GetDeviceNames(Names* device_names);

  // VideoCaptureDevice implementation.
  virtual void Allocate(int width,
                        int height,
                        int frame_rate,
                        VideoCaptureDevice::EventHandler* observer) override;
  virtual void Start() override;
  virtual void Stop() override;
  virtual void DeAllocate() override;
  virtual const Name& device_name() override;

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
  int frame_size_;
  scoped_array<uint8> fake_frame_;
  int frame_count_;
  int frame_width_;
  int frame_height_;

  static bool fail_next_create_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FakeVideoCaptureDevice);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_FAKE_VIDEO_CAPTURE_DEVICE_H_
