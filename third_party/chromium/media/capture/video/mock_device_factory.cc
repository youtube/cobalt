// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/capture/video/mock_device_factory.h"

#include <utility>

namespace {

// Report a single hard-coded supported format to clients.
media_m96::VideoCaptureFormat kSupportedFormat(gfx::Size(640, 480),
                                           25.0f,
                                           media_m96::PIXEL_FORMAT_I420);

// Wraps a raw pointer to a media_m96::VideoCaptureDevice and allows us to
// create a std::unique_ptr<media_m96::VideoCaptureDevice> that delegates to it.
class RawPointerVideoCaptureDevice : public media_m96::VideoCaptureDevice {
 public:
  explicit RawPointerVideoCaptureDevice(media_m96::VideoCaptureDevice* device)
      : device_(device) {}

  // media_m96::VideoCaptureDevice:
  void AllocateAndStart(const media_m96::VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override {
    device_->AllocateAndStart(params, std::move(client));
  }
  void RequestRefreshFrame() override { device_->RequestRefreshFrame(); }
  void StopAndDeAllocate() override { device_->StopAndDeAllocate(); }
  void GetPhotoState(GetPhotoStateCallback callback) override {
    device_->GetPhotoState(std::move(callback));
  }
  void SetPhotoOptions(media_m96::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override {
    device_->SetPhotoOptions(std::move(settings), std::move(callback));
  }
  void TakePhoto(TakePhotoCallback callback) override {
    device_->TakePhoto(std::move(callback));
  }
  void OnUtilizationReport(int frame_feedback_id,
                           media_m96::VideoCaptureFeedback feedback) override {
    device_->OnUtilizationReport(frame_feedback_id, feedback);
  }

 private:
  media_m96::VideoCaptureDevice* device_;
};

}  // anonymous namespace

namespace media_m96 {

MockDeviceFactory::MockDeviceFactory() = default;

MockDeviceFactory::~MockDeviceFactory() = default;

void MockDeviceFactory::AddMockDevice(
    media_m96::VideoCaptureDevice* device,
    const media_m96::VideoCaptureDeviceDescriptor& descriptor) {
  devices_[descriptor] = device;
}

void MockDeviceFactory::RemoveAllDevices() {
  devices_.clear();
}

std::unique_ptr<media_m96::VideoCaptureDevice> MockDeviceFactory::CreateDevice(
    const media_m96::VideoCaptureDeviceDescriptor& device_descriptor) {
  if (devices_.find(device_descriptor) == devices_.end())
    return nullptr;
  return std::make_unique<RawPointerVideoCaptureDevice>(
      devices_[device_descriptor]);
}

void MockDeviceFactory::GetDevicesInfo(GetDevicesInfoCallback callback) {
  std::vector<media_m96::VideoCaptureDeviceInfo> result;
  for (const auto& entry : devices_) {
    result.emplace_back(entry.first);
    result.back().supported_formats.push_back(kSupportedFormat);
  }

  std::move(callback).Run(std::move(result));
}

}  // namespace media_m96
