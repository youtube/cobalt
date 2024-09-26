// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_XR_SERVICE_ISOLATED_DEVICE_PROVIDER_H_
#define CONTENT_BROWSER_XR_SERVICE_ISOLATED_DEVICE_PROVIDER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "device/vr/public/cpp/vr_device_provider.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class VrUiHost;
}

namespace content {

class IsolatedVRDeviceProvider
    : public device::VRDeviceProvider,
      public device::mojom::IsolatedXRRuntimeProviderClient {
 public:
  IsolatedVRDeviceProvider();
  ~IsolatedVRDeviceProvider() override;

  // If the VR API requires initialization that should happen here.
  void Initialize(device::VRDeviceProviderClient* client) override;

  // Returns true if initialization is complete.
  bool Initialized() override;

 private:
  // IsolatedXRRuntimeProviderClient
  void OnDeviceAdded(
      mojo::PendingRemote<device::mojom::XRRuntime> device,
      mojo::PendingRemote<device::mojom::XRCompositorHost> compositor_host,
      device::mojom::XRDeviceDataPtr device_data,
      device::mojom::XRDeviceId device_id) override;
  void OnDeviceRemoved(device::mojom::XRDeviceId id) override;
  void OnDevicesEnumerated() override;
  void OnServerError();
  void SetupDeviceProvider();

  bool initialized_ = false;
  int retry_count_ = 0;
  mojo::Remote<device::mojom::IsolatedXRRuntimeProvider> device_provider_;

  raw_ptr<device::VRDeviceProviderClient> client_ = nullptr;
  mojo::Receiver<device::mojom::IsolatedXRRuntimeProviderClient> receiver_{
      this};

  using UiHostMap = base::flat_map<device::mojom::XRDeviceId,
                                   std::unique_ptr<content::VrUiHost>>;
  UiHostMap ui_host_map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_XR_SERVICE_ISOLATED_DEVICE_PROVIDER_H_
