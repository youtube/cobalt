// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Windows specific implementation of VideoCaptureDevice.
// DirectShow is used for capturing. DirectShow provide its own threads
// for capturing.

#ifndef MEDIA_VIDEO_CAPTURE_WIN_VIDEO_CAPTURE_DEVICE_WIN_H_
#define MEDIA_VIDEO_CAPTURE_WIN_VIDEO_CAPTURE_DEVICE_WIN_H_

// Avoid including strsafe.h via dshow as it will cause build warnings.
#define NO_DSHOW_STRSAFE
#include <dshow.h>

#include <map>
#include <string>

#include "base/threading/non_thread_safe.h"
#include "base/threading/thread.h"
#include "base/win/scoped_comptr.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"
#include "media/video/capture/win/capability_list_win.h"
#include "media/video/capture/win/sink_filter_win.h"
#include "media/video/capture/win/sink_input_pin_win.h"

namespace media {

// All the methods in the class can only be run on a COM initialized thread.
class VideoCaptureDeviceWin
    : public base::NonThreadSafe,
      public VideoCaptureDevice,
      public SinkFilterObserver {
 public:
  explicit VideoCaptureDeviceWin(const Name& device_name);
  virtual ~VideoCaptureDeviceWin();
  // Opens the device driver for this device.
  // This function is used by the static VideoCaptureDevice::Create function.
  bool Init();

  // VideoCaptureDevice implementation.
  virtual void Allocate(int width,
                        int height,
                        int frame_rate,
                        VideoCaptureDevice::EventHandler* observer) override;
  virtual void Start() override;
  virtual void Stop() override;
  virtual void DeAllocate() override;
  virtual const Name& device_name() override;

  static void GetDeviceNames(Names* device_names);

 private:
  enum InternalState {
    kIdle,  // The device driver is opened but camera is not in use.
    kAllocated,  // The camera has been allocated and can be started.
    kCapturing,  // Video is being captured.
    kError  // Error accessing HW functions.
            // User needs to recover by destroying the object.
  };

  // Implements SinkFilterObserver.
  virtual void FrameReceived(const uint8* buffer, int length);

  bool CreateCapabilityMap();
  void SetErrorState(const char* reason);

  Name device_name_;
  InternalState state_;
  VideoCaptureDevice::EventHandler* observer_;

  base::win::ScopedComPtr<IBaseFilter> capture_filter_;
  base::win::ScopedComPtr<IGraphBuilder> graph_builder_;
  base::win::ScopedComPtr<IMediaControl> media_control_;
  base::win::ScopedComPtr<IPin> input_sink_pin_;
  base::win::ScopedComPtr<IPin> output_capture_pin_;
  // Used when using a MJPEG decoder.
  base::win::ScopedComPtr<IBaseFilter> mjpg_filter_;
  base::win::ScopedComPtr<IPin> input_mjpg_pin_;
  base::win::ScopedComPtr<IPin> output_mjpg_pin_;

  scoped_refptr<SinkFilter> sink_filter_;

  // Map of all capabilities this device support.
  CapabilityList capabilities_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureDeviceWin);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_WIN_VIDEO_CAPTURE_DEVICE_WIN_H_
