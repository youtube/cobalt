// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Linux specific implementation of VideoCaptureDevice.
// V4L2 is used for capturing. V4L2 does not provide its own thread for
// capturing so this implementation uses a Chromium thread for fetching frames
// from V4L2.

#ifndef MEDIA_VIDEO_CAPTURE_LINUX_VIDEO_CAPTURE_DEVICE_LINUX_H_
#define MEDIA_VIDEO_CAPTURE_LINUX_VIDEO_CAPTURE_DEVICE_LINUX_H_

#include <string>

#include "base/threading/thread.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"

namespace media {

class VideoCaptureDeviceLinux : public VideoCaptureDevice {
 public:
  explicit VideoCaptureDeviceLinux(const Name& device_name);
  virtual ~VideoCaptureDeviceLinux();

  // VideoCaptureDevice implementation.
  virtual void Allocate(int width,
                        int height,
                        int frame_rate,
                        EventHandler* observer) override;
  virtual void Start() override;
  virtual void Stop() override;
  virtual void DeAllocate() override;
  virtual const Name& device_name() override;

 private:
  enum InternalState {
    kIdle,  // The device driver is opened but camera is not in use.
    kAllocated,  // The camera has been allocated and can be started.
    kCapturing,  // Video is being captured.
    kError  // Error accessing HW functions.
            // User needs to recover by destroying the object.
  };

  // Buffers used to receive video frames from with v4l2.
  struct Buffer {
    Buffer() : start(0), length(0) {}
    void* start;
    size_t length;
  };

  // Called on the v4l2_thread_.
  void OnAllocate(int width,
                  int height,
                  int frame_rate,
                  EventHandler* observer);
  void OnStart();
  void OnStop();
  void OnDeAllocate();
  void OnCaptureTask();

  bool AllocateVideoBuffers();
  void DeAllocateVideoBuffers();
  void SetErrorState(const std::string& reason);

  InternalState state_;
  VideoCaptureDevice::EventHandler* observer_;
  Name device_name_;
  int device_fd_;  // File descriptor for the opened camera device.
  base::Thread v4l2_thread_;  // Thread used for reading data from the device.
  Buffer* buffer_pool_;
  int buffer_pool_size_;  // Number of allocated buffers.

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureDeviceLinux);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_LINUX_VIDEO_CAPTURE_DEVICE_LINUX_H_
