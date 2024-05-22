// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_app_device_provider_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/task/bind_post_task.h"

namespace media {

CameraAppDeviceProviderImpl::CameraAppDeviceProviderImpl(
    mojo::PendingRemote<cros::mojom::CameraAppDeviceBridge> bridge,
    DeviceIdMappingCallback mapping_callback)
    : bridge_(std::move(bridge)),
      mapping_callback_(std::move(mapping_callback)),
      weak_ptr_factory_(this) {}

CameraAppDeviceProviderImpl::~CameraAppDeviceProviderImpl() = default;

void CameraAppDeviceProviderImpl::Bind(
    mojo::PendingReceiver<cros::mojom::CameraAppDeviceProvider> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void CameraAppDeviceProviderImpl::GetCameraAppDevice(
    const std::string& source_id,
    GetCameraAppDeviceCallback callback) {
  mapping_callback_.Run(
      source_id,
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &CameraAppDeviceProviderImpl::GetCameraAppDeviceWithDeviceId,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void CameraAppDeviceProviderImpl::GetCameraAppDeviceWithDeviceId(
    GetCameraAppDeviceCallback callback,
    const absl::optional<std::string>& device_id) {
  if (!device_id.has_value()) {
    std::move(callback).Run(
        cros::mojom::GetCameraAppDeviceStatus::ERROR_INVALID_ID,
        mojo::NullRemote());
    return;
  }

  bridge_->GetCameraAppDevice(*device_id, std::move(callback));
}

void CameraAppDeviceProviderImpl::IsSupported(IsSupportedCallback callback) {
  bridge_->IsSupported(std::move(callback));
}

void CameraAppDeviceProviderImpl::SetVirtualDeviceEnabled(
    const std::string& source_id,
    bool enabled,
    SetVirtualDeviceEnabledCallback callback) {
  mapping_callback_.Run(
      source_id,
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &CameraAppDeviceProviderImpl::SetVirtualDeviceEnabledWithDeviceId,
          weak_ptr_factory_.GetWeakPtr(), enabled, std::move(callback))));
}

void CameraAppDeviceProviderImpl::SetVirtualDeviceEnabledWithDeviceId(
    bool enabled,
    SetVirtualDeviceEnabledCallback callback,
    const absl::optional<std::string>& device_id) {
  if (!device_id.has_value()) {
    std::move(callback).Run(false);
    return;
  }

  bridge_->SetVirtualDeviceEnabled(*device_id, enabled, std::move(callback));
}

}  // namespace media
