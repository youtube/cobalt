// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/xr_integration_client.h"

#include "build/build_config.h"
#include "content/public/browser/xr_install_helper.h"
#include "device/vr/public/cpp/vr_device_provider.h"
#include "device/vr/public/mojom/vr_service.mojom-shared.h"

namespace content {

std::unique_ptr<XrInstallHelper> XrIntegrationClient::GetInstallHelper(
    device::mojom::XRDeviceId device_id) {
  return nullptr;
}

std::unique_ptr<BrowserXRRuntime::Observer>
XrIntegrationClient::CreateRuntimeObserver() {
  return nullptr;
}

XRProviderList XrIntegrationClient::GetAdditionalProviders() {
  return {};
}

#if !BUILDFLAG(IS_ANDROID)
std::unique_ptr<VrUiHost> XrIntegrationClient::CreateVrUiHost(
    device::mojom::XRDeviceId device_id,
    mojo::PendingRemote<device::mojom::XRCompositorHost> compositor) {
  return nullptr;
}
#endif

}  // namespace content
