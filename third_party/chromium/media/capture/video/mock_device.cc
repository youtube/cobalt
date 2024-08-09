// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/capture/video/mock_device.h"

namespace media_m96 {

MockDevice::MockDevice() = default;

MockDevice::~MockDevice() = default;

void MockDevice::SendStubFrame(const media_m96::VideoCaptureFormat& format,
                               int rotation,
                               int frame_feedback_id) {
  auto stub_frame = media_m96::VideoFrame::CreateZeroInitializedFrame(
      format.pixel_format, format.frame_size,
      gfx::Rect(format.frame_size.width(), format.frame_size.height()),
      format.frame_size, base::TimeDelta());
  client_->OnIncomingCapturedData(
      stub_frame->data(0),
      static_cast<int>(media_m96::VideoFrame::AllocationSize(
          stub_frame->format(), stub_frame->coded_size())),
      format, gfx::ColorSpace(), rotation, false /* flip_y */,
      base::TimeTicks(), base::TimeDelta(), frame_feedback_id);
}

void MockDevice::SendOnStarted() {
  client_->OnStarted();
}

void MockDevice::AllocateAndStart(const media_m96::VideoCaptureParams& params,
                                  std::unique_ptr<Client> client) {
  client_ = std::move(client);
  DoAllocateAndStart(params, &client_);
}

void MockDevice::StopAndDeAllocate() {
  DoStopAndDeAllocate();
  client_.reset();
}

void MockDevice::GetPhotoState(GetPhotoStateCallback callback) {
  DoGetPhotoState(&callback);
}

void MockDevice::SetPhotoOptions(media_m96::mojom::PhotoSettingsPtr settings,
                                 SetPhotoOptionsCallback callback) {
  DoSetPhotoOptions(&settings, &callback);
}

void MockDevice::TakePhoto(TakePhotoCallback callback) {
  DoTakePhoto(&callback);
}

}  // namespace media_m96
