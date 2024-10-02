// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/lacros/device_factory_adapter_lacros.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/video_capture/lacros/device_proxy_lacros.h"

namespace video_capture {

DeviceFactoryAdapterLacros::DeviceFactoryAdapterLacros(
    mojo::PendingRemote<crosapi::mojom::VideoCaptureDeviceFactory>
        device_factory_ash)
    : device_factory_ash_(std::move(device_factory_ash)) {}

DeviceFactoryAdapterLacros::~DeviceFactoryAdapterLacros() = default;

void DeviceFactoryAdapterLacros::GetDeviceInfos(
    GetDeviceInfosCallback callback) {
  DCHECK(device_factory_ash_.is_bound());
  device_factory_ash_->GetDeviceInfos(std::move(callback));
}

void DeviceFactoryAdapterLacros::CreateDevice(const std::string& device_id,
                                              CreateDeviceCallback callback) {
  DCHECK(device_factory_ash_.is_bound());
  mojo::PendingRemote<crosapi::mojom::VideoCaptureDevice> proxy_remote;
  auto proxy_receiver = proxy_remote.InitWithNewPipeAndPassReceiver();
  // Since |device_proxy| is owned by this instance and the cleanup callback
  // is only called within the lifetime of |device_proxy|, it should be safe
  // to use base::Unretained(this) here.
  auto device_proxy = std::make_unique<DeviceProxyLacros>(
      absl::nullopt, std::move(proxy_remote),
      base::BindOnce(
          &DeviceFactoryAdapterLacros::OnClientConnectionErrorOrClose,
          base::Unretained(this), device_id));

  auto wrapped_callback = base::BindOnce(
      [](CreateDeviceCallback callback, video_capture::Device* device,
         crosapi::mojom::DeviceAccessResultCode code) {
        media::VideoCaptureError video_capture_result_code;
        switch (code) {
          case crosapi::mojom::DeviceAccessResultCode::NOT_INITIALIZED:
            video_capture_result_code = media::VideoCaptureError::
                kCrosHalV3DeviceDelegateFailedToInitializeCameraDevice;
            break;
          case crosapi::mojom::DeviceAccessResultCode::SUCCESS:
            video_capture_result_code = media::VideoCaptureError::kNone;
            break;
          case crosapi::mojom::DeviceAccessResultCode::ERROR_DEVICE_NOT_FOUND:
            video_capture_result_code = media::VideoCaptureError::
                kServiceDeviceLauncherServiceRespondedWithDeviceNotFound;
            break;
          default:
            NOTREACHED() << "Unexpected device access result code";
        }

        DCHECK(callback);
        DCHECK(device);
        DeviceInfo info{device, media::VideoCaptureError::kNone};
        info.result_code = video_capture_result_code;
        std::move(callback).Run(std::move(info));
      },
      std::move(callback), device_proxy.get());

  devices_.emplace(device_id, std::move(device_proxy));
  device_factory_ash_->CreateDevice(device_id, std::move(proxy_receiver),
                                    std::move(wrapped_callback));
}

void DeviceFactoryAdapterLacros::StopDevice(const std::string device_id) {
  OnClientConnectionErrorOrClose(device_id);
}

void DeviceFactoryAdapterLacros::AddSharedMemoryVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingRemote<mojom::Producer> producer,
    mojo::PendingReceiver<mojom::SharedMemoryVirtualDevice>
        virtual_device_receiver) {
  NOTREACHED();
}

void DeviceFactoryAdapterLacros::AddTextureVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingReceiver<mojom::TextureVirtualDevice>
        virtual_device_receiver) {
  NOTREACHED();
}

void DeviceFactoryAdapterLacros::AddGpuMemoryBufferVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingReceiver<mojom::GpuMemoryBufferVirtualDevice>
        virtual_device_receiver) {
  NOTREACHED();
}

void DeviceFactoryAdapterLacros::RegisterVirtualDevicesChangedObserver(
    mojo::PendingRemote<mojom::DevicesChangedObserver> observer,
    bool raise_event_if_virtual_devices_already_present) {
  NOTREACHED();
}

void DeviceFactoryAdapterLacros::OnClientConnectionErrorOrClose(
    std::string device_id) {
  devices_.erase(device_id);
}

}  // namespace video_capture
