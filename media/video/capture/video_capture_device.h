// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VideoCaptureDevice is the abstract base class for realizing video capture
// device support in Chromium. It provides the interface for OS dependent
// implementations.
// The class is created and functions are invoked on a thread owned by
// VideoCaptureManager. Capturing is done on other threads depended on the OS
// specific implementation.

#ifndef MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_H_
#define MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_H_

#include <list>
#include <string>

#include "base/time.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT VideoCaptureDevice {
 public:

  struct Name {
    // Friendly name of a device
    std::string device_name;

    // Unique name of a device. Even if there are multiple devices with the same
    // friendly name connected to the computer this will be unique.
    std::string unique_id;
  };
  typedef std::list<Name> Names;

  // Color formats from camera.
  enum Format {
    kColorUnknown,  // Color format not set.
    kI420,
    kYUY2,
    kUYVY,
    kRGB24,
    kARGB,
    kMJPEG,  // Currently only supported on Windows.
  };

  // Describes the format a camera capture video in.
  struct Capability {
    Capability()
        : width(0),
          height(0),
          frame_rate(0),
          color(kColorUnknown) {}
    int width;
    int height;
    int frame_rate;
    Format color;
  };

  class EventHandler {
   public:
    // Captured a new video frame.
    virtual void OnIncomingCapturedFrame(const uint8* data,
                                         int length,
                                         base::Time timestamp) = 0;
    // An error has occurred that can not be handled
    // and VideoCaptureDevice must be DeAllocated.
    virtual void OnError() = 0;
    // Called when VideoCaptureDevice::Allocate has been called
    // to inform of the resulting frame size and color format.
    virtual void OnFrameInfo(const Capability& info) = 0;

   protected:
    virtual ~EventHandler() {}
  };
  // Creates a VideoCaptureDevice object.
  // Return NULL if the hardware is not available.
  static VideoCaptureDevice* Create(const Name& device_name);
  virtual ~VideoCaptureDevice() {}

  // Gets the names of all video capture devices connected to this computer.
  static void GetDeviceNames(Names* device_names);

  // Prepare the camera for use. After this function has been called no other
  // applications can use the camera. On completion EventHandler::OnFrameInfo is
  // called informing of the resulting resolution and frame rate.
  // DeAllocate must be called before this function can be called again and
  // before the object is deleted.
  virtual void Allocate(int width,
                        int height,
                        int frame_rate,
                        EventHandler* observer) = 0;

  // Start capturing video frames. Allocate must be called before this function.
  virtual void Start() = 0;

  // Stop capturing video frames.
  virtual void Stop() = 0;

  // DeAllocates the camera. This means other applications can use it.
  // After this function has been called the Capture device is reset to the
  // state it was when created.
  virtual void DeAllocate() = 0;

  // Get the name of the capture device.
  virtual const Name& device_name() = 0;
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_H_
