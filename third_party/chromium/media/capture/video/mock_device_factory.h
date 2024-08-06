// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_MOCK_DEVICE_FACTORY_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_MOCK_DEVICE_FACTORY_H_

#include <map>

#include "third_party/chromium/media/capture/video/video_capture_device_factory.h"

namespace media_m96 {

// Implementation of media_m96::VideoCaptureDeviceFactory that allows clients to
// add mock devices.
class MockDeviceFactory : public media_m96::VideoCaptureDeviceFactory {
 public:
  MockDeviceFactory();
  ~MockDeviceFactory() override;

  void AddMockDevice(media_m96::VideoCaptureDevice* device,
                     const media_m96::VideoCaptureDeviceDescriptor& descriptor);
  void RemoveAllDevices();

  // media_m96::VideoCaptureDeviceFactory implementation.
  std::unique_ptr<media_m96::VideoCaptureDevice> CreateDevice(
      const media_m96::VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDevicesInfo(GetDevicesInfoCallback callback) override;

 private:
  std::map<media_m96::VideoCaptureDeviceDescriptor, media_m96::VideoCaptureDevice*>
      devices_;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_MOCK_DEVICE_FACTORY_H_
