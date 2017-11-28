// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Windows specific implementation of VideoCaptureDevice.
// DirectShow is used for capturing. DirectShow provide its own threads
// for capturing.

#ifndef MEDIA_VIDEO_CAPTURE_WIN_VIDEO_CAPTURE_DEVICE_MF_WIN_H_
#define MEDIA_VIDEO_CAPTURE_WIN_VIDEO_CAPTURE_DEVICE_MF_WIN_H_

#include <mfidl.h>
#include <mfreadwrite.h>

#include <vector>

#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "base/win/scoped_comptr.h"
#include "media/video/capture/video_capture_device.h"

interface IMFSourceReader;

namespace media {

class MFReaderCallback;

class VideoCaptureDeviceMFWin
    : public base::NonThreadSafe,
      public VideoCaptureDevice {
 public:
  explicit VideoCaptureDeviceMFWin(const Name& device_name);
  virtual ~VideoCaptureDeviceMFWin();

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

  // Returns true iff the current platform supports the Media Foundation API
  // and that the DLLs are available.  On Vista this API is an optional download
  // but the API is advertised as a part of Windows 7 and onwards.  However,
  // we've seen that the required DLLs are not available in some Win7
  // distributions such as Windows 7 N and Windows 7 KN.
  static bool PlatformSupported();

  static void GetDeviceNames(Names* device_names);

  // Captured a new video frame.
  void OnIncomingCapturedFrame(const uint8* data, int length,
                               const base::Time& time_stamp);

 private:
  void OnError(HRESULT hr);

  Name name_;
  base::win::ScopedComPtr<IMFActivate> device_;
  scoped_refptr<MFReaderCallback> callback_;

  base::Lock lock_;  // Used to guard the below variables.
  VideoCaptureDevice::EventHandler* observer_;
  base::win::ScopedComPtr<IMFSourceReader> reader_;
  bool capture_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureDeviceMFWin);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_WIN_VIDEO_CAPTURE_DEVICE_MF_WIN_H_
