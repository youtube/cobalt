// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_LINUX_FAKE_DEVICE_PROVIDER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_LINUX_FAKE_DEVICE_PROVIDER_H_

#include <string>
#include <vector>

#include "third_party/chromium/media/capture/video/linux/video_capture_device_factory_linux.h"
#include "third_party/chromium/media/capture/video/video_capture_device_descriptor.h"
#include "third_party/chromium/media/capture/video_capture_types.h"

namespace media_m96 {

class FakeDeviceProvider
    : public VideoCaptureDeviceFactoryLinux::DeviceProvider {
 public:
  FakeDeviceProvider();
  ~FakeDeviceProvider() override;

  void AddDevice(const VideoCaptureDeviceDescriptor& descriptor);

  void GetDeviceIds(std::vector<std::string>* target_container) override;
  std::string GetDeviceModelId(const std::string& device_id) override;
  std::string GetDeviceDisplayName(const std::string& device_id) override;

 private:
  std::vector<VideoCaptureDeviceDescriptor> descriptors_;
};
}  // namespace media_m96
#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_LINUX_FAKE_DEVICE_PROVIDER_H_
